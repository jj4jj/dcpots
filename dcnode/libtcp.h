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
	stcp_addr_t() :ip("0.0.0.0"), port(0) {}
	uint32_t u32ip() const
	{
		return inet_addr(ip.c_str());
	}
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
	stcp_config_t()
	{
		max_client = 8192;
		max_backlog = 2048;
		max_recv_buff = max_send_buff = 1024 * 100; //100K
		max_tcp_send_buff_size = 1024 * 100; //100K
		max_tcp_recv_buff_size = 640 * 100; //600K
		is_server = 0;
	}
};

struct stcp_t;
enum stcp_close_reason_type
{
	STCP_MSG_ERR = 1,	//msg error
	STCP_CONNECT_ERR = 2, //connect
	STCP_PEER_CLOSE = 3,
	STCP_POLL_ERR = 4,
	STCP_INVAL_CALL = 5, //usage err
	STCP_SYS_ERR = 6, //system call error refer to errno
};

enum stcp_event_type
{
	STCP_EVT_INIT = 0,
    STCP_CONNECTED = 1,
	STCP_NEW_CONNX,
	STCP_CLOSED ,
    STCP_READ ,
    STCP_WRITE ,
    STCP_EVENT_MAX
};

struct stcp_event_t
{
    stcp_event_type		type;
	int					fd;
    const stcp_msg_t *  msg;
	stcp_close_reason_type		reason;
	stcp_event_t() :type(STCP_EVT_INIT), msg(nullptr){}
};

typedef int (*stcp_event_cb_t)(stcp_t*, const stcp_event_t & ev, void * ud);

struct stcp_t * stcp_create(const stcp_config_t & conf);
void            stcp_destroy(stcp_t * );
void            stcp_event_cb(stcp_t*, stcp_event_cb_t cb, void *ud);
//return proced
int	            stcp_poll(stcp_t *, int timeout_us, int max_proc = 100);
int				stcp_send(stcp_t *,int fd, const stcp_msg_t & msg);
int             stcp_connect(stcp_t *, const stcp_addr_t & addr, int retry = 0);
int				stcp_reconnect(stcp_t* , int fd);
bool            stcp_is_server(stcp_t *);

