#pragma once
#include "base/stdinc.h"
#include "base/msg_buffer.hpp"

struct dagent_config_t
{
	string	name;
	int		max_msg_size;//1MB	
    string  addr;
	int		heartbeat;//3*heartbeat timeout will set be dead
	string	plugin_path;
	bool	extmode;
	dagent_config_t() :max_msg_size(1048576){
		name = "noname";
		max_msg_size = 1048576;
		heartbeat = 10;
		plugin_path = "plugins";
		extmode = false;
	}
};

typedef int (*dagent_cb_t)(const msg_buffer_t &  msg, const char * src);

int     dagent_init(const dagent_config_t & conf);
void    dagent_destroy();
int		dagent_ready(); //1:ready;0:not ready;-1:abort
void    dagent_update(int timeout_ms = 10);
int     dagent_send(const char * dst, int type, const msg_buffer_t & msg);
int     dagent_cb_push(int type, dagent_cb_t cb);
int     dagent_cb_pop(int type);

//python file load
int     dagent_load_plugin(const char * file);

void    dagent_export_python(bool for_ext);