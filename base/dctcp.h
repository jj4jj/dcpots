#pragma once
#include <string>

struct dctcp_msg_t {
    const char * buff{ nullptr };
    int   buff_sz{ 0 };
    dctcp_msg_t(const char * buf, int sz = 0);
};
struct dctcp_config_t {
    int max_recv_buff;
    int max_send_buff;
	int max_backlog;
	int max_client;
	int max_tcp_send_buff_size;
	int max_tcp_recv_buff_size;
	dctcp_config_t();
};

enum dctcp_close_reason_type {
	DCTCP_MSG_OK = 0, //OK
	DCTCP_MSG_ERR = 1,	//msg error
	DCTCP_CONNECT_ERR = 2, //connect
	DCTCP_PEER_CLOSE = 3, //peer close it
	DCTCP_POLL_ERR = 4, //refer to errno
    DCTCP_CONNX_HUP = 5, //hup or error(refuse)
	DCTCP_SYS_CALL_ERR = 6, //system call error refer to errno
	DCTCP_CLOSE_ACTIVE = 7, //by uplayer (user)
};

enum dctcp_event_type {
	DCTCP_INIT = 0,
    DCTCP_CONNECTED = 1,
	DCTCP_NEW_CONNX,
	DCTCP_CLOSED ,
    DCTCP_READ ,
    DCTCP_EVENT_MAX
};
struct sockaddr;
struct dctcp_event_t {
    dctcp_event_type			type;
    int							listenfd{ -1 };//for new connection host fd
    int							fd{ -1 };//event fd
    const dctcp_msg_t *			msg;
	dctcp_close_reason_type		reason { DCTCP_MSG_OK};
	int							error {0};
    struct sockaddr         *   netaddr {nullptr};
	dctcp_event_t();
};

struct dctcp_t;
struct msg_buffer_t;
struct dctcp_msg_codec_t {
    //encode 0:success, otherwise error
    typedef int(*encode_func_t)(msg_buffer_t & buff, const msg_buffer_t & msg, void * ud);
    //return >0: success, return the costed buffer msg length , = 0, need more, < 0 error.
    typedef int(*decode_func_t)(msg_buffer_t & msg, const msg_buffer_t & buff, void * ud);
    ///////////////////////////////////////////////////////////////////////////////////////
    encode_func_t   encode;
    decode_func_t   decode;
};
typedef int(*dctcp_event_cb_t)(dctcp_t*, const dctcp_event_t & ev, void * ud);
////////////////////////////////////////////////////////////////////////////
dctcp_t *           dctcp_default_pump();
dctcp_t *	        dctcp_create(const dctcp_config_t & conf);
void				dctcp_destroy(dctcp_t * );
void				dctcp_event_cb(dctcp_t*, dctcp_event_cb_t cb, void *ud);
void                dctcp_codec(dctcp_t *, const dctcp_msg_codec_t * codec, void * codec_ud);
//return proced events
int					dctcp_poll(dctcp_t *, int interval_us, int max_proc = 100);
//fproto:msg:sz32,msg:sz16,msg:sz8,token:xxx,pack,
int					dctcp_listen(dctcp_t *, const std::string & addr, const char * fproto = "msg:sz32",
                                 dctcp_event_cb_t listener = nullptr, void * listener_ud = nullptr,
                                 const dctcp_msg_codec_t * codec = nullptr, void * codec_ud = nullptr); //return a fd >= 0when success
int					dctcp_connect(dctcp_t *, const std::string & addr, int retry = 0, const char * fproto = "msg:sz32" ,
                                  dctcp_event_cb_t connector = nullptr, void * connector_ud = nullptr,
                                  const dctcp_msg_codec_t * codec = nullptr, void * codec_ud = nullptr);
int					dctcp_send(dctcp_t *, int fd, const dctcp_msg_t & msg);
void				dctcp_close(dctcp_t *, int fd);

