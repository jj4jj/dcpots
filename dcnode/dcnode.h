#pragma once
#include "stdinc.h"
#include "proto/dcnode.pb.h"
struct dcnode_t;

struct dcnode_msg_t : public dcnode::MsgDCNode;


struct dcnode_addr_t
{
	string msgq_key;  //key:r -> recv key:s -> send
	string listen_addr; //listen ip:port
	string parent_addr;  //parent ip:port
};

struct dcnode_config_t
{
    dcnode_addr_t  addr; //parent tcp and msgq
    int max_channel_buff_size;//
    int heart_beat_gap;//seconds
    int max_register_children;//max children
	string	name;
};

dcnode_t* dcnode_create(const dcnode_config_t & conf);
void      dcnode_destroy(dcnode_t* dc);
void      dcnode_update(dcnode_t*, const timeval & tv,int timout_us);

typedef int(*dcnode_dispatcher_t)(void * ud, const dcnode_msg_t & msg);

void      dcnode_set_dispatcher(dcnode_t*, dcnode_dispatcher_t);
int      dcnode_send(dcnode_t*, const char * dst, const char * buff, int sz);

