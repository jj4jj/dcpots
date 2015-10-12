#pragma once


#include "dcnode/dcnode.h"
#include "proto/dagent.pb.h"

struct dagent_msg_t : public dagent::MsgDAgent;

struct dagent_config_t
{
    dcnode_config_t node_conf;
    char            plugin_start_file[32];
};

typedef int (*dagent_cb_t)(const dagent_msg_t &  msg);

int     dagent_init(const dagent_config_t & conf);
void    dagent_destroy();
void    dagent_update();
int     dagent_send(const char * dst, const dagent_msg_t & msg);
int     dagent_cb_push(int type, dagent_cb_t cb);
int     dagent_cb_pop(int type);

//reg python file
int     dagent_init_plugins();
int     dagent_destroy_plugins();
int     dagent_reload_plugins();

