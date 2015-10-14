#include "dagent.h"

struct dagent_t
{
	//
	dcnode_t * node;
	dagent_config_t conf;
	unordered_map<int, dagent_cb_t>		cbs;
	//python vm todo
	msg_buffer_t						send_msgbuffer;
	char								last_error_msg[1024];
	int									last_errno;
};


static dagent_t AGENT;

static int	_error(int err, const char * msg)
{
	AGENT.last_errno = err;
	strncpy(AGENT.last_error_msg, msg, sizeof(AGENT.last_error_msg) - 1);
	return err;
}

static int _dispatcher(void * ud, const dcnode_msg_t & msg)
{
	assert(ud == &AGENT);
	dagent_msg_t	dm;
	if (!dm.unpack(msg.msg_data().c_str(), msg.msg_data().length()))
	{
		//error pack
		return _error(-1, "msg unpack error !");
	}
	auto it = AGENT.cbs.find(dm.type());
	if (it != AGENT.cbs.end())
	{
		return it->second(dm, msg.src());
	}
	//not found
	return _error(-2, "not found handler !");
}
int     dagent_init(const dagent_config_t & conf)
{
	if (AGENT.node)
	{
		return _error(-1, "node has been inited !");
	}
	dcnode_t * node = dcnode_create(conf.node_conf);
	if (!node)
	{
		//node error
		return _error(-2, "create dcnode error !");
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
	if (!msg.pack(AGENT.send_msgbuffer))
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

const char *	dagent_errmsg()
{
	return	AGENT.last_error_msg;
}
int				dagent_errno()
{
	return	AGENT.last_errno;
}
const char *	dagent_errro_str(int err)
{
	return "";
}









