#include "dagent.h"

struct dagent_t
{
	//
	dcnode_t * node;
	dagent_config_t conf;
	unordered_map<int, dagent_cb_t>		cbs;
	//python vm todo
};

static dagent_t AGENT;

static int _dispatcher(const char* src, const dcnode_msg_t & msg)
{
	dagent_msg_t	dm;
	if (!dm.SerilizeFromBytes(msg.data, msg.sz))
	{
		//error for msg format 
		return -1;
	}

	auto it = AGENT.cbs.find(dm.type());
	if (it != AGENT.cbs.end())
	{
		return it->second(dm);
	}
	//not found
	return -2;
}
int     dagent_init(const dagent_config_t & conf)
{
	if (AGENT.node)
	{
		return -1;
	}
	dcnode_t * node = dcnode_create(conf.node_conf);
	if (!node)
	{
		//node error
		return -2;
	}
	dcnode_set_dispatcher(_dispatcher);
	AGENT.conf = conf;
	AGENT.node = node;
	return 0;
}

void    dagent_destroy()
{
	dcnode_destroy(AGENT.node);
	AGENT.node = NULL;
}
void    dagent_update()
{
	//1ms
	struct timeval tv;
	gettimeofday(&tv, NULL);
	dcnode_update(AGENT.node, tv, 1000);	
}
int     dagent_send(const char * dst, const dagent_msg_t & msg)
{	
	static char buffer[1024*1024];
	if (!msg.SerializeToArray(buffer, sizeof(buffer)))
	{
		//serialize error
		return -1;
	}
	return dcnode_send(AGENT.node, dst, dcnode_msg_t(buffer, msg.ByteSize()));
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










