#pragma once
#include "stdinc.h"

struct dctcp_msg_t
{
    const char * buff;
    int   buff_sz;
	dctcp_msg_t(const char * buf, int sz) :buff(buf), buff_sz(sz){}
};

struct dctcp_addr_t
{
    string ip;
    int  port;
	dctcp_addr_t() :ip("0.0.0.0"), port(0) {}
	uint32_t u32ip() const
	{
		return inet_addr(ip.c_str());
	}
};
struct dctcp_config_t
{
    int is_server; //0:client, 1:server
    int max_recv_buff;
    int max_send_buff;
	int max_backlog;
	int max_client;
	int max_tcp_send_buff_size;
	int max_tcp_recv_buff_size;
    dctcp_addr_t listen_addr;
	dctcp_config_t()
	{
		max_client = 8192;
		max_backlog = 2048;
		max_recv_buff = max_send_buff = 1024 * 100; //100K
		max_tcp_send_buff_size = 1024 * 100; //100K
		max_tcp_recv_buff_size = 640 * 100; //600K
		is_server = 0;
	}
};

struct dctcp_t;
enum dctcp_close_reason_type
{
	STCP_MSG_OK = 0, //OK
	STCP_MSG_ERR = 1,	//msg error
	STCP_CONNECT_ERR = 2, //connect
	STCP_PEER_CLOSE = 3,
	STCP_POLL_ERR = 4, //refer to errno
	STCP_INVAL_CALL = 5, //usage err
	STCP_SYS_ERR = 6, //system call error refer to errno
	STCP_CLOSE_ACTIVE = 7, //by uplayer
};

enum dctcp_event_type
{
	STCP_EVT_INIT = 0,
    STCP_CONNECTED = 1,
	STCP_NEW_CONNX,
	STCP_CLOSED ,
    STCP_READ ,
    STCP_WRITE ,
    STCP_EVENT_MAX
};

struct dctcp_event_t
{
    dctcp_event_type		type;
	int					fd;
    const dctcp_msg_t *  msg;
	dctcp_close_reason_type		reason;
	int							error;
	dctcp_event_t() :type(STCP_EVT_INIT), msg(nullptr), reason(STCP_MSG_OK), error(0){}
};

typedef int (*dctcp_event_cb_t)(dctcp_t*, const dctcp_event_t & ev, void * ud);

struct dctcp_t * dctcp_create(const dctcp_config_t & conf);
void            dctcp_destroy(dctcp_t * );
void            dctcp_event_cb(dctcp_t*, dctcp_event_cb_t cb, void *ud);
//return proced
int	            dctcp_poll(dctcp_t *, int timeout_us, int max_proc = 100);
int				dctcp_send(dctcp_t *,int fd, const dctcp_msg_t & msg);
int             dctcp_connect(dctcp_t *, const dctcp_addr_t & addr, int retry = 0);
int				dctcp_reconnect(dctcp_t* , int fd);
void			dctcp_close(dctcp_t *, int fd);
bool            dctcp_is_server(dctcp_t *);


