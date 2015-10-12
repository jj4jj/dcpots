#include "dcnode.h"
#include "libmq.h"
#include "libtcp.h"




struct dcnode_t
{
	dcnode_config_t conf;
	stcp_t	*	parent;
	stcp_t  *	listener;
	smq_t	*	smq;	//mq
	bool		ready;
	//local name map addr[id]


	dcnode_t() :smq(nullptr), listener(nullptr), parent(nullptr), ready(false)
	{
	}
};

//leaf: no children(no stcp listen)
static bool _is_leaf(dcnode_t* dc)
{
	return (dc->listener) == nullptr;
}
static int _register_name(dcnode_t * dc , const string & name, bool block)
{
	dcnode_msg_t	msg;
	int ret = stcp_send(dc->parent, );
	if (! stcp_dc->connector)
	{
		return -1;
	}
}

dcnode_t* dcnode_create(const dcnode_config_t & conf)
{
	dcnode_t * n = new dcnode_t();
	if (!n)
		return nullptr;

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
	}
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
		//typedef int (*stcp_event_cb_t)(stcp_t*, const stcp_event_t & ev);
		stcp_addr_t saddr;
		saddr.ip = conf.addr.parent_addr;
		saddr.port = conf.addr.parent_addr; //get port todo
		stcp_connect(stcp, saddr, 2000, true);
		n->parent = stcp;
	}
	
	//parent
	if (n->parent)
	{
		//register name
		_register_name(conf.name, true);
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
}
void      dcnode_update(dcnode_t*, const timeval & tv, int timout_us)
{

}

//typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t*, dcnode_dispatcher_t)
{

}

int      dcnode_listen(dcnode_t*, const dcnode_addr_t & addr)
{

}
int      dcnode_connect(dcnode_t*, const dcnode_addr_t & addr)
{

}
int      dcnode_send(dcnode_t*, const char * dst, const dcnode_msg_t & msg)
{

}

