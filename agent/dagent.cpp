#include "dagent.h"

struct dagent_t
{
	//
	dcnode_t * node;
	dagent_config_t conf;
};

static dagent_t AGENT;
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
	dcnode_send(AGENT.node, dst, );
}
int     dagent_cb_push(int type, dagent_cb_t cb)
{

}
int     dagent_cb_pop(int type)
{

}

//reg python file
int     dagent_init_plugins()
{

}
int     dagent_destroy_plugins()
{

}
int     dagent_reload_plugins()
{

}










