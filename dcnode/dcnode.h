#pragma once
#include "stdinc.h"
#include "proto/dcnode.pb.h"
#include "msg_proto.hpp"
#include "error_msg.h"

struct dcnode_t;

typedef msgproto_t<dcnode::MsgDCNode>	dcnode_msg_t;

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
	dcnode_config_t()
	{
		name = "noname";
		max_register_children = 100;
		heart_beat_gap = 60;
		max_channel_buff_size = 1024 * 1024;
		addr.listen_addr = "";
		addr.parent_addr = "";
		addr.msgq_key = "";
	}
};

typedef	std::function<void()>	dcnode_timer_callback_t;
typedef int(*dcnode_dispatcher_t)(void * ud, const dcnode_msg_t & msg);

dcnode_t* dcnode_create(const dcnode_config_t & conf);
void      dcnode_destroy(dcnode_t* dc);
void      dcnode_update(dcnode_t*, int timout_us);
uint64_t  dcnode_timer_add(dcnode_t * ,int delayms , dcnode_timer_callback_t cb);
void	  dcnode_timer_cancel(dcnode_t *, uint64_t cookie);

void      dcnode_set_dispatcher(dcnode_t*, dcnode_dispatcher_t, void* ud);
int       dcnode_send(dcnode_t*, const char * dst, const char * buff, int sz);

error_msg_t * dcnode_error(dcnode_t *);
