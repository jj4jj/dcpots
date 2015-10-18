#include "dagent.h"
#include "dcnode/error_msg.h"

struct dagent_t
{
	//
	dcnode_t * node;
	dagent_config_t conf;
	unordered_map<int, dagent_cb_t>		cbs;
	//python vm todo
	msg_buffer_t						send_msgbuffer;
	error_msg_t					*		error_msg;
};


static dagent_t AGENT;

static inline int	_error(const char * msg, int err = -1, error_msg_t * trace = nullptr)
{
	ERROR_BT_MSG(AGENT.error_msg, trace, err, msg);
	return err;
}
static int _dispatcher(void * ud, const char * src, const msg_buffer_t & msg)
{
	assert(ud == &AGENT);
	dagent_msg_t	dm;
	if (!dm.Unpack(msg.buffer, msg.valid_size))
	{
		//error pack
		return _error("msg unpack error !");
	}
	auto it = AGENT.cbs.find(dm.type());
	if (it != AGENT.cbs.end())
	{
		return it->second(dm, src);
	}
	//not found
	return _error("not found handler !");
}
int     dagent_init(const dagent_config_t & conf)
{
	if (AGENT.node)
	{
		return _error("node has been inited !");
	}
	if (AGENT.send_msgbuffer.create(conf.max_msg_size))
		return _error("create send msgbuffer error !");
	if (!(AGENT.error_msg = error_create(1024)))
		return _error("create error msg error !");
	dcnode_t * node = dcnode_create(conf.node_conf);
	if (!node)
	{
		//node error
		return _error("create dcnode error !");
	}
	dcnode_set_dispatcher(node, _dispatcher, &AGENT);
	AGENT.conf = conf;
	AGENT.node = node;
	return 0;
}

void    dagent_destroy()
{
	dcnode_destroy(AGENT.node);
	AGENT.node = NULL;
	AGENT.cbs.clear();
	AGENT.send_msgbuffer.destroy();
}
void    dagent_update(int timeout_ms)
{
	dcnode_update(AGENT.node, timeout_ms*1000);	
}
int     dagent_send(const char * dst, const dagent_msg_t & msg)
{	
	if (!msg.Pack(AGENT.send_msgbuffer))
	{
		//serialize error
		return -1;
	}
	return dcnode_send(AGENT.node, dst, AGENT.send_msgbuffer.buffer, AGENT.send_msgbuffer.valid_size);
}
int     dagent_cb_push(int type, dagent_cb_t cb)
{
	auto it = AGENT.cbs.find(type);
	if (it != AGENT.cbs.end())
	{
		//repeat
		return -1;
	}
	AGENT.cbs[type] = cb;
	return 0;
}
int     dagent_cb_pop(int type)
{
	auto it = AGENT.cbs.find(type);
	if (it != AGENT.cbs.end())
	{
		AGENT.cbs.erase(it);
		return 0;
	}
	//not found
	return -1;
}

//reg python file
int     dagent_init_plugins()
{
	//todo
	return -1;
}
int     dagent_destroy_plugins()
{
	//todo
	return -1;

}
int     dagent_reload_plugins()
{
	//todo
	return -1;
}

struct error_msg_t * dagent_errmsg()
{
	return AGENT.error_msg;
}








