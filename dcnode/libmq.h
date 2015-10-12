#pragma once

#include "stdinc.h"

struct smq_t;


struct smq_config_t
{
    const char * key;
    int   buffsz;
};
struct smq_msg_t
{
    const char * buffer;
    int     sz;
};

typedef int (*smq_msg_cb_t)(smq_t * , const smq_msg_t & msg);

smq_t * smq_create(const smg_config_t & conf);
void    smq_destroy(smq_t*);
void    smq_msg_cb(smq_t *, smq_msg_cb_t cb);
void    smq_poll(smq_t*, int timeout_us);
int     smq_send(smq_t*,uint64_t dst, const smq_msg_t & msg);

//for debug
//status report

