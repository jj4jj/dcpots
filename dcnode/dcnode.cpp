#include "dcnode.h"
#include "proto/dcnode.pb.h"
#include "base/msg_proto.hpp"
#include "base/dcsmq.h"
#include "base/dctcp.h"
#include "base/logger.h"
#include "base/utility.hpp"
#include "base/dcshm.h"

typedef msgproto_t<dcnode::MsgDCNode>	dcnode_msg_t;

struct dcnode_name_map_entry_t {
	char		name[32];
	uint64_t	id;
};

struct dcnode_name_map_t {
	int			nc;
	dcnode_name_map_entry_t	names[DCNODE_MAX_LOCAL_NODES_NUM];
	dcnode_name_map_t(){
		bzero(this, sizeof(*this));
	}
};

struct dcnode_t
{
	enum {
		DCNODE_ERROR = -1,	//ERROR , ABORT
		DCNODE_INIT = 0,
		DCNODE_CONNECTING = 1, //reconnect
		DCNODE_CONNECTED = 2,
		DCNODE_NAME_REG_ING = 3,
		DCNODE_NAME_REG = 4,
		DCNODE_READY = 5,
		DCNODE_ABORT = 6,	//ABORT
	};
	dcnode_config_t conf;
	//tcp node
	dctcp_t	*		stcp;
	int				parentfd;
	//smq node
	dcsmq_t	*		smq;	//mq

	//as a client , fsm
	int			fsm_state;
	int			fsm_error;

	//local name map addr[id]
	dcnode_dispatcher_t		dispatcher;
	void *					dispatcher_ud;

	//timer cb
	std::multimap<uint64_t, uint64_t>						expiring_callbacks;
	std::unordered_map<uint64_t, dcnode_timer_callback_t>	timer_callbacks;
	std::unordered_map<uint64_t, uint64_t>					callback_periods;	//period call back timer

	//smq named nodes (agent)
	std::unordered_map<string, uint64_t>				named_smqid;
	std::unordered_map<uint64_t, time_t>				smq_hb_expire_time;	//children heart beat expire info
	dcnode_name_map_t									* smq_name_mapping_shm;	//shm

	//tcp named children (agent)
	std::unordered_map<string, int>						named_tcpfd;
	std::unordered_map<int, string>						tcpfd_map_name;
	std::unordered_map<int, time_t>						tcp_hb_expire_time;	//children heart beat expire info

	//parent heart beat info
	time_t												parent_hb_expire_time;

	//send buffer
	msg_buffer_t										send_buffer;

	dcnode_t()
	{
		init();
	}
	void init()
	{
		smq = nullptr;
		stcp = nullptr;
		fsm_state = DCNODE_INIT;
		dispatcher = nullptr;
		timer_callbacks.clear();
		callback_periods.clear();
		expiring_callbacks.clear();
		named_smqid.clear();
		named_tcpfd.clear();
		tcpfd_map_name.clear();
		smq_hb_expire_time.clear();
		tcp_hb_expire_time.clear();
		parent_hb_expire_time = 0;
		parentfd = -1;
		smq_name_mapping_shm = nullptr;
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

	uint64_t expiretimems = expired_ms + util::time_unixtime_ms();
	dc->expiring_callbacks.insert(std::make_pair(expiretimems, cookie));
	dc->timer_callbacks[cookie] = cb;
	if (repeat)
		dc->callback_periods.insert(std::make_pair(cookie, expired_ms));
	//LOGP("add callback timer delay:%dms ... cookie:%lu",expired_ms, cookie);
	return cookie;
}
static void	_remove_timer_callback(dcnode_t* dc, uint64_t cookie){
	if (cookie & 1)
	{
		dc->callback_periods.erase(cookie);
	}
	dc->timer_callbacks.erase(cookie);
	//LOGP("remove callback timer ... cookie:%lu", cookie);
}

//leaf: smq client but no any tcp info
static bool _is_leaf(dcnode_t* dc){
	if (dc->conf.addr.listen_addr.empty() &&	//not tcp server
		dc->conf.addr.parent_addr.empty()){
		if (!dc->conf.addr.msgq_path.empty() &&
			!dc->conf.addr.msgq_push)	//but a smq server
		{
			return false;
		}
		return true;
	}
	return false;
}
static bool _is_root(dcnode_t* dc){
	return (dc->conf.addr.parent_addr.empty() &&
			dc->conf.addr.msgq_push == false);
}

static int	_fsm_check(dcnode_t * dc, bool checkforce);
static inline int _switch_dcnode_fsm(dcnode_t * dc, int state) {
	LOGP("switch dcnode fsm %d -> %d ", dc->fsm_state, state);
	dc->fsm_state = state;
	return _fsm_check(dc, true);
}
static int _fsm_register_name(dcnode_t * dc){
	LOGP("register panrent name with:%s", dc->conf.name.c_str());
	if (_is_root(dc)){
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
	}
	dcnode_msg_t	msg;
	msg.set_type(dcnode::MSG_REG_NAME);
	msg.set_src(dc->conf.name);
	msg.mutable_ext()->set_unixtime(time(NULL));
	if (!msg.Pack(dc->send_buffer)) {
		//pack error
		return -1;
	}
	dc->fsm_state = dcnode_t::DCNODE_NAME_REG_ING;
	if (_is_leaf(dc)) {
		//to agent
		return dcsmq_send(dc->smq, dcsmq_session(dc->smq),
			dcsmq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else {
		if (dc->conf.addr.parent_addr.empty()){
			//no need
			return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
		}
		//to parent
		return dctcp_send(dc->stcp, dc->parentfd,
			dctcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	return 0;
}
static int _send_to_parent(dcnode_t * dc, dcnode_msg_t & dm) {
	dm.set_src(dc->conf.name);
	dm.mutable_ext()->set_unixtime(time(NULL));
	if (!dm.Pack(dc->send_buffer)){
		//error pack
		return -1;
	}
	if (_is_leaf(dc)){
		//to agent
		return dcsmq_send(dc->smq, dcsmq_session(dc->smq), dcsmq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else if (!dc->conf.addr.parent_addr.empty() && dc->parentfd != -1)
	{
		return dctcp_send(dc->stcp, dc->parentfd, dctcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	return 0;
}
static  int _fsm_connect_parent(dcnode_t * dc){
	if (dc->conf.addr.parent_addr.empty())
	{
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_CONNECTED);
	}
	dc->fsm_state = dcnode_t::DCNODE_CONNECTING;
	dctcp_addr_t saddr;
	saddr.ip = dc->conf.addr.parent_addr.substr(0, dc->conf.addr.parent_addr.find(':'));
	saddr.port = strtol(dc->conf.addr.parent_addr.substr(dc->conf.addr.parent_addr.find(':')+1).c_str(),NULL,10);
	return dctcp_connect(dc->stcp, saddr);
}
static dcnode_name_map_entry_t * _name_smq_entry_find(dcnode_t * dc, uint64_t session) {
	struct dcnode_name_map_entry_t dnme;
	dnme.id = session;
	typedef dcnode_name_map_entry_t entry_t;
	entry_t * beg = dc->smq_name_mapping_shm->names,
			* end = dc->smq_name_mapping_shm->names + dc->smq_name_mapping_shm->nc;
	auto comp = [](const entry_t & a, const entry_t & b){
		return a.id < b.id;
	};
	auto it = std::lower_bound(beg, end, dnme, comp);
	if (it != end && it->id == session){
		return it;
	}
	return nullptr;
}
static dcnode_name_map_entry_t * _name_smq_entry_find(dcnode_t * dc, const string & name) {
	auto it = dc->named_smqid.find(name);
	if (it != dc->named_smqid.end())
	{
		return _name_smq_entry_find(dc, it->second);
	}
	return nullptr;
}
//must has no name map
static dcnode_name_map_entry_t * _name_smq_entrty_alloc(dcnode_t * dc, const string & name) {
	if (dc->smq_name_mapping_shm->nc >= DCNODE_MAX_LOCAL_NODES_NUM){
		//error for maxx children
		return nullptr;
	}
	//just sorted
	static uint64_t s_smqpid_seq = 0;
	if (s_smqpid_seq == 0){
		s_smqpid_seq = time(NULL);
		s_smqpid_seq <<= 18; //>2^18 not equal getpid()
	}
	s_smqpid_seq++;

	dcnode_name_map_entry_t * entry = &(dc->smq_name_mapping_shm->names[dc->smq_name_mapping_shm->nc]);
	dc->smq_name_mapping_shm->nc++;
	//map
	dc->named_smqid[name] = s_smqpid_seq;
	//entry
	entry->id = s_smqpid_seq;
	strncpy(entry->name, name.c_str(), sizeof(entry->name) - 1);

	return entry;
}
static int _smq_msgpid_close(dcnode_t * dc, uint64_t msgpid){
	//children name -> id map is const table , no changed
	if (msgpid == dcsmq_session(dc->smq)) { //parent
		assert(_is_leaf(dc));
		LOGP("smq parent ndoe closed ....");
		dcsmq_set_session(dc->smq, getpid()); //
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
	else {
		dc->smq_hb_expire_time.erase(msgpid);
		return 0;
	}
}
static int _stcp_sockfd_close(dcnode_t * dc, int fd) {
	auto it = dc->tcpfd_map_name.find(fd);
	if (it != dc->tcpfd_map_name.end())
	{
		dc->named_tcpfd.erase(it->second);
		dc->tcpfd_map_name.erase(it);
	}
	if (fd == dc->parentfd)
	{
		dc->parentfd = -1;
		LOGP("parent ndoe closed ....");
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
	dc->tcp_hb_expire_time.erase(fd);
	return 0;
}

static void _fsm_update_hearbeat_timer(dcnode_t * dc, int sockfd, uint64_t smqsession){
	//LOGP("_update_hearbeat_timer sockfd:%d msgpid:%lu ....", sockfd, smqsession);
	if (_is_leaf(dc)) {
		dc->parent_hb_expire_time = time(NULL) + dc->conf.max_live_heart_beat_gap;
	}
	else if (smqsession > 0) {
		if (dc->conf.addr.msgq_push){
			if (dcsmq_session(dc->smq) == smqsession)
			{
				dc->parent_hb_expire_time = time(NULL) + dc->conf.max_live_heart_beat_gap;
			}
		}
		else{
			if (_name_smq_entry_find(dc, smqsession))
			{
				//named node
				dc->smq_hb_expire_time[smqsession] = time(NULL) + dc->conf.max_live_heart_beat_gap;
			}
		}
	}
	else if (sockfd != -1){
		if (sockfd == dc->parentfd){
			dc->parent_hb_expire_time = time(NULL) + dc->conf.max_live_heart_beat_gap;
		}
		else{
			dc->tcp_hb_expire_time[sockfd] = time(NULL) + dc->conf.max_live_heart_beat_gap;
		}
	}
}
static void _node_expired(dcnode_t * dc, int sockfd , uint64_t msgpid = 0 ){
	//erorr log
	LOGP("node exipred sockfd:%d msgqpid:%lu",sockfd, msgpid);
	if (msgpid > 0){
		_smq_msgpid_close(dc, msgpid);
	}
	else if(sockfd != -1){
		//active close
		dctcp_close(dc->stcp, sockfd);
	}
	else{
		//parent tcp node
		_switch_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
}

static void _fsm_start_heart_beat_timer(dcnode_t * dc){
	dcnode_timer_callback_t hb = [dc](){
		dcnode_msg_t dm;
		dm.set_type(dcnode::MSG_HEART_BEAT);
		dm.mutable_ext()->set_opt(dcnode::MSG_OPT_REQ);
		_send_to_parent(dc, dm);		
	};
	if (dc->conf.heart_beat_gap > 0 && 
		(!dc->conf.addr.parent_addr.empty() ||	//tcp client
		  (!dc->conf.addr.msgq_path.empty() && dc->conf.addr.msgq_push )))		//mq client
	{
		LOGP("add parent heart-beat timer checker with:%ds", dc->conf.heart_beat_gap);
		dc->parent_hb_expire_time = time(NULL) + dc->conf.max_live_heart_beat_gap;
		_insert_timer_callback(dc, dc->conf.heart_beat_gap * 1000, hb, true);
	}
	//start check all children timer
	dcnode_timer_callback_t hb_cheker = [dc](){
		time_t tNow = time(NULL);
		if (dc->parent_hb_expire_time > 0 &&
			dc->parent_hb_expire_time < tNow){
			LOGP("parent node is expired ...");
			_node_expired(dc, dc->parentfd, dcsmq_session(dc->smq));
		}
		vector<uint64_t>	msgq_expired;
		vector<int>			tcp_expired;
		for (auto & it : dc->smq_hb_expire_time){
			if (it.second < tNow){
				msgq_expired.push_back(it.first);
			}
		}
		for (auto & it : dc->tcp_hb_expire_time){
			if (it.second < tNow){
				tcp_expired.push_back(it.first);
			}
		}

		for (auto it : msgq_expired){
			LOGP("smq children expired :%lu...",it);
			_node_expired(dc, -1, it);
		}
		for (auto it : tcp_expired){			
			LOGP("tcp children expired :%d...",it);
			_node_expired(dc, it);
		}

	};
	if (dc->conf.max_live_heart_beat_gap > 0 &&
		dc->conf.max_live_heart_beat_gap > dc->conf.heart_beat_gap){
		LOGP("add children heart-beat timer checker with:%ds", dc->conf.max_live_heart_beat_gap);
		_insert_timer_callback(dc, 1000 * dc->conf.max_live_heart_beat_gap, hb_cheker, true);
	}
}
static void _rebuild_smq_name_map(dcnode_t *dc){
	if (!dc->named_smqid.empty()){
		return;
	}
	dc->named_smqid.clear();
	dcnode_name_map_t * p = dc->smq_name_mapping_shm;
	for (auto i = 0; i < p->nc; i++){
		dc->named_smqid[p->names[i].name] = p->names[i].id;
	}
}
static int _name_smq_maping_shm_create(dcnode_t * dc, bool  owner){
	if (dc->smq_name_mapping_shm || !dc->smq){
		return 0;
	}
	dcshm_config_t shm_conf;
	shm_conf.attach = !owner;
	shm_conf.shm_path = dc->conf.addr.msgq_path;
	if (owner){
		shm_conf.shm_size = sizeof(dcnode_name_map_t);
	}
	else {
		shm_conf.shm_size = 0;
	}
	if (dcshm_create(shm_conf, (void **)&dc->smq_name_mapping_shm, shm_conf.attach)){
		return -1;
	}
	if (shm_conf.attach){	//attached
		_rebuild_smq_name_map(dc);
	}
	return 0;
}
static int	_fsm_check(dcnode_t * dc, bool checkforce){
	switch (dc->fsm_state)
	{
	case dcnode_t::DCNODE_INIT:
		//add timer hb
		return _fsm_connect_parent(dc);
	case dcnode_t::DCNODE_CONNECTED:
		return _fsm_register_name(dc);
	case dcnode_t::DCNODE_NAME_REG:
		if (dc->smq){
			//reg name
			if (_name_smq_maping_shm_create(dc, false)){
				LOGP("attach shm error !");
				dc->fsm_state = dcnode_t::DCNODE_ABORT;
				return -1;
			}
			else {
				_rebuild_smq_name_map(dc);
			}
		}
		dc->fsm_state = dcnode_t::DCNODE_READY;
		LOGP("fsm ready !");
		return 0;
	case dcnode_t::DCNODE_READY:
		//nothing report parent
		return 0;
	default: return -1;
	}
}
static int _update_tcpfd_name(dcnode_t * dc, int sockfd, const string & name, bool force){
	auto it = dc->named_tcpfd.find(name);
	if (it != dc->named_tcpfd.end())
	{
		if (it->second != sockfd) {
			if (force){
				dc->tcpfd_map_name.erase(it->second);
				dc->tcp_hb_expire_time.erase(it->second);
				dc->named_tcpfd.erase(it);
			}
			else{
				LOGP("the request name:%s is collision [%d] ...",name.c_str(), it->second);
				return -1;
			}
		}
	}
	LOGP("update tcp name map %s->%d", name.c_str(), sockfd);
	dc->named_tcpfd[name] = sockfd;
	dc->tcpfd_map_name[sockfd] = name;
	return 0;
}
static int _response_msg(dcnode_t * dc, int sockfd, uint64_t msgqpid, dcnode_msg_t & dm, const dcnode_msg_t & dmsrc) {
	dm.set_src(dc->conf.name);
	dm.set_dst(dmsrc.src());
	dm.set_type(dmsrc.type());
	dm.mutable_ext()->set_unixtime(time(NULL));
	dm.mutable_ext()->set_opt(dcnode::MSG_OPT_RSP);
	LOGP("response msg from:%s dst:%s", dmsrc.src().c_str(), dm.dst().c_str());
	bool ret = dm.Pack(dc->send_buffer);
	if (!ret){
		//error pack
		return -1;
	}
	if (sockfd != -1){
		return dctcp_send(dc->stcp, sockfd, dctcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else{
		assert(msgqpid > 0);
		return dcsmq_send(dc->smq, msgqpid, dcsmq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
}

static int _fsm_abort(dcnode_t * dc, int error){
	dc->fsm_error = error;
	return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_ABORT);
}

static int _fsm_update_name(dcnode_t * dc, int sockfd, uint64_t msgsrcid,const dcnode_msg_t & dm) {
	if (dm.ext().opt() == dcnode::MSG_OPT_REQ){
		//register children name
		int ret = 0;
		dcnode_msg_t dmrsp;
		uint64_t session = 0;
		if (msgsrcid > 0){
			assert(sockfd == -1);
			dcnode_name_map_entry_t * entry = _name_smq_entry_find(dc, dm.src());
			if (!entry){//first , name is available
				entry = _name_smq_entrty_alloc(dc, dm.src());
				if (!entry){ ret = -1; }
				session = entry->id;
			}
			else
			{
				session = entry->id;
				//name is busy , check it expired ?
				if (dm.reg_name().session() != entry->id){ //first time [init register]
					if (dc->smq_hb_expire_time.find(entry->id) != dc->smq_hb_expire_time.end()){
						//collision , alived node
						LOGP("request name:%s session:%lu is collision [%lu] ...",
							dm.src().c_str(),dm.reg_name().session(), entry->id);
						ret = -1;
					}
					else{
						//expired , name available
						dc->smq_hb_expire_time[entry->id] = time(NULL) + dc->conf.max_live_heart_beat_gap;
					}
				}
			}
			if (ret == 0){
				LOGP("update smq name map %s->%lu [%lu]", dm.src().c_str(), session, msgsrcid);
			}
		}
		else {
			assert(sockfd != -1);
			//response to . no stat change - ready
			ret = _update_tcpfd_name(dc, sockfd, dm.src(), dm.reg_name().session() > 0);
		}
		dmrsp.mutable_reg_name()->set_ret(ret);
		dmrsp.mutable_reg_name()->set_session(session);
		return _response_msg(dc, sockfd, msgsrcid, dmrsp, dm);
	}
	else {
		assert(dm.dst() == dc->conf.name);
		if (dm.reg_name().ret() != 0) {
			return _fsm_abort(dc, dm.reg_name().ret());
		}
		else {
			if (msgsrcid > 0){
				assert(sockfd == -1);
				//using sender id
				assert(dm.reg_name().session() > 0);
				dcsmq_set_session(dc->smq, dm.reg_name().session());
			}
			else {
				assert(sockfd != -1);
				_update_tcpfd_name(dc, sockfd, dm.src(), true);
			}
			return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
		}
	}
}

//node handle msg
static int _handle_msg(dcnode_t * dc, const dcnode_msg_t & dm, int sockfd, uint64_t msgqpid){
	if (dm.ext().unixtime() > 0 && dm.ext().unixtime() + dc->conf.max_msg_expired_time < time(NULL) ) {
		//expired msg
		LOGP("expired msg :%s",dm.Debug());
		return -1;
	}
	_fsm_update_hearbeat_timer(dc, sockfd, msgqpid);
	dcnode_msg_t rspmsg;
	switch (dm.type())
	{
	case dcnode::MSG_DATA:
		//to up callback 
		LOGP("dispatch msg :%s->:%s size:%d",dm.src().c_str(), dm.dst().c_str(), dm.PackSize());
		return dc->dispatcher(dc->dispatcher_ud, dm.src().c_str(), msg_buffer_t(dm.msg_data().data(), dm.msg_data().length()));
	case dcnode::MSG_REG_NAME:
		//insert tcp src -> map name
		return _fsm_update_name(dc, sockfd, msgqpid, dm);
	case dcnode::MSG_HEART_BEAT:
		if (dm.ext().opt() == dcnode::MSG_OPT_REQ) {
			_response_msg(dc, sockfd, msgqpid, rspmsg, dm);
		}
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
		LOGP("foward msg to :%s with smq to parent", dst.c_str());
		return dcsmq_send(dc->smq, dcsmq_session(dc->smq), dcsmq_msg_t(buff, buff_sz));
	}

	//check children name , if not found , send to parent except src
	auto it = dc->named_smqid.find(dst);
	if (it != dc->named_smqid.end())
	{
		LOGP("foward msg to :%s with smq [found dst]", dst.c_str());
		return dcsmq_send(dc->smq, it->second, dcsmq_msg_t(buff, buff_sz));
	}
	auto tit = dc->named_tcpfd.find(dst);
	if (tit != dc->named_tcpfd.end())
	{
		LOGP("foward msg to :%s with tcp [found dst]", dst.c_str());
		return dctcp_send(dc->stcp, tit->second, dctcp_msg_t(buff, buff_sz));
	}

	//to up layer 
	if (sockfd != dc->parentfd && dc->parentfd != -1)
	{
		LOGP("foward msg to :%s not found name , so report to up layer with tcp:%d...",
			dst.c_str(), dc->parentfd);
		dctcp_send(dc->stcp, dc->parentfd, dctcp_msg_t(buff, buff_sz));
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
		LOGP("foward msg to :%s not found name , so report to lower layer with tcp:%d ...",
			dst.c_str() , it->second);
		dctcp_send(dc->stcp, it->second, dctcp_msg_t(buff, buff_sz));
	}
	return 0;
}
static int _msg_cb(dcnode_t * dc, int sockfd, uint64_t msgqpid, const char * buff, int buff_sz){
	dcnode_msg_t dm;
	if (!dm.Unpack(buff, buff_sz)) {
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
static int _smq_cb(dcsmq_t * smq, uint64_t src, const dcsmq_msg_t & msg, void * ud) {
	dcnode_t * dc = (dcnode_t*)ud;
	return _msg_cb(dc, -1, src, msg.buffer, msg.sz);
}

//server
static int _stcp_cb(dctcp_t* server, const dctcp_event_t & ev, void * ud) {
	dcnode_t * dc = (dcnode_t*)ud;
	switch (ev.type)
	{
	case dctcp_event_type::DCTCP_NEW_CONNX:
		//new connection no need build the map , but in reg name
		break;
	case dctcp_event_type::DCTCP_CONNECTED:
		//connected
		dc->parentfd = ev.fd;
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_CONNECTED);
	case dctcp_event_type::DCTCP_READ:
		//
		return _msg_cb(dc, ev.fd, 0, ev.msg->buff, ev.msg->buff_sz);
	case dctcp_event_type::DCTCP_CLOSED:
		//error record
		return _stcp_sockfd_close(dc, ev.fd);
	default: return -1;
	}
	return 0;
}
static int _check_conf(const dcnode_config_t & conf){
	if (!conf.addr.parent_addr.empty() &&
		conf.addr.msgq_push){	//tcp parent and smq using .
		//invalid node
		return -1;
	}
	return 0;
}
dcnode_t* dcnode_create(const dcnode_config_t & conf) {
	if (_check_conf(conf)){
		//config error
		return nullptr;
	}
	dcnode_t * n = new dcnode_t();
	if (!n) //memerror
		return nullptr;
	n->conf = conf;
	dctcp_config_t sc;
	sc.max_recv_buff = conf.max_channel_buff_size;
	sc.max_send_buff = conf.max_channel_buff_size;
	sc.max_tcp_send_buff_size = conf.max_channel_buff_size;
	sc.max_tcp_recv_buff_size = conf.max_channel_buff_size;

	if (!_is_leaf(n))
	{
		if (conf.addr.listen_addr.length())
		{
			sc.is_server = true;
			dctcp_addr_t saddr;
			const string & listenaddr = conf.addr.listen_addr;
			saddr.ip = listenaddr.substr(0, listenaddr.find(':'));
			saddr.port = strtol(listenaddr.substr(listenaddr.find(':') + 1).c_str(), nullptr, 10);
			sc.listen_addr = saddr;
		}
		n->stcp = dctcp_create(sc);
		if (!n->stcp) {
			dcnode_destroy(n); return nullptr;
		}
		dctcp_event_cb(n->stcp, _stcp_cb, n);
	}
	if (!conf.addr.msgq_path.empty())
	{
		dcsmq_config_t smc;
		smc.key = conf.addr.msgq_path;
		smc.msg_buffsz = conf.max_channel_buff_size;
		smc.server_mode = !_is_leaf(n);

		n->smq = dcsmq_create(smc);
		if (!n->smq) {
			dcnode_destroy(n);
			return nullptr;
		}
		dcsmq_msg_cb(n->smq, _smq_cb, n);
		if (smc.server_mode){
			if (_name_smq_maping_shm_create(n, true)){
				LOGP("create shm error !");
				dcnode_destroy(n);
				return nullptr;
			}			
		}
		else {
			dcsmq_set_session(n->smq, getpid());	//default session
		}
	}
	n->send_buffer.create(conf.max_channel_buff_size);
	_fsm_start_heart_beat_timer(n);
	return n;
}
void      dcnode_destroy(dcnode_t* dc){
	if (dc->stcp)
	{
		dctcp_destroy(dc->stcp);
		dc->stcp = nullptr;
	}
	if (dc->smq)
	{
		dcsmq_destroy(dc->smq);
		dc->smq = nullptr;
	}
	if (dc->smq_name_mapping_shm){
		dcshm_destroy(dc->smq_name_mapping_shm);
	}
	dc->init();
	delete dc;
}
static	void _check_timer_callback(dcnode_t * dc){
	uint64_t currentms = util::time_unixtime_ms();
	std::vector<std::pair<uint64_t, uint64_t> >		addagin;
	for (auto it = dc->expiring_callbacks.begin(); it != dc->expiring_callbacks.end();)
	{
		if (it->first > currentms){ break; }
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
	if (dc->fsm_state == dcnode_t::DCNODE_ABORT){
		return ;
	}
	//check cb
	_check_timer_callback(dc);

	_fsm_check(dc, false);

	if (dc->stcp) {
		dctcp_poll(dc->stcp, timout_us);
	}
	if (dc->smq) {
		dcsmq_poll(dc->smq, timout_us);
	}
	_check_timer_callback(dc);
}

//typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t* dc, dcnode_dispatcher_t dspatch, void * ud) {
	dc->dispatcher = dspatch;
	dc->dispatcher_ud = ud;
}
uint64_t  dcnode_timer_add(dcnode_t * dc, int delayms, dcnode_timer_callback_t cb, bool repeat) {
	return	_insert_timer_callback(dc, delayms, cb, repeat);
}
void	  dcnode_timer_cancel(dcnode_t * dc, uint64_t cookie) {
	_remove_timer_callback(dc, cookie);
}
static  bool _name_exists(dcnode_t * dc, const char * name){
	if (!name || !name[0]){
		return false;
	}
	if (dc->named_smqid.find(name) != dc->named_smqid.end()){
		return true;
	}
	if (dc->named_tcpfd.find(name) != dc->named_tcpfd.end()){
		return true;
	}
	return false;
}

int      dcnode_send(dcnode_t* dc, const char * dst, const char * buff, int sz){
	if (dc->fsm_state != dcnode_t::DCNODE_READY &&
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
	if (!dm.Pack(dc->send_buffer)){
		//error
		return -1;
	}
	//dc, tcp_src,sockfd, buff, buff_sz, dm.dst()
	return _forward_msg(dc, -1, dc->send_buffer.buffer, dc->send_buffer.valid_size, string(dst));
}

void	  dcnode_abort(dcnode_t * dc){
	_fsm_abort(dc, 0);
}
//-1:stop ; 0:not ready;1:ready
int		  dcnode_ready(dcnode_t *  dc){
	if (dc->fsm_state == dcnode_t::DCNODE_READY){
		return 1;
	}
	else if (dc->fsm_state != dcnode_t::DCNODE_ABORT){
		return 0;
	}
	else {
		return -1;
	}
}


