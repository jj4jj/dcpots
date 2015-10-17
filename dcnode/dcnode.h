#pragma once
//1. swap name with parent (agent),  for address collision 
//2. leaf (client) will reg its name in local machine .
//3. parent will known all children name .
//4. parent can forward msg to others (or lookup name table)

/*
				F
			E	4
		A		B		C		D
	1	2	3

A(1,2,3)
if 1 not ready
	it has no privilege send msg_data(BUS MSG)
else
	A(1,2,3) will be a circle node

if A not ready
	A has no privilege send msg_data
	but can forward msg to E or its children 2,3  (-1)
else
	E(A,B,C,D) is a circle node

if E not ready
	no privilege send msg_data
	but can forward msg to F(parent) and (B,C,D) (-A)
----------------------------------------------------------------
NODE.is_not_ready => dcnode_send maybe error [if dst is knowned].


*/
#include "stdinc.h"
#include "proto/dcnode.pb.h"
#include "msg_proto.hpp"
#include "error_msg.h"

struct dcnode_t;



typedef msgproto_t<dcnode::MsgDCNode>	dcnode_msg_t;

struct dcnode_addr_t
{
	string msgq_path;  //file path for machine sharing
	bool   msgq_push; //client push mode as a client , not server
	string listen_addr; //listen ip:port
	string parent_addr;  //parent ip:port
	dcnode_addr_t() :msgq_push(true){}
};

struct dcnode_config_t
{
    dcnode_addr_t  addr; //parent tcp and msgq
    int max_channel_buff_size;//
    int heart_beat_gap;//seconds
    int max_register_children;//max children
	int max_expired_time; //expired time s
	int max_live_heart_beat_gap; //expire time for close -> 5*max_expire
	string	name;
	dcnode_config_t()
	{
		name = "noname";
		max_register_children = 11024;
		heart_beat_gap = 60;
		max_channel_buff_size = 1024 * 1024;
		addr.listen_addr = "";
		addr.parent_addr = "";
		addr.msgq_path = "";
		max_expired_time = 30;
		max_live_heart_beat_gap = 5 * max_expired_time;
	}
};

typedef	std::function<void()>	dcnode_timer_callback_t;
typedef int(*dcnode_dispatcher_t)(void * ud, const dcnode_msg_t & msg);

dcnode_t* dcnode_create(const dcnode_config_t & conf);
void      dcnode_destroy(dcnode_t* dc);
void      dcnode_update(dcnode_t*, int timout_us);

uint64_t  dcnode_timer_add(dcnode_t * ,int delayms , dcnode_timer_callback_t cb, bool repeat = false);
void	  dcnode_timer_cancel(dcnode_t *, uint64_t cookie);

void      dcnode_set_dispatcher(dcnode_t*, dcnode_dispatcher_t, void* ud);
int       dcnode_send(dcnode_t*, const char * dst, const char * buff, int sz);

error_msg_t * dcnode_error(dcnode_t *);
bool		  dcnode_stoped(dcnode_t *);