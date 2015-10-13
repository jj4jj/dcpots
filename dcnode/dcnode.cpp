#include "dcnode.h"
#include "libmq.h"
#include "libtcp.h"

//int->errortype
//int ec
//ok:0
//error:1:timeout
enum DCNodeCBErrorType
{
	DCNODE_CB_OK = 0,
	DCNODE_CB_TIMEOUT = 1
};
typedef	std::function<int(int)>	dcnode_callback_t;

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
	stcp_t	*	parent;
	stcp_t  *	listener;
	smq_t	*	smq;	//mq
	int			state;
	time_t		next_stat_time;
	//local name map addr[id]
	dcnode_dispatcher_t		dispatcher;
	void *					dispatcher_ud;
	std::multimap<uint64_t, uint64_t>					expiring_info;//time->cookie for rpc
	std::unordered_map<uint64_t, dcnode_callback_t>		callbacks;

	std::unordered_map<string, uint64_t>				smq_children;
	std::unordered_map<uint64_t, string>				smq_children_map_name;

	std::unordered_map<string, stcp_t *>				tcp_children;
	std::unordered_map<stcp_t *, string>				tcp_children_map_name;
	
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
		callbacks.clear();
		expiring_info.clear();
		next_stat_time = 0;		
	}
};

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
	}
	dc->next_stat_time = tNow + timeout_time;
}

//leaf: no children(no stcp listen)
static bool _is_leaf(dcnode_t* dc)
{
	return (dc->listener) == nullptr;
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
	saddr.ip = dc->conf.addr.parent_addr;
	saddr.port = dc->conf.addr.parent_addr; //get port todo
	return stcp_connect(dc->parent, saddr);
}

static int _register_name(dcnode_t * dc)
{
	dcnode::dcnode_msg_t	msg;
	msg.set_type(dcnode::MSG_REG_NAME);
	msg.set_src(dc->conf.name);
	static char buffer[1024];
	if (!msg.SerializeToBytes(buffer, sizeof(buffer)))
	{
		//pack error
		return -1;
	}
	int ret = 0;
	dc->state = dcnode_t::DCNODE_NAME_REG_ING;
	if (_is_leaf(dc))
	{
		//to agent
		ret = smq_send(dc->smq, 0, smq_msg_t(buffer, msg.ByteSize()));
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
		ret = stcp_send(dc->parent, stcp_msg_t(buffer, msg.ByteSize()));
	}
	if (ret)
	{
		//send err
		return ret;
	}
	return 0;
}
//
static int _smq_cb(smq_t * smq, uint64_t src, const smq_msg_t & msg, void * ud)
{
	dcnode_t * dc = (dcnode_t*)ud;
	//get mypid msg
	return _msg_cb(dc, nullptr, src, msg.buff, msg.buff_sz);
}

static int _handle_msg(dcnode_t * dc,const dcnode_msg_t & dm, stcp_t * tcp_src, uint64_t msgqpid)
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
			_check_dcnode_fsm(dc, true);
		}
		else
		{
			if (tcp_src)
			{
				auto it = dc->tcp_children.find(dm.src());
				if (it != dc->tcp_children.end())
				{
					dc->tcp_children_map_name.erase(it->second);
					dc->tcp_children.erase(it);
				}
				dc->tcp_children[dm.src()] = tcp_src;
				dc->tcp_children_map_name[tcp_src] = dm.src;
			}
			else
			{
				assert(msgqpid > 0);
				auto it = dc->smq_children.find(dm.src());
				if (it != dc->smq_children.end())
				{
					dc->smq_children_map_name.erase(it->second);
					dc->smq_children.erase(it);
				}
				dc->smq_children[dm.src()] = msgqpid;
				dc->smq_children_map_name[msgqpid] = dm.src;
			}
			//response msg

		}
		break;
	case dcnode::MSG_HEART_BEAT:
		// todo update expiring info
		break;
	case dcnode::MSG_RPC:
		break;
	default:
		//error type
		return - 1;
		break;
	}
	return 0;
}

static int _forward_msg(dcnode_t * dc, stcp_t * tcp_src, const char * buff, int buff_sz, const string & dst)
{
	//check children name , if not found , send to parent except src
	//forward
	auto it = dc->smq_children.find(dst);
	if (it != dc->smq_children.end())
	{
		return smq_send(dc->smq, it->second, smq_msg_t(buff, buff_sz));
	}
	auto tit = dc->tcp_children.find(dst);
	if (tit != dc->tcp_children.end())
	{
		return stcp_send(tit->second, stcp_msg_t(buff, buff_sz));
	}

	for (auto it = dc->tcp_children.begin();
		it != dc->tcp_children.end(); it++)
	{
		if (it->second == tcp_src)
		{
			continue;
		}
		stcp_send(it->second, stcp_msg_t(buff, buff_sz));
	}
	if (tcp_src != dc->parent)
	{
		stcp_send(dc->parent, stcp_msg_t(buff, buff_sz));
	}
	return 0;
}

static int _msg_cb(dcnode_t * dc, stcp_t * tcp_src, uint64_t msgqpid, const char * buff, int buff_sz)
{
	dcnode_msg_t dm;
	if (!dm.SerializeFromBytes(buff, buff_sz))
	{
		//error for decode
		return -1;
	}
	if (dm.dst().length() == 0 ||
		dm.dst() == dc->conf.name)
	{
		//to me
		return _handle_msg(dc, dm, tcp_src, msgqpid);
	}
	else
	{
		return _forward_msg(dc, tcp_src, buff, buff_sz, dm.dst);		
	}
}

//server
static int _stcp_server_cb(stcp_t* server, const stcp_event_t & ev, void * ud)
{
	dcnode_t * dc = (dcnode_t*)ud;
	switch (ev.type)
	{
	case stcp_event_type::STCP_CONNECTED:
		//new connection
		break;
	case stcp_event_type::STCP_READ:
		//
		return _msg_cb(dc, ev.stcp,0, ev.msg->buff, ev.msg->buff_sz);
		break;
	case stcp_event_type::STCP_CLOSED:
		auto it = dc->tcp_children_map_name.find(ev.stcp);
		if (it != dc->tcp_children_map_name.end())
		{
			dc->tcp_children.erase(it->second);
			dc->tcp_children_map_name.erase(it);
		}
		break;
	}

}
//client
static int _stcp_client_cb(stcp_t* client, const stcp_event_t & ev, void * ud)
{
	dcnode_t * dc = (dcnode_t*)ud;
	switch (ev.type)
	{
	case stcp_event_type::STCP_CONNECTED:
		//connected
		dc->state = dcnode_t::DCNODE_CONNECTED;
		_check_dcnode_fsm(dc, true);
		break;
	case stcp_event_type::STCP_READ:
		//
		return _msg_cb(dc, ev.stcp,0, ev.msg->buff, ev.msg->buff_sz);
		break;
	case stcp_event_type::STCP_CLOSED:
		_connect_parent(dc);
		break;
	}
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
		sc.listen_addr = conf.addr.listen_addr;
		sc.max_recv_buff = conf.max_channel_buff_size;
		sc.max_send_buff = conf.max_channel_buff_size;
		stcp_t * stcp = stcp_create(sc);
		if (!stcp)
		{
			dcnode_destroy(n);
			return nullptr;
		}
		n->listener = stcp;
		stcp_event_cb(stcp, _stcp_server_cb);
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
		stcp_event_cb(stcp, _stcp_client_cb);
	}

	if (conf.addr.msgq_key.length())
	{
		smq_config_t sc;
		sc.key = conf.addr.msgq_key;
		sc.buffsz = conf.max_channel_buff_size;
		smq_t * smq = smq_create(sc);
		if (!smq)
		{
			dcnode_destroy(n);
			return nullptr;
		}
		n->smq = smq;
		if (_is_leaf(n))
		{
			smq_msg_cb(smq, _smq_leaf_cb);
		}
		else
		{
			smq_msg_cb(smq, _smq_agent_cb);
		}
	}
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
static	void _check_callback(dcnode_t * dc)
{
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t currentms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	auto itend = dc->expiring_info.upper_bound(currentms);

	for (auto it = dc->expiring_info.begin(); it != itend;)
	{
		auto previt = it++;
		auto funcit = dc->callbacks.find(previt->second());
		if (funcit != dc->callbacks.end())
		{
			funcit->second(DCNODE_CB_TIMEOUT);//call back
			dc->callbacks.erase(funcit);
		}
		dc->expiring_info.erase(previt);
	}
}
void      dcnode_update(dcnode_t* dc, const timeval & tv, int timout_us)
{
	//check cb
	_check_callback(dc);

	_check_dcnode_fsm(dc);

	if (dc->state == dcnode_t::DCNODE_READY && dc->listener)
	{
		stcp_poll(dc->listener, timout_us);
	}
	if (dc->state == dcnode_t::DCNODE_READY && dc->smq)
	{
		smq_poll(dc->smq, timout_us);
	}

	_check_callback(dc);
}

//typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t* dc, dcnode_dispatcher_t dspatch)
{
	dc->dispatcher = dspatch;
}

int      dcnode_listen(dcnode_t*, const dcnode_addr_t & addr)
{

}
int      dcnode_connect(dcnode_t*, const dcnode_addr_t & addr)
{

}

int      dcnode_send(dcnode_t* dc, const char * dst, const char * buff, int sz)
{
	dcnode_msg_t dm;
	dm.set_src(dc->conf.name);
	dm.set_dst(dst);
	dm.set_type(dcnode::MSG_DATA);
	dm.set_msg_data(buff, sz);
	static char buffer[1024*1024];
	if(!dm.SerializeToBytes(buffer, sizeof(buffer)))
	{
		//error ser
		return -1;
	}
	return _forward_msg(dc, nullptr, buffer, dm.ByteSize(), string(dst));
}

