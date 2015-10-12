#pragma once
#include "stdinc.h"

struct dcnode_t;
struct dcnode_msg_t;

struct dcnode_addr_t
{
    char msgq_key[32];  //key:r -> recv key:s -> send
    char tcp_addr[48]; //ip:port
};

struct dcnode_config_t
{
    dcnode_addr_t  listen_addr; //msgq recv and tcp recv
    dcnode_addr_t  parent_addr; //parent tcp recv and msgq recv
    int max_channel_buff_size;//
    int heart_beat_gap;//seconds
    int max_register_children;//max children
};

dcnode_t* dcnode_create(const dcnode_config_t & conf);
void      dcnode_destroy(dcnode_t* dc);
void      dcnode_update(dcnode_t*, const time_val tv,int timout_us);

typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t*, dcnode_dispatcher_t);

int      dcnode_listen(dcnode_t*, const dcnode_addr_t & addr);
int      dcnode_connect(dcnode_t*, const dcnode_addr_t & addr);
int      dcnode_send(dcnode_t*, const char * dst, const dcnode_msg_t & msg, bool dst_islocal);

