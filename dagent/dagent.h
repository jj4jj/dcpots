#pragma once
#include "base/stdinc.h"
#include "base/msg_buffer.hpp"

enum dagent_plugin_type {
	DAGENT_PLUGIN_LUA = 1,
	DAGENT_PLUGIN_PYTHON = 2,
};
struct dagent_config_t
{
	string	name;
	int		max_msg_size;//1MB	
	bool	routermode; //just relay msg
	string	localkey;
	string	parent;	//parent tcp addr eg.127.0.0.1:8888
	string	listen;	//listen 127.0.0.1:8888
	int		hearbeat;//3*heartbeat timeout will set be dead
	dagent_config_t() :max_msg_size(1048576),routermode(false){
		name = "noname";
		max_msg_size = 1048576;
		routermode = false;
		parent = "";
		listen = "";
		localkey = "";
		hearbeat = 10;
	}
};

typedef int (*dagent_cb_t)(const msg_buffer_t &  msg, const char * src);

int     dagent_init(const dagent_config_t & conf);
void    dagent_destroy();
bool	dagent_ready(); //ready to write ?
void    dagent_update(int timeout_ms = 10);
int     dagent_send(const char * dst, int type, const msg_buffer_t & msg);
int     dagent_cb_push(int type, dagent_cb_t cb);
int     dagent_cb_pop(int type);

int     dagent_load_plugin(const char * file);
int     dagent_unload_plugin(const char * file);

