#pragma once

#include "stdinc.h"

struct smq_t;

struct smq_config_t
{
    string			key;
    int				msg_buffsz;
	int				max_queue_buff_size;
	bool			server_mode;
	bool			attach;
	smq_config_t()
	{
		msg_buffsz = 1024 * 1024;
		max_queue_buff_size = 10 * 1024 * 1024; //2MB
		server_mode = false;
		attach = false;
	}
};

struct smq_msg_t
{
    const char * buffer;
    int			 sz;
	smq_msg_t(const char * buf, int s) :buffer(buf), sz(s){}
};

typedef int (*smq_msg_cb_t)(smq_t * , uint64_t src, const smq_msg_t & msg, void * ud);
smq_t * smq_create(const smq_config_t & conf);
void    smq_destroy(smq_t*);
void    smq_msg_cb(smq_t *, smq_msg_cb_t cb, void * ud);
void    smq_poll(smq_t*, int timeout_us);
int     smq_send(smq_t*,uint64_t dst, const smq_msg_t & msg);
bool	smq_server_mode(smq_t *);

//for debug
//status report

