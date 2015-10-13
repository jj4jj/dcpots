#pragma once
#include "stdinc.h"

struct stcp_msg_t
{
    const char * buff;
    int   buff_sz;
	stcp_msg_t(const char * buf, int sz) :buff(buf), buff_sz(sz){}
};

struct stcp_addr_t
{
    string ip;
    int  port;
};
struct stcp_config_t
{
    int is_server; //0:client, 1:server
    int max_recv_buff;
    int max_send_buff;
	int max_backlog;
	int max_client;
	int max_tcp_send_buff_size;
	int max_tcp_recv_buff_size;
    stcp_addr_t listen_addr;
};

struct stcp_t;

enum stcp_event_type
{
    STCP_CONNECTED = 0, //client :connected, server:new connection
    STCP_CLOSED ,
    STCP_READ ,
    STCP_WRITE ,
	STCP_CONNECT_ERROR,
	STCP_DATA_ERR,
    STCP_EVENT_MAX
};

struct stcp_event_t
{
    stcp_event_type		type;
	int					fd;
    const stcp_msg_t *  msg;
};

typedef int (*stcp_event_cb_t)(stcp_t*, const stcp_event_t & ev, void * ud);

struct stcp_t * stcp_create(const stcp_config_t & conf);
void            stcp_destroy(stcp_t * );
void            stcp_event_cb(stcp_t*, stcp_event_cb_t cb£¬void * ud);
void            stcp_poll(stcp_t *,int timeout_us);
int				stcp_send(stcp_t *,int fd, const stcp_msg_t & msg);
int             stcp_connect(stcp_t *, const stcp_addr_t & addr, int retry = 0);
int				stcp_reconnect(stcp_t*);
bool            stcp_is_server(stcp_t *);

