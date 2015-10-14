#include "dcnode.h"
#include "libmq.h"
#include "libtcp.h"

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
	stcp_t	*	parent; //to parent
	stcp_t  *	listener; //children
	smq_t	*	smq;	//mq
	int			state;
	time_t		next_stat_time;

	//local name map addr[id]
	dcnode_dispatcher_t		dispatcher;
	void *					dispatcher_ud;

	//timer cb
	std::multimap<uint64_t, uint64_t>						expiring_callbacks;
	std::unordered_map<uint64_t, dcnode_timer_callback_t>	timer_callbacks;

	//smq children (agent)
	std::unordered_map<string, uint64_t>				smq_children;
	std::unordered_map<uint64_t, string>				smq_children_map_name;

	//tcp children (agent)
	std::unordered_map<string, int>						tcp_children;
	std::unordered_map<int, string>						tcp_children_map_name;
	//
	msg_buffer_t										send_buffer;

	dcnode_t()
	{
		init();
	}
	void init()
	{
		smq = nullptr;
		listener = nullptr;
		parent = nullptr;
		state = DCNODE_INIT;
		dispatcher = nullptr;
		timer_callbacks.clear();
		expiring_callbacks.clear();
		smq_children.clear();
		smq_children_map_name.clear();
		tcp_children.clear();
		tcp_children_map_name.clear();
		next_stat_time = 0;		
	}
};

static uint32_t	s_cb_seq = 0;
static uint64_t  _insert_timer_callback(dcnode_t * dc , int32_t expired_ms, dcnode_timer_callback_t cb)
{
	uint64_t cookie = time(NULL);
	cookie <<= 24;
	cookie |= s_cb_seq++;
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t expiretimems = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	expiretimems += expired_ms;
	dc->expiring_callbacks.insert(std::make_pair(expiretimems, cookie));
	dc->timer_callbacks[cookie] = cb;
	return cookie;
}
static void	_remove_timer_callback(dcnode_t* dc, uint64_t cookie)
{
	dc->timer_callbacks.erase(cookie);
}

//leaf: no children(no stcp listen)
static bool _is_leaf(dcnode_t* dc)
{
	return (dc->listener) == nullptr;
}
static int _register_name(dcnode_t * dc)
{
	dcnode_msg_t	msg;
	msg.set_type(dcnode::MSG_REG_NAME);
	msg.set_src(dc->conf.name);
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
		ret = smq_send(dc->smq, 0, smq_msg_t(buffer, sz));
	}
	else
	{
		if (!dc->parent)
		{
			//no need
			dc->state = dcnode_t::DCNODE_NAME_REG;
			return 0;
		}
		//to parent
		ret = stcp_send(dc->parent, stcp_msg_t(buffer, sz));
	}
	if (ret)
	{
		//send err
		return ret;
	}
	return 0;
}
static  int _connect_parent(dcnode_t * dc)
{
	if (!dc->parent)
	{
		dc->state = dcnode_t::DCNODE_CONNECTED;
		return 0;
	}
	dc->state = dcnode_t::DCNODE_CONNECTING;
	stcp_addr_t saddr;
	saddr.ip = dc->conf.addr.parent_addr.substr(0, dc->conf.addr.parent_addr.find(':'));
	saddr.port = strtol(dc->conf.addr.parent_addr.substr(dc->conf.addr.parent_addr.find(':')+1).c_str(),NULL,10);
	return stcp_connect(dc->parent, saddr);
}
static int	_check_dcnode_fsm(dcnode_t * dc, bool checkforce)
{
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
	case dcnode_t::DCNODE_READY:
		//nothing
		timeout_time = 30;//20s
		break;
	default: return -1;
	}
	dc->next_stat_time = tNow + timeout_time;
	return 0;
}
static void _update_children_name(dcnode_t * dc, stcp_t * src, uint64_t smqid, int sockfd , const string & name)
{
	if (src != nullptr)
	{
		if (src == dc->listener)
		{
			//children
			auto it = dc->tcp_children.find(name);
			if (it != dc->tcp_children.end())
			{
				dc->tcp_children_map_name.erase(it->second);
				dc->tcp_children.erase(it);
			}
			dc->tcp_children[name] = sockfd;
			dc->tcp_children_map_name[sockfd] = name;
		}
		else
		{
			//parent name noneed
		}
	}
	else // if (smqid > 0)
	{
		assert(smqid > 0);
		auto it = dc->smq_children.find(name);
		if (it != dc->smq_children.end())
		{
			dc->smq_children_map_name.erase(it->second);
			dc->smq_children.erase(it);
		}
		dc->smq_children[name] = smqid;
		dc->smq_children_map_name[smqid] = name;
	}
}
static int _response_msg(dcnode_t * dc, stcp_t * tcp_src, int sockfd, uint64_t msgqpid, dcnode_msg_t & dm, const dcnode_msg_t & dmsrc)
{
	dm.set_src(dc->conf.name);
	dm.set_dst(dmsrc.src());
	bool ret = dm.pack(dc->send_buffer);
	if (!ret)
	{
		//error pack
		return -1;
	}
	if (tcp_src)
	{
		return stcp_send(tcp_src, sockfd, stcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else
	{
		assert(msgqpid > 0);
		return smq_send(dc->smq, msgqpid, smq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
}
static int _handle_msg(dcnode_t * dc, const dcnode_msg_t & dm, stcp_t * tcp_src, int sockfd, uint64_t msgqpid)
{
	//to me
	switch (dm.type())
	{
	case dcnode::MSG_DATA:
		//to up
		return dc->dispatcher(dc->dispatcher_ud, dm);
		break;
	case dcnode::MSG_REG_NAME:
		//insert tcp src -> map name
		if (dc->state == dcnode_t::DCNODE_NAME_REG_ING)
		{
			dc->state = dcnode_t::DCNODE_NAME_REG;
			return _check_dcnode_fsm(dc, true);
		}
		else
		{
			_update_children_name(dc, tcp_src, msgqpid, sockfd,dm.src());
			//response msg todo
			dcnode_msg_t ddm;
			ddm.set_type(dcnode::MSG_REG_NAME);
			_response_msg(dc, tcp_src, msgqpid, sockfd, ddm, dm);
		}
		break;
	case dcnode::MSG_HEART_BEAT:
		// todo update expiring info
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

static int _forward_msg(dcnode_t * dc, stcp_t * tcp_src, int sockfd, const char * buff, int buff_sz, const string & dst)
{
	//check children name , if not found , send to parent except src
	auto it = dc->smq_children.find(dst);
	if (it != dc->smq_children.end())
	{
		return smq_send(dc->smq, it->second, smq_msg_t(buff, buff_sz));
	}
	auto tit = dc->tcp_children.find(dst);
	if (tit != dc->tcp_children.end())
	{
		return stcp_send(dc->listener, tit->second, stcp_msg_t(buff, buff_sz));
	}

	for (auto it = dc->tcp_children.begin();
		it != dc->tcp_children.end(); it++)
	{
		if (it->second == sockfd)
		{
			continue;
		}
		stcp_send(dc->listener, it->second, stcp_msg_t(buff, buff_sz));
	}
	if (tcp_src != dc->parent)
	{
		stcp_send(dc->parent, stcp_msg_t(buff, buff_sz));
	}
	return 0;
}
static int _msg_cb(dcnode_t * dc, stcp_t * tcp_src, int sockfd, uint64_t msgqpid, const char * buff, int buff_sz)
{
	dcnode_msg_t dm;
	if (!dm.unpack(buff, buff_sz))
	{
		//error for decode
		return -1;
	}
	if (dm.dst().length() == 0 ||
		dm.dst() == dc->conf.name)
	{
		//to me
		return _handle_msg(dc, dm, tcp_src, sockfd, msgqpid);
	}
	else
	{
		return _forward_msg(dc, tcp_src,sockfd, buff, buff_sz, dm.dst());
	}
}
static int _smq_cb(smq_t * smq, uint64_t src, const smq_msg_t & msg, void * ud)
{
	dcnode_t * dc = (dcnode_t*)ud;
	return _msg_cb(dc, nullptr, 0, src, msg.buffer, msg.sz);
}

//server
static int _stcp_server_cb(stcp_t* server, const stcp_event_t & ev, void * ud)
{
	dcnode_t * dc = (dcnode_t*)ud;
	switch (ev.type)
	{
	case stcp_event_type::STCP_CONNECTED:
		//new connection no need build the map , but in reg name
		break;
	case stcp_event_type::STCP_READ:
		//
		return _msg_cb(dc, dc->listener, ev.fd,0, ev.msg->buff, ev.msg->buff_sz);
		break;
	case stcp_event_type::STCP_CLOSED:
	{
		auto it = dc->tcp_children_map_name.find(ev.fd);
		if (it != dc->tcp_children_map_name.end())
		{
			dc->tcp_children.erase(it->second);
			dc->tcp_children_map_name.erase(it);
		}
	}
		break;
	default: return -1;
	}
	return 0;
}
//client connect parent
static int _stcp_client_cb(stcp_t* client, const stcp_event_t & ev, void * ud)
{
	dcnode_t * dc = (dcnode_t*)ud;
	switch (ev.type)
	{
	case stcp_event_type::STCP_CONNECTED:
		//connected
		dc->state = dcnode_t::DCNODE_CONNECTED;
		return _check_dcnode_fsm(dc, true);
	case stcp_event_type::STCP_READ:
		//reada msg
		return _msg_cb(dc, client, ev.fd, 0 , ev.msg->buff, ev.msg->buff_sz);
	case stcp_event_type::STCP_CLOSED:
		//init state
		dc->state = dcnode_t::DCNODE_INIT;
		return _check_dcnode_fsm(dc, true);
	default: return -1;
	}
	return 0;
}

dcnode_t* dcnode_create(const dcnode_config_t & conf)
{
	dcnode_t * n = new dcnode_t();
	if (!n)
		return nullptr;
	
	if (conf.addr.listen_addr.length())
	{
		stcp_config_t sc;
		sc.is_server = true;
		stcp_addr_t saddr;
		const string & listenaddr = conf.addr.listen_addr;
		saddr.ip = listenaddr.substr(0, listenaddr.find(':'));
		saddr.port = strtol(listenaddr.substr(listenaddr.find(':') + 1).c_str(), nullptr, 10);
		sc.listen_addr = saddr;
		sc.max_recv_buff = conf.max_channel_buff_size;
		sc.max_send_buff = conf.max_channel_buff_size;
		stcp_t * stcp = stcp_create(sc);
		if (!stcp)
		{
			dcnode_destroy(n);
			return nullptr;
		}
		n->listener = stcp;
		stcp_event_cb(stcp, _stcp_server_cb, n);
	}

	if (conf.addr.parent_addr.length())
	{
		stcp_config_t sc;
		sc.is_server = false;
		sc.max_recv_buff = conf.max_channel_buff_size;
		sc.max_send_buff = conf.max_channel_buff_size;
		stcp_t * stcp = stcp_create(sc);
		if (!stcp)
		{
			dcnode_destroy(n);
			return nullptr;
		}
		n->parent = stcp;
		stcp_event_cb(stcp, _stcp_client_cb, n);
	}

	if (conf.addr.msgq_key.length())
	{
		smq_config_t sc;
		sc.key = conf.addr.msgq_key;
		sc.msg_buffsz = conf.max_channel_buff_size;
		smq_t * smq = smq_create(sc);
		if (!smq)
		{
			dcnode_destroy(n);
			return nullptr;
		}
		n->smq = smq;
		smq_msg_cb(smq, _smq_cb, n);
	}
	n->send_buffer.create(conf.max_channel_buff_size);

	return n;
}
void      dcnode_destroy(dcnode_t* dc)
{
	if (dc->parent)
	{
		stcp_destroy(dc->parent);
		dc->parent = nullptr;
	}
	if (dc->listener)
	{
		stcp_destroy(dc->listener);
		dc->listener = nullptr;
	}
	if (dc->smq)
	{
		smq_destroy(dc->smq);
		dc->smq = nullptr;
	}
	for (auto it = dc->smq_children.begin();
		it != dc->smq_children.end(); ++it)
	{

	}
	dc->smq_children.clear();


	for (auto it = dc->tcp_children.begin();
		it != dc->tcp_children.end(); ++it)
	{
	}
	dc->tcp_children.clear();

	dc->init();
}
static	void _check_timer_callback(dcnode_t * dc)
{
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t currentms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	auto itend = dc->expiring_callbacks.upper_bound(currentms);

	for (auto it = dc->expiring_callbacks.begin(); it != itend;)
	{
		auto previt = it++;
		auto funcit = dc->timer_callbacks.find(previt->second);
		if (funcit != dc->timer_callbacks.end())
		{
			funcit->second();//call back
			dc->timer_callbacks.erase(funcit);
		}
		dc->expiring_callbacks.erase(previt);
	}
}
void      dcnode_update(dcnode_t* dc, int timout_us)
{
	//check cb
	_check_timer_callback(dc);

	_check_dcnode_fsm(dc, false);

	if (dc->state == dcnode_t::DCNODE_READY && dc->listener)
	{
		stcp_poll(dc->listener, timout_us);
	}
	if (dc->state == dcnode_t::DCNODE_READY && dc->smq)
	{
		smq_poll(dc->smq, timout_us);
	}

	_check_timer_callback(dc);
}

//typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t* dc, dcnode_dispatcher_t dspatch, void * ud)
{
	dc->dispatcher = dspatch;
	dc->dispatcher_ud = ud;
}
uint64_t  dcnode_timer_add(dcnode_t * dc, int delayms, dcnode_timer_callback_t cb)
{
	return	_insert_timer_callback(dc, delayms, cb);
}
void	  dcnode_timer_cancel(dcnode_t * dc, uint64_t cookie)
{
	_remove_timer_callback(dc, cookie);
}


int      dcnode_send(dcnode_t* dc, const char * dst, const char * buff, int sz)
{
	dcnode_msg_t dm;
	dm.set_src(dc->conf.name);
	dm.set_dst(dst);
	dm.set_type(dcnode::MSG_DATA);
	dm.set_msg_data(buff, sz);	
	if (!dm.pack(dc->send_buffer))
	{
		//error ser
		return -1;
	}
	//dc, tcp_src,sockfd, buff, buff_sz, dm.dst()
	return _forward_msg(dc, nullptr, 0, dc->send_buffer.buffer, dc->send_buffer.valid_size, string(dst));
}

