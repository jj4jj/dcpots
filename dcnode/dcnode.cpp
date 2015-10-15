#include "dcnode.h"
#include "libmq.h"
#include "libtcp.h"
#include "error_msg.h"

struct dcnode_t
{
	enum {
		DCNODE_INIT = 0,
		DCNODE_CONNECTING = 1, //reconnect
		DCNODE_CONNECTED = 2,
		DCNODE_NAME_REG_ING = 3,
		DCNODE_NAME_REG = 4,
		DCNODE_READY = 5
	};
	dcnode_config_t conf;
	stcp_t	*	stcp;
	int			parentfd;
	smq_t	*	smq;	//mq
	int			state;
	time_t		next_stat_time;

	//local name map addr[id]
	dcnode_dispatcher_t		dispatcher;
	void *					dispatcher_ud;

	//timer cb
	std::multimap<uint64_t, uint64_t>						expiring_callbacks;
	std::unordered_map<uint64_t, dcnode_timer_callback_t>	timer_callbacks;
	std::unordered_map<uint64_t, uint64_t>					callback_periods;

	//smq children (agent)
	std::unordered_map<string, uint64_t>				named_smqid;
	std::unordered_map<uint64_t, string>				smqid_map_name;

	//tcp children (agent)
	std::unordered_map<string, int>						named_tcpfd;
	std::unordered_map<int, string>						tcpfd_map_name;
	//
	msg_buffer_t										send_buffer;
	error_msg_t							*				error_msg;
	//heart beat info
	time_t												parent_hb_time;
	std::unordered_map<uint64_t, time_t>				smq_last_hb_time;
	std::unordered_map<int, time_t>						tcp_last_hb_time;

	dcnode_t()
	{
		init();
	}
	void init()
	{
		smq = nullptr;
		stcp = nullptr;
		state = DCNODE_INIT;
		dispatcher = nullptr;
		timer_callbacks.clear();
		callback_periods.clear();
		expiring_callbacks.clear();
		named_smqid.clear();
		smqid_map_name.clear();
		named_tcpfd.clear();
		tcpfd_map_name.clear();
		smq_last_hb_time.clear();
		tcp_last_hb_time.clear();
		next_stat_time = 0;
		error_msg = nullptr;
		parent_hb_time = 0;
		parentfd = -1;
	}
};

static uint32_t	s_cb_seq = 0;
static inline uint64_t  _insert_timer_callback(dcnode_t * dc , int32_t expired_ms, dcnode_timer_callback_t cb, bool repeat = false) {
	uint64_t cookie = time(NULL);
	cookie <<= 24; //24+32 = 56;
	cookie |= s_cb_seq++;
	cookie <<= 1;
	if (repeat)
		cookie |= 1;
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t expiretimems = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	expiretimems += expired_ms;
	dc->expiring_callbacks.insert(std::make_pair(expiretimems, cookie));
	dc->timer_callbacks[cookie] = cb;
	if (repeat)
		dc->callback_periods.insert(std::make_pair(cookie, expiretimems));

	return cookie;
}
static void	_remove_timer_callback(dcnode_t* dc, uint64_t cookie){
	if (cookie & 1)
	{
		dc->callback_periods.erase(cookie);
	}
	dc->timer_callbacks.erase(cookie);
}

//leaf: no children(no stcp listen)
static bool _is_leaf(dcnode_t* dc){
	return (dc->conf.addr.listen_addr.empty() &&
			dc->conf.addr.parent_addr.empty() );
}
static int	_check_dcnode_fsm(dcnode_t * dc, bool checkforce);
static inline int _change_dcnode_fsm(dcnode_t * dc, int state) {
	//log change 
	dc->state = state;
	return _check_dcnode_fsm(dc, true);
}
static int _register_name(dcnode_t * dc){
	dcnode_msg_t	msg;
	msg.set_type(dcnode::MSG_REG_NAME);
	msg.set_src(dc->conf.name);
	msg.mutable_ext()->set_unixtime(time(NULL));
	static char buffer[1024];
	int sz = sizeof(buffer);
	if (!msg.pack(buffer, sz))
	{
		//pack error
		return -1;
	}
	int ret = 0;
	dc->state = dcnode_t::DCNODE_NAME_REG_ING;
	if (_is_leaf(dc))
	{
		//to agent
		ret = smq_send(dc->smq, getpid(), smq_msg_t(buffer, sz));
	}
	else
	{
		if (dc->conf.addr.parent_addr.empty())
		{
			//no need
			_change_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
			return 0;
		}
		//to parent
		ret = stcp_send(dc->stcp, dc->parentfd, stcp_msg_t(buffer, sz));
	}
	if (ret)
	{
		//send err
		return ret;
	}
	return 0;
}
static int _send_to_parent(dcnode_t * dc, dcnode_msg_t & dm) {
	dm.set_src(dc->conf.name);
	dm.set_dst("");
	dm.mutable_ext()->set_unixtime(time(NULL));
	if (!dm.pack(dc->send_buffer)){
		//error pack
		return -1;
	}
	if (_is_leaf(dc)){
		//to agent
		return smq_send(dc->smq, getpid(), smq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else if (!dc->conf.addr.parent_addr.empty())
	{
		return stcp_send(dc->stcp, dc->parentfd, stcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	return 0;
}
static  int _connect_parent(dcnode_t * dc){
	if (dc->conf.addr.parent_addr.empty())
	{
		_change_dcnode_fsm(dc, dcnode_t::DCNODE_CONNECTED);
		return 0;
	}
	dc->state = dcnode_t::DCNODE_CONNECTING;
	stcp_addr_t saddr;
	saddr.ip = dc->conf.addr.parent_addr.substr(0, dc->conf.addr.parent_addr.find(':'));
	saddr.port = strtol(dc->conf.addr.parent_addr.substr(dc->conf.addr.parent_addr.find(':')+1).c_str(),NULL,10);
	return stcp_connect(dc->stcp, saddr);
}

static int _stcp_sockfd_close(dcnode_t * dc, int fd)
{
	auto it = dc->tcpfd_map_name.find(fd);
	if (it != dc->tcpfd_map_name.end())
	{
		dc->named_tcpfd.erase(it->second);
		dc->tcpfd_map_name.erase(it);
	}
	if (fd == dc->parentfd)
	{
		dc->parentfd = -1;
		return _change_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
	dc->tcp_last_hb_time.erase(fd);
	return 0;
}

static void _update_hearbeat_timer(dcnode_t * dc, int sockfd, uint64_t msgpid){
	//response msg
	if (_is_leaf(dc)){
		dc->parent_hb_time = time(NULL);
	}
	else if (msgpid > 0) {
		dc->smq_last_hb_time[msgpid] = time(NULL);
	}
	else if (sockfd == dc->parentfd){
		dc->parent_hb_time = time(NULL);
	}
	else {
		dc->tcp_last_hb_time[sockfd] = time(NULL);
	}
}
static void _node_expired(dcnode_t * dc, int sockfd = -1, uint64_t msgpid = 0){
	//erorr log
	if (_is_leaf(dc)){
		_change_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
	else if (msgpid > 0) {
		//how ? update time
	}
	else if (sockfd == dc->parentfd){
		_change_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
		stcp_close(dc->stcp, dc->parentfd);
	}
	else {
		stcp_close(dc->stcp, dc->parentfd);
	}
}

static void _start_heart_beat_timer(dcnode_t * dc){
	//typedef	std::function<void()>	dcnode_timer_callback_t;
	dcnode_timer_callback_t hb = [dc](){
		dcnode_msg_t dm;
		dm.set_type(dcnode::MSG_HEART_BEAT);
		_send_to_parent(dc, dm);		
	};
	dc->parent_hb_time = time(NULL);
	_insert_timer_callback(dc, dc->conf.heart_beat_gap * 1000, hb, true);
	//start check all children timer
	dcnode_timer_callback_t hb_cheker = [dc](){
		time_t tNow = time(NULL);
		if (dc->parent_hb_time < tNow){
			_node_expired(dc, -1, 0);
		}
		for (auto it : dc->smq_last_hb_time){
			if (it.second < tNow){
				_node_expired(dc, -1, it.first);
			}
		}
		for (auto it : dc->tcp_last_hb_time){
			if (it.second < tNow){
				_node_expired(dc, it.first);
			}
		}
	};
	_insert_timer_callback(dc, 1000 * dc->conf.max_live_heart_beat_gap, hb_cheker, true);
}
static int	_check_dcnode_fsm(dcnode_t * dc, bool checkforce){
	time_t tNow = time(NULL);
	if (!checkforce && dc->next_stat_time > tNow)
	{
		return 0;
	}
	int timeout_time = 1;
	switch (dc->state)
	{
	case dcnode_t::DCNODE_INIT:
		_connect_parent(dc);
		timeout_time = 2;//2s
		break;
	case dcnode_t::DCNODE_CONNECTED:
		_register_name(dc);
		timeout_time = 2;//2s
		break;
	case dcnode_t::DCNODE_NAME_REG:
		dc->state = dcnode_t::DCNODE_READY;
		//add timer hb
		_start_heart_beat_timer(dc);		
	case dcnode_t::DCNODE_READY:
		//nothing report parent
		timeout_time = 30;//20s
		break;
	default: return -1;
	}
	dc->next_stat_time = tNow + timeout_time;
	return 0;
}
static void _update_peer_name(dcnode_t * dc, int sockfd, uint64_t smqid, const string & name){
	if (sockfd != -1)
	{
		//children
		auto it = dc->named_tcpfd.find(name);
		if (it != dc->named_tcpfd.end())
		{
			dc->tcpfd_map_name.erase(it->second);
			dc->tcp_last_hb_time.erase(it->second);
			dc->named_tcpfd.erase(it);
		}
		dc->named_tcpfd[name] = sockfd;
		dc->tcpfd_map_name[sockfd] = name;
	}
	else // if (smqid > 0)
	{
		assert(smqid > 0);
		auto it = dc->named_smqid.find(name);
		if (it != dc->named_smqid.end())
		{
			dc->smqid_map_name.erase(it->second);
			dc->smq_last_hb_time.erase(it->second);
			dc->named_smqid.erase(it);
		}
		dc->named_smqid[name] = smqid;
		dc->smqid_map_name[smqid] = name;
	}
}
static int _response_msg(dcnode_t * dc, int sockfd, uint64_t msgqpid, dcnode_msg_t & dm, const dcnode_msg_t & dmsrc) {
	dm.set_src(dc->conf.name);
	dm.set_dst(dmsrc.src());
	dm.mutable_ext()->set_unixtime(time(NULL));
	bool ret = dm.pack(dc->send_buffer);
	if (!ret)
	{
		//error pack
		return -1;
	}
	if (sockfd != -1)
	{
		return stcp_send(dc->stcp, sockfd, stcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else
	{
		assert(msgqpid > 0);
		return smq_send(dc->smq, msgqpid, smq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
}
static int _handle_msg(dcnode_t * dc, const dcnode_msg_t & dm, int sockfd, uint64_t msgqpid){
	//to me
	LOGP("hanlde msg size:%d %s", dm.ByteSize(), dm.debug());
	if (dm.ext().unixtime() > 0 && dm.ext().unixtime() + dc->conf.max_expired_time < time(NULL) ) {
		//expired msg
		return -1;
	}
	dcnode_msg_t rspmsg;
	switch (dm.type())
	{
	case dcnode::MSG_DATA:
		//to up callback 
		return dc->dispatcher(dc->dispatcher_ud, dm);
	case dcnode::MSG_REG_NAME:
		//insert tcp src -> map name
		if (dc->state == dcnode_t::DCNODE_NAME_REG_ING)
		{
			//got parent name
			_update_peer_name(dc, sockfd, msgqpid, dm.src());
			return _change_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
		}
		else
		{
			//response no stat change - ready
			_update_peer_name(dc, sockfd, msgqpid, dm.src());
			//response msg get children name
			rspmsg.set_type(dcnode::MSG_REG_NAME);
			_response_msg(dc, sockfd, msgqpid, rspmsg, dm);
		}
		break;
	case dcnode::MSG_HEART_BEAT:
		_update_hearbeat_timer(dc, sockfd, msgqpid);
		rspmsg.set_type(dcnode::MSG_REG_NAME);
		_response_msg(dc, sockfd, msgqpid, rspmsg, dm);
		break;
	case dcnode::MSG_RPC:
		break;
	default:
		//error type
		return -1;
		break;
	}
	return 0;
}

static int _forward_msg(dcnode_t * dc, int sockfd, const char * buff, int buff_sz, const string & dst) {
	if (_is_leaf(dc))
	{
		//to msgq
		return smq_send(dc->smq, getpid(), smq_msg_t(buff, buff_sz));
	}

	//check children name , if not found , send to parent except src
	auto it = dc->named_smqid.find(dst);
	if (it != dc->named_smqid.end())
	{
		return smq_send(dc->smq, it->second, smq_msg_t(buff, buff_sz));
	}
	auto tit = dc->named_tcpfd.find(dst);
	if (tit != dc->named_tcpfd.end())
	{
		return stcp_send(dc->stcp, tit->second, stcp_msg_t(buff, buff_sz));
	}

	//to up layer 
	if (sockfd != dc->parentfd)
	{
		stcp_send(dc->stcp, dc->parentfd, stcp_msg_t(buff, buff_sz));
	}
	//all children
	for (auto it = dc->named_tcpfd.begin();
		it != dc->named_tcpfd.end(); it++)
	{
		if (it->second == sockfd ||
			it->second == dc->parentfd)
		{
			continue;
		}
		stcp_send(dc->stcp, it->second, stcp_msg_t(buff, buff_sz));
	}
	return 0;
}
static int _msg_cb(dcnode_t * dc, int sockfd, uint64_t msgqpid, const char * buff, int buff_sz){
	dcnode_msg_t dm;
	if (!dm.unpack(buff, buff_sz)) {
		//error for decode
		return -1;
	}
	if (dm.dst().length() == 0 ||
		dm.dst() == dc->conf.name) {
		//to me
		return _handle_msg(dc, dm, sockfd, msgqpid);
	}
	else {
		return _forward_msg(dc, sockfd, buff, buff_sz, dm.dst());
	}
}
static int _smq_cb(smq_t * smq, uint64_t src, const smq_msg_t & msg, void * ud) {
	dcnode_t * dc = (dcnode_t*)ud;
	return _msg_cb(dc, -1, src, msg.buffer, msg.sz);
}

//server
static int _stcp_cb(stcp_t* server, const stcp_event_t & ev, void * ud) {
	dcnode_t * dc = (dcnode_t*)ud;

	switch (ev.type)
	{
	case stcp_event_type::STCP_NEW_CONNX:
		//new connection no need build the map , but in reg name
		break;
	case stcp_event_type::STCP_CONNECTED:
		//connected
		dc->parentfd = ev.fd;
		return _change_dcnode_fsm(dc, dcnode_t::DCNODE_CONNECTED);
	case stcp_event_type::STCP_READ:
		//
		return _msg_cb(dc, ev.fd, 0, ev.msg->buff, ev.msg->buff_sz);
	case stcp_event_type::STCP_CLOSED:
		//error record
		return _stcp_sockfd_close(dc, ev.fd);
	default: return -1;
	}
	return 0;
}

dcnode_t* dcnode_create(const dcnode_config_t & conf) {
	dcnode_t * n = new dcnode_t();
	if (!n)
		return nullptr;
	n->conf = conf;
	stcp_config_t sc;
	sc.max_recv_buff = conf.max_channel_buff_size;
	sc.max_send_buff = conf.max_channel_buff_size;
	sc.max_tcp_send_buff_size = conf.max_channel_buff_size;
	sc.max_tcp_recv_buff_size = conf.max_channel_buff_size;

	if (!_is_leaf(n))
	{
		if (conf.addr.listen_addr.length())
		{
			sc.is_server = true;
			stcp_addr_t saddr;
			const string & listenaddr = conf.addr.listen_addr;
			saddr.ip = listenaddr.substr(0, listenaddr.find(':'));
			saddr.port = strtol(listenaddr.substr(listenaddr.find(':') + 1).c_str(), nullptr, 10);
			sc.listen_addr = saddr;
		}
		n->stcp = stcp_create(sc);
		if (!n->stcp) {
			dcnode_destroy(n); return nullptr;
		}
		stcp_event_cb(n->stcp, _stcp_cb, n);
	}
	if (conf.addr.msgq_key.length())
	{
		smq_config_t smc;
		smc.key = conf.addr.msgq_key;
		smc.msg_buffsz = conf.max_channel_buff_size;
		smc.is_server = !_is_leaf(n);

		n->smq = smq_create(smc);
		if (!n->smq) {
			dcnode_destroy(n);
			return nullptr;
		}
		smq_msg_cb(n->smq, _smq_cb, n);
	}
	n->send_buffer.create(conf.max_channel_buff_size);
	n->error_msg = error_create();
	return n;
}
void      dcnode_destroy(dcnode_t* dc){
	if (dc->stcp)
	{
		stcp_destroy(dc->stcp);
		dc->stcp = nullptr;
	}
	if (dc->smq)
	{
		smq_destroy(dc->smq);
		dc->smq = nullptr;
	}
	if (dc->error_msg)
	{
		error_destroy(dc->error_msg);
		dc->error_msg = nullptr;
	}
	dc->init();
	delete dc;
}
static	void _check_timer_callback(dcnode_t * dc){
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t currentms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	auto itend = dc->expiring_callbacks.upper_bound(currentms);	
	std::vector<std::pair<uint64_t, uint64_t> >		addagin;
	for (auto it = dc->expiring_callbacks.begin(); it != itend;)
	{
		auto previt = it++;
		uint64_t cookie = previt->second;
		auto funcit = dc->timer_callbacks.find(cookie);
		if (funcit != dc->timer_callbacks.end())
		{
			funcit->second();//call back
			if (cookie & 1)
			{
				//repeat
				auto periodit = dc->callback_periods.find(cookie);
				if (periodit != dc->callback_periods.end())
				{
					//add agin
					addagin.push_back(std::make_pair(currentms + periodit->second, cookie));
				}
			}
			else
			{
				dc->timer_callbacks.erase(funcit);
			}
		}
		dc->expiring_callbacks.erase(previt);
	}
	for (auto pair : addagin)
	{
		dc->expiring_callbacks.insert(pair);
	}
}
void      dcnode_update(dcnode_t* dc, int timout_us) {
	//check cb
	_check_timer_callback(dc);

	_check_dcnode_fsm(dc, false);

	if (dc->stcp) {
		stcp_poll(dc->stcp, timout_us);
	}
	if (dc->smq) {
		smq_poll(dc->smq, timout_us);
	}
	_check_timer_callback(dc);
}

//typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t* dc, dcnode_dispatcher_t dspatch, void * ud) {
	dc->dispatcher = dspatch;
	dc->dispatcher_ud = ud;
}
inline uint64_t  dcnode_timer_add(dcnode_t * dc, int delayms, dcnode_timer_callback_t cb, bool repeat) {
	return	_insert_timer_callback(dc, delayms, cb, repeat);
}
void	  dcnode_timer_cancel(dcnode_t * dc, uint64_t cookie) {
	_remove_timer_callback(dc, cookie);
}
static  bool _name_exists(dcnode_t * dc, const char * name)
{
	if (dc->named_smqid.find(name) != dc->named_smqid.end()){
		return true;
	}
	if (dc->named_tcpfd.find(name) != dc->named_tcpfd.end()){
		return true;
	}
	return false;
}

int      dcnode_send(dcnode_t* dc, const char * dst, const char * buff, int sz)
{
	if (dc->state != dcnode_t::DCNODE_READY &&
		!_name_exists(dc, dst)) {
		//error not ready (name not reg) 
		return -1;
	}
	dcnode_msg_t dm;
	dm.set_src(dc->conf.name);
	dm.set_dst(dst);
	dm.set_type(dcnode::MSG_DATA);
	dm.set_msg_data(buff, sz);	
	dm.mutable_ext()->set_unixtime(time(NULL));
	if (!dm.pack(dc->send_buffer)){
		//error
		return -1;
	}
	//dc, tcp_src,sockfd, buff, buff_sz, dm.dst()
	return _forward_msg(dc, -1, dc->send_buffer.buffer, dc->send_buffer.valid_size, string(dst));
}
error_msg_t * dcnode_error(dcnode_t * dc){
	return dc->error_msg;
}

