#pragma once

#include "dcnode/dcnode.h"
#include "proto/dagent.pb.h"

typedef msgproto_t<dagent::MsgDAgent>	dagent_msg_t;

enum dagent_plugin_type {
	DAGENT_PLUGIN_LUA = 1,
	DAGENT_PLUGIN_PYTHON = 2,
};
struct dagent_config_t
{
    dcnode_config_t node_conf;
	int				max_msg_size;//1MB	
	dagent_config_t() :max_msg_size(1048576){}
};

typedef int (*dagent_cb_t)(const dagent_msg_t &  msg, const string & src);

int     dagent_init(const dagent_config_t & conf);
void    dagent_destroy();
void    dagent_update(int timeout_ms = 10);
int     dagent_send(const char * dst, const dagent_msg_t & msg);
int     dagent_cb_push(int type, dagent_cb_t cb);
int     dagent_cb_pop(int type);

int     dagent_load_plugin(const char * file);
int     dagent_unload_plugin(const char * file);

