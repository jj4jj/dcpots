#pragma once
#include "stdinc.h"

struct dcnode_t;

#pragma pack(1)
struct dcnode_msg_t
{
	enum
	{
		MSG_REG_NAME = 1,
		MSG_HEART_BEAT = 2,
		MSG_DATA = 3,
	};
	struct
	{
		int32_t		length;
		char		src[32];
		char		dst[32];
		uint8_t		type; //register name , data , heart beat ,
	} head;
	union {
		char	data[1];
	} ext;
	//return total size
	int pack(char * buff, int sz, const char* payload = nullptr, int payloadsz = 0)
	{
		if (sz < sizeof(head)+payloadsz)
		{
			//error size
			return -1;
		}
		int ext_size = 0;
		//ext size



		if (sz < sizeof(head)+payloadsz + ext_size)
		{
			//error size
			return -2;
		}

		dcnode_msg_t * net_msg = (dcnode_msg_t*)buff;
		net_msg->head = head;
		net_msg->head.length = htonl(head.length);
		memcpy(net_msg + sizeof(head)+ext_size, payload, payloadsz);
		return 0;
	}
	//return payload size
	int unpack(const char * buff, int sz)
	{
		if (sz < sizeof(head))
		{
			//error size
			return -1;
		}
		int payload_sz = sz;
		dcnode_msg_t * net_msg = (dcnode_msg_t*)buff;
		memcpy(&head, buff, sizeof(head));
		head.length = ntohl(net_msg->length);
		int ext_size = 0;
		switch(net_msg->type)
		{
		case MSG_REG_NAME:
			break;
		case MSG_HEART_BEAT:
			break;
		case MSG_DATA:
			break;
		default:
			//error type
			return -1;
			break;
		}
		if (head.length != sz)
		{
			//error sz
			return -2;
		}
		return sz - sizeof(head) - ext_size;
	}
	
};
#pragma pack()


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

typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t*, dcnode_dispatcher_t);

int      dcnode_listen(dcnode_t*, const dcnode_addr_t & addr);
int      dcnode_connect(dcnode_t*, const dcnode_addr_t & addr);
int      dcnode_send(dcnode_t*, const char * dst, const dcnode_msg_t & msg);

