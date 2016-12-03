#include "dctcp.h"
#include "logger.h"
#include "msg_proto.hpp"
#include "profile.h"
#include "msg_codec.h"

dctcp_msg_t::dctcp_msg_t(const char * buf, int sz) :buff(buf), buff_sz(sz){
	if (!sz){buff_sz = strlen(buf);}
}
dctcp_config_t::dctcp_config_t() {
	max_client = 8192;
	max_backlog = 2048;
	max_recv_buff = max_send_buff = 1024 * 100; //100K
	max_tcp_send_buff_size = 1024 * 100; //100K
	max_tcp_recv_buff_size = 640 * 100; //600K
}
dctcp_event_t::dctcp_event_t() :type(DCTCP_INIT), listenfd(-1), fd(-1), 
    msg(nullptr), reason(DCTCP_MSG_OK), error(0){
}

//client connecting
struct dctcp_connector_t {
	int					reconnect;
	int					max_reconnect;
	sockaddr_in 		connect_addr;
	dctcp_connector_t() {
		memset(this, 0, sizeof(*this));
	}
};
struct dctcp_listener_t {
	sockaddr_in			listenaddr;
	dctcp_listener_t(){
		memset(this, 0, sizeof(*this));
	}
};
#define MAX_DECODE_BUFFER_SIZE  (2*1024*1024)
struct dctcp_proto_dispatcher_t {
    dctcp_msg_codec_t   codec {nullptr, nullptr};
	std::string			param_s;
	dctcp_event_cb_t	event_cb{ nullptr };
	void		*		event_cb_ud{ nullptr };
    msg_buffer_t        buffer;
	bool operator == (const dctcp_proto_dispatcher_t & rhs_) const {
		return this->codec.encode == rhs_.codec.encode &&
			this->codec.decode == rhs_.codec.decode &&
			this->param_s == rhs_.param_s &&
			this->event_cb == rhs_.event_cb &&
			this->event_cb_ud == rhs_.event_cb_ud;
	}
};
struct dctcp_listener_env_t {
	typedef std::unordered_map<int, dctcp_proto_dispatcher_t*>	dctcp_event_listener_t;
	std::vector<dctcp_proto_dispatcher_t>	dispatcher_allocator;	
	dctcp_proto_dispatcher_t		base_listener;
	dctcp_event_listener_t			cust_listener;
};
struct dctcp_t {
    dctcp_config_t	conf;
    int				epfd{ -1 };	//epfd for poller
	std::unordered_map<int, dctcp_connector_t>		connectors;//client connecting
	std::unordered_map<int, dctcp_listener_t>		listeners;//server listeners fd
	///////////////////////////////////////////////////////////////
	dctcp_listener_env_t							proto_listeners;
    std::unordered_map<int, int>					fd_map_listenfd;
    ///////////////////////////////////////////////////////////////
    epoll_event	*	events{ nullptr };
    int				nproc{ 0 };
    int				nevts{ 0 };
    uint64_t        next_evt_poll_time{ 0 };
	//////////////////////////////fd<->send and recv//////////////
	std::unordered_map<int, msg_buffer_t>	sock_recv_buffer;
	std::unordered_map<int, msg_buffer_t>	sock_send_buffer;
	///////////////////////////////////////////////////////////////
	msg_buffer_t	misc_buffer;
	dctcp_t(){
		init();
	}
	void init(){
		epfd = -1;
		events = nullptr;
		nevts = nproc = 0;
		sock_recv_buffer.clear();
		sock_send_buffer.clear();
		connectors.clear();
		listeners.clear();
		misc_buffer.destroy();
	}
};

static inline int _set_socket_opt(int fd, int name, void * val, socklen_t len){
	int lv = SOL_SOCKET;
	if (name == TCP_NODELAY) lv = IPPROTO_TCP;
	return setsockopt(fd, lv, name, val, len);
}

static inline int _set_socket_ctl(int fd, int flag, bool open){
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0){
		return -1;
	}
	if (open){
		flags |= flag;
	}
	else{
		flags &= ~(flag);
	}
	if (fcntl(fd, F_SETFL, flags) < 0){
		return -1;
	}
	return 0;
}
static inline  int 	_init_socket_options(int fd, int sendbuffsize,int recvbuffsize){
	int on = 1;
	int ret = _set_socket_opt(fd, TCP_NODELAY, &on, sizeof(on));
	ret |= _set_socket_opt(fd, SO_REUSEADDR, &on, sizeof(on));
	size_t buffsz = recvbuffsize;
	ret |= _set_socket_opt(fd, SO_RCVBUF, &buffsz, sizeof(buffsz));
	buffsz = sendbuffsize;
	ret |= _set_socket_opt(fd, SO_SNDBUF, &buffsz, sizeof(buffsz));
	ret |= _set_socket_ctl(fd, O_NONBLOCK, true); //nblock
	if (ret != 0){
		close(fd);
		return ret;
	}
	return 0;
}
static inline int _create_tcpsocket(int sendbuffsize, int recvbuffsize){
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return -1;
	int ret = _init_socket_options(fd , sendbuffsize, recvbuffsize);
	if (ret) return -2;
	return fd;
}
static dctcp_t * s_default_pump = nullptr;
dctcp_t *    dctcp_default_pump() {
    if (s_default_pump) {
        return s_default_pump;
    }
    else {
        dctcp_config_t tcpconf;
        tcpconf.max_recv_buff = 32 * 1024;
        tcpconf.max_send_buff = 64 * 1024;
        tcpconf.max_client = 40960;
        tcpconf.max_tcp_send_buff_size = 64 * 1024;
        tcpconf.max_tcp_recv_buff_size = 16 * 1024;
        return dctcp_create(tcpconf);
    }
}

dctcp_t * dctcp_create(const dctcp_config_t & conf){
	dctcp_t * stcp = new dctcp_t();
	stcp->conf = conf;
	stcp->events = (epoll_event *)malloc(conf.max_client * sizeof(epoll_event));
	if (!stcp->events) { dctcp_destroy(stcp); return nullptr; }

	stcp->epfd = epoll_create(conf.max_client);
	if (stcp->epfd < 0) { dctcp_destroy(stcp); return nullptr; }

	stcp->misc_buffer.create(1024 * 1024);
	return stcp;
}
static inline	void	_close_listeners(dctcp_t * stcp){
	for (auto & it : stcp->listeners) close(it.first);
}
static inline	void	_close_connections(dctcp_t * stcp){
	for (auto & it : stcp->connectors) close(it.first);
}
void            dctcp_destroy(dctcp_t * stcp){
	GLOG_TRA("stcp destroy ....");
	_close_listeners(stcp);
	_close_connections(stcp);
	if (stcp->epfd >= 0) close(stcp->epfd);
	if (stcp->events) free(stcp->events);
	stcp->init();
	delete stcp;
}
static inline int _dctcp_get_fd_listenfd(dctcp_t * stcp, int fd){
	auto it = stcp->fd_map_listenfd.find(fd);
	if (it == stcp->fd_map_listenfd.end()){
		return -1;
	}
	return it->second;
}
static inline dctcp_proto_dispatcher_t * _dctcp_get_proto_dispatcher(dctcp_t * stcp, int fd, int listenfd){
	int efd = -1;
	if (listenfd != -1){
		efd = listenfd;
	}
	else if (fd != -1) {
		efd = fd;
	}
	else {
		assert("dctcp event fd or listenfd error !" && false);
	}
	auto it = stcp->proto_listeners.cust_listener.find(efd);
	if (it != stcp->proto_listeners.cust_listener.end()){
		return it->second;
	}
	return &stcp->proto_listeners.base_listener;
}
static	inline void	dctcp_event_dispatch(dctcp_t *stcp, const dctcp_event_t & ev, 
		dctcp_proto_dispatcher_t * proto_env = nullptr){
	if (proto_env){ //settings
		if (proto_env->event_cb){
			proto_env->event_cb(stcp, ev, proto_env->event_cb_ud);
		}
	}
	else { // no settings
		dctcp_proto_dispatcher_t * proto_env = _dctcp_get_proto_dispatcher(stcp, ev.fd, ev.listenfd);
		if (proto_env && proto_env->event_cb){
			proto_env->event_cb(stcp, ev, proto_env->event_cb_ud);
		}
	}
}
void            dctcp_event_cb(dctcp_t *stcp, dctcp_event_cb_t cb, void * ud){
	stcp->proto_listeners.base_listener.event_cb = cb;
	stcp->proto_listeners.base_listener.event_cb_ud = ud;
}
static inline int _op_poll(dctcp_t * stcp, int cmd, int fd, int flag = 0, int listenfd = -1){
	epoll_event ev;
	ev.data.u64 = (uint32_t)listenfd;
	ev.data.u64 <<= 32;
	ev.data.fd = (uint32_t)fd;
	ev.events = flag;
	return epoll_ctl(stcp->epfd, cmd, fd, &ev);
}
//////////////////////////////////////////////////////////////////////////
#define DEF_DEFAULT_CODEC_MSG_SIZE(msgszb)	\
static int _codec_msg_size_##msgszb##_encode(msg_buffer_t & buff, const msg_buffer_t & msg) {return dcs::codec_msg_size_encode(msgszb/8, buff, msg);}\
static int _codec_msg_size_##msgszb##_decode(msg_buffer_t & msg, const msg_buffer_t & buff) {return dcs::codec_msg_size_decode(msgszb/8, msg, buff);}
DEF_DEFAULT_CODEC_MSG_SIZE(8)
DEF_DEFAULT_CODEC_MSG_SIZE(16)
DEF_DEFAULT_CODEC_MSG_SIZE(32)
//////////////////////////////////////////////////////////////////////////
static inline void _dispatcher_codec_init(dctcp_proto_dispatcher_t * dispatcher, const char * fproto,
									const dctcp_msg_codec_t * codec){
	if (!strcasecmp(fproto, "msg:sz32")){
		dispatcher->codec.encode = _codec_msg_size_32_encode;
		dispatcher->codec.decode = _codec_msg_size_32_decode;
	}
	else if (!strcasecmp(fproto, "msg:sz16")){
		dispatcher->codec.encode = _codec_msg_size_16_encode;
		dispatcher->codec.decode = _codec_msg_size_16_decode;
	}
	else if (!strcasecmp(fproto, "msg:sz8")){
		dispatcher->codec.encode = _codec_msg_size_8_encode;
		dispatcher->codec.decode = _codec_msg_size_8_decode;
	}
	else if (fproto == strstr(fproto, "token:") && fproto[6]!=0 ){
		dispatcher->param_s = fproto+6;
	}
    else if (!strcasecmp(fproto, "codec")){
		assert(codec && "must define a codec ");
		dispatcher->codec = *codec;
    }
	else {
		assert(false && "error codec !");
	}
}

static inline void _fd_dettach_codec_dispatcher(dctcp_t * stcp, int fd) {
	auto it = stcp->proto_listeners.cust_listener.find(fd);
	if (it != stcp->proto_listeners.cust_listener.end()) {
		//no need free the codec
		stcp->proto_listeners.cust_listener.erase(it);
	}
}

static inline void _fd_attach_codec_dispatcher(dctcp_t * stcp, int fd, const char * fproto, dctcp_event_cb_t cb, void * ud, const dctcp_msg_codec_t * codec) {
	dctcp_proto_dispatcher_t codec_dispatcher;
	_dispatcher_codec_init(&codec_dispatcher, fproto, codec);
	codec_dispatcher.event_cb = cb;
	codec_dispatcher.event_cb_ud = ud;
	auto it = std::find(stcp->proto_listeners.dispatcher_allocator.begin(),
		stcp->proto_listeners.dispatcher_allocator.end(), codec_dispatcher);
	if (it == stcp->proto_listeners.dispatcher_allocator.end()) {
		stcp->proto_listeners.dispatcher_allocator.push_back(codec_dispatcher);
		dctcp_proto_dispatcher_t & cbenv = stcp->proto_listeners.dispatcher_allocator.back();
		cbenv.buffer.reserve(MAX_DECODE_BUFFER_SIZE);
		stcp->proto_listeners.cust_listener[fd] = &cbenv;
	}
}
static inline void _add_listenner(dctcp_t * stcp, int fd, const sockaddr_in & addr,
	const char * fproto, dctcp_event_cb_t cb, void * ud, const dctcp_msg_codec_t * codec){
	assert(fd >= 0);
	assert(stcp->listeners.find(fd) == stcp->listeners.end());
	dctcp_listener_t listener;
	listener.listenaddr = addr;
	stcp->listeners[fd] = listener;
	///////////////////////////////////////////////////////////////////
	_fd_attach_codec_dispatcher(stcp, fd, fproto, cb, ud, codec);
}
static inline dctcp_connector_t & _add_connector(dctcp_t * stcp, int fd, const sockaddr_in & saddr, int retry,
	const char * fproto, dctcp_event_cb_t cb, void * ud, const dctcp_msg_codec_t * codec){
	assert(fd >= 0);
    dctcp_connector_t cnx;
    cnx.max_reconnect = retry;
    cnx.reconnect = 0;
    cnx.connect_addr = saddr;
    stcp->connectors[fd] = cnx;
	_fd_attach_codec_dispatcher(stcp, fd, fproto, cb, ud, codec);
    return stcp->connectors[fd];
}
static inline int _get_sockerror(int fd){
	int error = 0;
	socklen_t len = sizeof(int);
	int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == 0)
		return error;
	else 
		return errno;
}
static inline bool  _is_fd_ok(int fd){
    return fcntl(fd, F_GETFL) != -1;
}
static inline void _free_sock_msg_buffer(dctcp_t * stcp, int fd){
	auto it = stcp->sock_recv_buffer.find(fd);
	if (it != stcp->sock_recv_buffer.end()){
		GLOG_TRA("free recv buffer ....fd:%d", fd);
		it->second.destroy();
		stcp->sock_recv_buffer.erase(it);
	}
	it = stcp->sock_send_buffer.find(fd);
	if (it != stcp->sock_send_buffer.end()){
		GLOG_TRA("free send buffer ....fd:%d", fd);
		it->second.destroy();
		stcp->sock_send_buffer.erase(it);
	}
}
static inline msg_buffer_t * _get_sock_msg_buffer(dctcp_t * stcp, int fd, bool for_recv){
	if (for_recv){
		auto it = stcp->sock_recv_buffer.find(fd);
		if (it == stcp->sock_recv_buffer.end()){
			msg_buffer_t buf;
			if (buf.create(stcp->conf.max_recv_buff + 8)){
				return nullptr;
			}
			stcp->sock_recv_buffer[fd] = buf;
			return &(stcp->sock_recv_buffer[fd]);
		}
		return &(it->second);
	}
	else{
		auto it = stcp->sock_send_buffer.find(fd);
		if (it == stcp->sock_send_buffer.end()){
			msg_buffer_t	buf;
            if (buf.create(stcp->conf.max_send_buff + 8)){
				return nullptr;
			}
			stcp->sock_send_buffer[fd] = buf;
			return &(stcp->sock_send_buffer[fd]);
		}
		return &(it->second);
	}
}
static inline void	_close_fd(dctcp_t * stcp, int fd, dctcp_close_reason_type reason, int listenfd){
    GLOG_TRA("close fd:%d for reason:%d errno:%d error:%s", fd, reason, errno, strerror(errno));
	int error = _get_sockerror(fd);
	_op_poll(stcp, EPOLL_CTL_DEL, fd);
	close(fd);
	//connecting ?	//listener ?
	if (stcp->connectors.find(fd) != stcp->connectors.end()){
		stcp->connectors.erase(fd);
	}
	else if (stcp->listeners.find(fd) != stcp->listeners.end()){
		stcp->listeners.erase(fd);
	}
	_free_sock_msg_buffer(stcp, fd);
	//just notify
	dctcp_event_t  sev;
	sev.type = dctcp_event_type::DCTCP_CLOSED;
	sev.fd = fd;
	sev.listenfd = listenfd;
	sev.reason = reason;
	sev.error = error;
	dctcp_event_dispatch(stcp, sev);
	_fd_dettach_codec_dispatcher(stcp, fd);
	stcp->fd_map_listenfd.erase(fd);
}

static inline void _new_connx(dctcp_t * stcp, int listenfd){
	struct sockaddr	addr;
	socklen_t	len = sizeof(addr);
    ACCEPT_:
	int nfd = accept(listenfd, &addr, &len);
	if (nfd >= 0){
        stcp->fd_map_listenfd[nfd] = listenfd;
		dctcp_event_t sev;
		sev.listenfd = listenfd;
		sev.fd = nfd;
		sev.type = dctcp_event_type::DCTCP_NEW_CONNX;
		_init_socket_options(nfd, stcp->conf.max_tcp_send_buff_size, stcp->conf.max_tcp_recv_buff_size);
		//add epollin
		_op_poll(stcp, EPOLL_CTL_ADD, nfd, EPOLLIN, listenfd);
		dctcp_event_dispatch(stcp, sev);
	}
	else{		//error
        if (errno == EINTR){
            goto ACCEPT_;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK){
            GLOG_SER("accept error listen fd:%d !", listenfd);
        }
	}
}
static inline int _dctcp_proto_msg_sz_length(int hsz, const char * buffer, int ibuff){
    if (ibuff < hsz){
        return 0;
    }
	int length = 0;
	switch (hsz){
	case sizeof(uint8_t) :
		length = *(uint8_t*)buffer;
		break;
	case sizeof(uint16_t) :
		length = ntohs(*(uint16_t*)buffer);
		break;
	case sizeof(uint32_t) :
		length = ntohl(*(uint32_t*)buffer);
		break;
	default:
		assert("error dctcp hsz !" && false);
		break;
	}
	return length;
}
static inline int _dctcp_proto_dispatch_msg_sz(dctcp_t * stcp, msg_buffer_t * buffer,
	int fd, int listenfd, dctcp_proto_dispatcher_t * proto_env, int hsz){
	dctcp_event_t	sev;
	sev.type = DCTCP_READ;
	sev.fd = fd;
	sev.listenfd = listenfd;
	int nproc = 0;//proc msg num
	//need dispatching
	//////////////////////////////////////////////////
    int msg_buff_start = 0;
    int msg_buff_rest = buffer->valid_size;
    int msg_buff_total = buffer->valid_size;
    int msg_length = _dctcp_proto_msg_sz_length(hsz, buffer->buffer, msg_buff_rest);
	while (msg_length > hsz) {
		if (msg_length > buffer->max_size){
			//errror msg , too big 
			GLOG_ERR("dctcp read msg length:%d is too much than buffer max size:%d hsz:%d rest:%d",
				msg_length, buffer->max_size, hsz, msg_buff_rest);
			_close_fd(stcp, fd, dctcp_close_reason_type::DCTCP_MSG_ERR, listenfd);
			return -1;
		}
		if (msg_length > msg_buff_rest){
			break;
		}
		dctcp_msg_t smsg(buffer->buffer + msg_buff_start + hsz, msg_length - hsz);
		sev.msg = &smsg;
		dctcp_event_dispatch(stcp, sev, proto_env);
		++nproc;
		msg_buff_start += msg_length;
		msg_buff_rest -= msg_length;
		//the connection may be close , check fd
		if (!_is_fd_ok(fd)){
			GLOG_TRA("fd closed when procssing msg rest of msg size:%d buff total:%d last msg unit size is :%d",
				msg_buff_rest, msg_buff_total, msg_length);
            return nproc;
		}
        msg_length = _dctcp_proto_msg_sz_length(hsz, buffer->buffer + msg_buff_start, msg_buff_rest);
    }//
    if (msg_buff_start > 0 && buffer->valid_size >= msg_buff_start){
		memmove(buffer->buffer,
			buffer->buffer + msg_buff_start,
            buffer->valid_size - msg_buff_start);
		////////////////////////////////////////////////////////////////
		buffer->valid_size -= msg_buff_start;
	}

	return nproc;
}
static inline int _dctcp_proto_dispatch_codec(dctcp_t * stcp, msg_buffer_t * buffer, int fd, int listenfd,
                                              dctcp_proto_dispatcher_t * proto_env) {
    dctcp_event_t	sev;
    sev.type = DCTCP_READ;
    sev.fd = fd;
    sev.listenfd = listenfd;
    int nproc = 0;//proc msg num
    //need dispatching
    //////////////////////////////////////////////////////////////////////////
    int msg_buff_length = 0;
    msg_buffer_t & msg_buff = proto_env->buffer;
    msg_buffer_t msg_proc_buff = *buffer;
    while(true){        
		if (proto_env->codec.encode) {
			msg_buff_length = proto_env->codec.decode(msg_buff, msg_proc_buff);
		}
		else { 
			msg_buff_length = dcs::codec_msg_token_decode(proto_env->param_s.data(), proto_env->param_s.length(), msg_buff, msg_proc_buff);
		}
        if (0 == msg_buff_length) {
            break;
        }
        else if(msg_buff_length < 0){
            //error codec
            GLOG_ERR("dctcp read msg decode error ret:%d msg buff valid:%d max:%d data buffer valid size:%d",
                     msg_buff_length, msg_buff.valid_size, msg_buff.max_size, buffer->valid_size);
            _close_fd(stcp, fd, dctcp_close_reason_type::DCTCP_MSG_ERR, listenfd);
            return -1;
        }
        dctcp_msg_t smsg(msg_buff.buffer, msg_buff.valid_size);
        sev.msg = &smsg;
        dctcp_event_dispatch(stcp, sev, proto_env);
        ++nproc;
        msg_proc_buff.buffer += msg_buff_length;
        msg_proc_buff.valid_size -= msg_buff_length;
        //the connection may be close , check fd
        if (!_is_fd_ok(fd)) {
            GLOG_TRA("fd closed when procssing msg rest of msg size:%d buff total:%d last msg unit size is :%d",
                     msg_proc_buff.valid_size, buffer->valid_size, msg_buff_length);
            //not need check buff (must not break)
            return nproc;
        }
    }
    if (nproc > 0) {
        memmove(buffer->buffer,
                msg_proc_buff.buffer,
                msg_proc_buff.valid_size);
        ////////////////////////////////////////////////////////////////
        buffer->valid_size = msg_proc_buff.valid_size;
    }
    return nproc;
}
static inline int _dctcp_proto_dispatch_token(dctcp_t * stcp, msg_buffer_t * buffer, int fd, int listenfd,
	dctcp_proto_dispatcher_t * proto_env){
	dctcp_event_t	sev;
	sev.type = DCTCP_READ;
	sev.fd = fd;
	sev.listenfd = listenfd;
	int nproc = 0;//proc msg num
	//need dispatching
	//////////////////////////////////////////////////
	int msg_buff_start = 0;
	int msg_buff_rest = buffer->valid_size;
	int msg_buff_total = buffer->valid_size;
	int msg_token_length = proto_env->param_s.length();
	int msg_length = 0;
	const char * ftok = strstr(buffer->buffer, proto_env->param_s.c_str());
	if (ftok && ftok < buffer->buffer + msg_buff_total){
        msg_length = ftok - buffer->buffer + msg_token_length;
	}
	if (!ftok && buffer->valid_size == buffer->max_size){
		//errror msg , too big 
		GLOG_ERR("dctcp read msg length:%d is too much than buffer max size:%d",
			msg_length, buffer->max_size);
		_close_fd(stcp, fd, dctcp_close_reason_type::DCTCP_MSG_ERR, listenfd);
		return -1;
	}
	while (msg_length > msg_token_length) {
        *(buffer->buffer + msg_length - msg_token_length) = '\0';
		dctcp_msg_t smsg(buffer->buffer + msg_buff_start, msg_length - msg_token_length);
		sev.msg = &smsg;
		dctcp_event_dispatch(stcp, sev, proto_env);
		++nproc;
		msg_buff_start += msg_length;
		msg_buff_rest -= msg_length;
		//the connection may be close , check fd
		if (!_is_fd_ok(fd)){
			GLOG_TRA("fd closed when procssing msg rest of msg size:%d buff total:%d last msg unit size is :%d",
				msg_buff_rest, msg_buff_total, msg_length);
            return nproc;
		}
		ftok = strstr(buffer->buffer + msg_buff_start, proto_env->param_s.c_str());
		if (ftok && ftok < buffer->buffer + msg_buff_total){
            msg_length = ftok - (buffer->buffer + msg_buff_start) + msg_token_length;
		}
		else {
			break;
		}
	}
    if (msg_buff_start > 0 && buffer->valid_size >= msg_buff_start){
		memmove(buffer->buffer,
			buffer->buffer + msg_buff_start,
            buffer->valid_size - msg_buff_start);
		////////////////////////////////////////////////////////////////
		buffer->valid_size -= msg_buff_start;
	}
	return nproc;
}
static inline int _read_tcp_socket(dctcp_t * stcp, int fd, int listenfd){
	msg_buffer_t * buffer = _get_sock_msg_buffer(stcp, fd, true);
	if (!buffer) {
		return -1;
	}
	dctcp_proto_dispatcher_t * proto_env = _dctcp_get_proto_dispatcher(stcp, fd, listenfd);
	if (!proto_env){
		return -2;
	}
	int nmsg = 0;
	while (buffer->max_size > (buffer->valid_size + 1)) {
		int sz = recv(fd, buffer->buffer + buffer->valid_size, 
						buffer->max_size - buffer->valid_size - 1, MSG_DONTWAIT);
		if (sz > 0){ //ok
			buffer->valid_size += sz;
		}
        else if (sz == 0){ //peer close
            _close_fd(stcp, fd, dctcp_close_reason_type::DCTCP_PEER_CLOSE, listenfd);
            return -3;
        }
        else if (sz == -1 && errno == EINTR){ //continue
			continue;
		}
        else if (sz == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
            return nmsg;
        }
        else {
            _close_fd(stcp, fd, dctcp_close_reason_type::DCTCP_SYS_CALL_ERR, listenfd);
            return -4;
        }
		//dispatch
		int ret = _dctcp_proto_dispatch_codec(stcp, buffer, fd, listenfd, proto_env);
		if (ret < 0){
			GLOG_ERR("fd:%d listen:%d dispatch proto msg error = %d",fd, listenfd, ret);
			return ret;
		}
		else if (ret >= 0){
			nmsg += ret;
		}
	}
	return nmsg;
}
static inline  int	_reconnect(dctcp_t* stcp, int fd, dctcp_connector_t & cnx){
    socklen_t addrlen = sizeof(cnx.connect_addr);
    int ret = connect(fd, (sockaddr*)&cnx.connect_addr, addrlen);
    if (ret && errno != EALREADY &&
        errno != EINPROGRESS) {
        //error
        return -1;
    }
    cnx.reconnect++;
    GLOG_TRA("tcp connect fd:%d tried:%d  ....", fd, cnx.reconnect);
	
	return _op_poll(stcp, EPOLL_CTL_ADD, fd, EPOLLOUT);
}
static inline void _connect_check(dctcp_t * stcp, int fd){
	int error = 0;
	auto it = stcp->connectors.find(fd);
	if (it == stcp->connectors.end()){
		//not connecting fd
		return ;
	}
	auto & cnx = it->second;
	socklen_t len = sizeof(int);
	if (0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)){
		dctcp_event_t sev;
		sev.fd = fd;
		if (0 == error){
			sev.type = dctcp_event_type::DCTCP_CONNECTED;
			dctcp_event_dispatch(stcp, sev);
			_op_poll(stcp, EPOLL_CTL_MOD, fd, EPOLLIN);
			return;
		}
	}
	else {
		if (cnx.reconnect < cnx.max_reconnect){
			//reconnect
			_op_poll(stcp, EPOLL_CTL_DEL, fd);
            _reconnect(stcp, fd, cnx);
		}
		else{
			_close_fd(stcp, fd, dctcp_close_reason_type::DCTCP_CONNECT_ERR, -1);
		}
	}
}
static inline int _dctcp_check_send_queue(dctcp_t * stcp, int fd, int listenfd, msg_buffer_t * msgbuff){
    int send_pdu = stcp->conf.max_tcp_send_buff_size;
    if (send_pdu > msgbuff->valid_size){
        send_pdu = msgbuff->valid_size;
    }
    int sent = 0;
    while (send_pdu > 0){
        int ret = send(fd, msgbuff->buffer + sent,
            send_pdu, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (ret > 0){ //send ok
            sent += ret;
        }
        else if (ret == 0){
            break;
        }
        else if (ret == -1 && errno == EINTR){
            continue;
        }
        else if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
            break;
        }
        else {
            GLOG_SER("send tcp fd:%d error sent:%d pdu:%d", fd, sent, send_pdu);
            _close_fd(stcp, fd, DCTCP_MSG_ERR, listenfd); //error write 
            return -1;
        }
        send_pdu = msgbuff->valid_size - sent;
        if (send_pdu > stcp->conf.max_tcp_send_buff_size){
            send_pdu = stcp->conf.max_tcp_send_buff_size;
        }
    }
    if (sent > 0 && msgbuff->valid_size >= sent){
        memmove(msgbuff->buffer, msgbuff->buffer + sent,
            msgbuff->valid_size - sent);
        msgbuff->valid_size -= sent;
    }
    return 0;
}
static inline int _write_tcp_socket(dctcp_t * stcp, int fd, const char * msg, int sz){
	//write app buffer ? write tcp socket directly ?
	msg_buffer_t* msgbuff = _get_sock_msg_buffer(stcp, fd, false);
	if (!msgbuff){//no buffer		
        GLOG_ERR("stcp fd:%d get buffer error !", fd);
		return -1;
	}
    int listenfd = _dctcp_get_fd_listenfd(stcp, fd);
    dctcp_proto_dispatcher_t * proto_env = _dctcp_get_proto_dispatcher(stcp, fd, listenfd);
    int msg_buff_total = 0;
    msg_buffer_t msg_buff_msg(msg, sz);
	int ret = 0;
	if (proto_env->codec.encode) {ret = proto_env->codec.encode(*msgbuff, msg_buff_msg);}
	else {ret = dcs::codec_msg_token_encode(proto_env->param_s.data(), proto_env->param_s.length(), *msgbuff, msg_buff_msg);}
	if (ret) {
		GLOG_ERR("encode msg error:%d msg_buff_msg.length:%d", ret, msg_buff_msg.valid_size);
		return -2;
	}
    return _dctcp_check_send_queue(stcp, fd, listenfd, msgbuff);
}

static inline bool _is_listenner(dctcp_t * stcp, int fd){
	return	stcp->listeners.find(fd) != stcp->listeners.end();
}
static inline void _proc(dctcp_t * stcp, const epoll_event & ev){
	//schedule
	//error check
	int listenfd = (ev.data.u64 >> 32);
	if (ev.events & EPOLLRDHUP){
		//peer close
		_close_fd(stcp, ev.data.fd, dctcp_close_reason_type::DCTCP_PEER_CLOSE, listenfd);
	}
	else if (ev.events & EPOLLERR){
		//error 
		_close_fd(stcp, ev.data.fd, dctcp_close_reason_type::DCTCP_POLL_ERR, listenfd);
	}
	else if ((ev.events & EPOLLHUP)){
		//rst ?
		_close_fd(stcp, ev.data.fd, dctcp_close_reason_type::DCTCP_INVAL_CALL, listenfd);
	}
	else if (ev.events & EPOLLOUT){
		_connect_check(stcp, ev.data.fd);
	}
	else if (ev.events & EPOLLIN){
		if (_is_listenner(stcp, ev.data.fd)){
			//new connection
			_new_connx(stcp, ev.data.fd);
		}
		else{
			_read_tcp_socket(stcp, ev.data.fd, listenfd);
		}
	}
	else {
		//error
		_close_fd(stcp, ev.data.fd, dctcp_close_reason_type::DCTCP_SYS_CALL_ERR, listenfd);
	}
}
void			dctcp_close(dctcp_t * stcp, int fd){
	_close_fd(stcp, fd, dctcp_close_reason_type::DCTCP_CLOSE_ACTIVE, _dctcp_get_fd_listenfd(stcp, fd));
}
int            dctcp_poll(dctcp_t * stcp, int timeout_us, int max_proc){
	PROFILE_FUNC();
	if (stcp->listeners.empty() &&
		stcp->connectors.empty()){
		return 0;
	}
    if (max_proc <= 0){
        max_proc = INT_MAX;
    }
	int nproc = 0; //process left events
	for (; stcp->nproc < stcp->nevts && nproc < max_proc; ++nproc){		
		_proc(stcp, stcp->events[stcp->nproc++]);
	}
	if (stcp->nproc < stcp->nevts){ //not over, busy for limit
		return nproc;
	}
	else { //poll over, clear
		stcp->nproc = 0;
		stcp->nevts = 0;
	}
    //next call epoll wait time
    ///////////////////////////////////////////////////////
    if (nproc == 0){//it's idle state, so limit next poll time
        uint64_t t_timeus_now = dcs::time_unixtime_us();
        if (t_timeus_now < stcp->next_evt_poll_time){
            return 0;
        }
        stcp->next_evt_poll_time = t_timeus_now + timeout_us;
    }
    ////////////////////////////////////////////////////////
	int n = epoll_wait(stcp->epfd, stcp->events, stcp->conf.max_client, 0);
    if (n > 0){
        stcp->nevts = n;
        for (; stcp->nproc < stcp->nevts && nproc < max_proc; ++nproc){
            _proc(stcp, stcp->events[stcp->nproc++]);
        }
    }
	return nproc;
}
int				dctcp_send(dctcp_t * stcp, int fd, const dctcp_msg_t & msg){
	if (fd < 0) {return -1; }
	return _write_tcp_socket(stcp, fd, msg.buff, msg.buff_sz);
}
int				dctcp_listen(dctcp_t * stcp, const string & addr, const char * fproto , dctcp_event_cb_t evcb, void * ud, const dctcp_msg_codec_t * codec){ //return a fd >= 0when success
    sockaddr_in addrin;
    int ret = dcs::socknetaddr(addrin, addr);
    if (ret){
        GLOG_ERR("listen addr :%s is invlaid !", addr.c_str());
        return -1;
    }
    int fd = _create_tcpsocket(stcp->conf.max_tcp_send_buff_size, stcp->conf.max_tcp_recv_buff_size);
	if (fd < 0) { return -1; }
	ret = bind(fd, (struct sockaddr *)&addrin, sizeof(struct sockaddr));
	if (ret) { close(fd); return -2; }
	ret = listen(fd, stcp->conf.max_backlog);
	if (ret) { close(fd); return -3; }

	epoll_event evt;
	evt.events = EPOLLIN | EPOLLET;
	evt.data.fd = fd;
	ret = epoll_ctl(stcp->epfd, EPOLL_CTL_ADD, fd, &evt);
	if (ret) { close(fd); return -4; };

	_add_listenner(stcp, fd, addrin, fproto, evcb, ud, codec);
	return fd;
}

int             dctcp_connect(dctcp_t * stcp, const string & addr, int retry, const char * fproto, dctcp_event_cb_t cb, void * ud, const dctcp_msg_codec_t * codec ){
    sockaddr_in saddr;
    int ret = dcs::socknetaddr(saddr, addr);
    if (ret){
        GLOG_ERR("connect sock addr error :%s", addr.c_str());
        return -1;
    }
    //allocate
    int fd = _create_tcpsocket(stcp->conf.max_tcp_send_buff_size, stcp->conf.max_tcp_recv_buff_size);
	if (fd < 0) return -1;//socket error
	int on = 1;
	_set_socket_opt(fd, SO_KEEPALIVE, &on, sizeof(on));
	saddr.sin_family = AF_INET;
	//////////////////////////////////////////////////////////////////////////
    dctcp_connector_t & cnx = _add_connector(stcp, fd, saddr, retry, fproto, cb, ud, codec);
	return _reconnect(stcp, fd, cnx);
}
