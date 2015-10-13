#include "libtcp.h"

typedef union epoll_data {
	void    *ptr;
	int      fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event {
	uint32_t     events;    /* Epoll events */
	epoll_data_t data;      /* User data variable */
};

struct msg_buffer_t {
	char *	buffer;
	int		max_size;
	int		recv_size;
	int		current_msg_start;
	int		current_msg_size;
	msg_buffer() :buffer(nullptr), max_size(0), recv_size(0), current_msg_start(0), current_msg_size(0)
	{
	}
	int create(int max_sz)
	{
		char * p = malloc(max_size);
		if (!p) return -1;
		max_size = max_sz;
		current_msg_start = current_msg_size = recv_size = 0;
		buffer = p;
		return 0;
	}
	~msg_buffer()
	{
		if (buffer) { free(buffer); buffer = nullptr; max_size = 0;}
	}
};

struct stcp_t
{
	stcp_config_t	conf;
	int				epfd;	//epfd for poller
	int				sockfd;	//listen fd or client fd
	///////////////////////////////
	stcp_event_cb_t	event_cb;
	void		*	event_cb_ud;
	///////////////////////////////
	epoll_event	*	events;
	int				nproc;
	int				nevts;
	///////////////////////////////
	int				reconnect;
	int				max_reconnect;
	sockaddr		connect_addr;
	//////////////////////////////
	std::unordered_map<int, msg_buffer_t>	sock_recv_buffer;


	void init()
	{
		sockfd = epfd = -1;
		event_cb = nullptr;
		event_cb_ud = nullptr;
		events = nullptr;
		nevts = nproc = 0;
		reconnect = max_reconnect = 0;
		sock_recv_buffer.clear();
	}
};

static int _set_socket_opt(int fd, int name, void * val, socklen_t len)
{
	return setsockopt(fd, SOL_SOCKET, name, val, len);
}

static int _set_socket_ctl(int fd, int flag, bool open)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		return -1;
	}
	if (open)
	{
		flags |= flag;
	}
	else
	{
		flags &= ~(flag);
	}
	if (fcntl(fd, F_SETFL, flags) < 0)
	{
		return -1;
	}
	return 0;
}
int _create_tcpsocket(int sendbuffsize, int recvbuffsize)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		return -1;
	}
	int on = 1;
	int ret = setsockopt(fd , SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (ret)
	{
		close(fd);
		return -2;
	}
	size_t buffsz = recvbuffsize;
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffsz, sizeof(buffsz));
	if (ret != 0)
	{
		close(fd);
		return -3;
	}
	buffsz = sendbuffsize;
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffsz, sizeof(buffsz));
	if (ret != 0)
	{
		close(fd);
		return -4;
	}
	_set_socket_ctl(fd, O_NONBLOCK, true); //nblock
	return fd;
}

struct stcp_t * stcp_create(const stcp_config_t & conf)
{
	int fd = _create_tcpsocket(conf.max_tcp_send_buff_size, conf.max_tcp_recv_buff_size);
	if (fd < 0) return nullptr;
	int epfd = epoll_create(conf.max_client);
	if (epfd < 0) goto FAIL_SOCKET;
	if (conf.is_server)
	{
		sockaddr_in addrin;
		memset(&addrin, 0, sizeof(addrin));
		addrin.sin_family = AF_INET;
		addrin.sin_port = htons(port);
		addrin.sin_addr.s_addr = ip;
		ret = bind(fd, (struct sockaddr *)&addrin, sizeof(struct sockaddr));
		if (ret) goto FAIL_SOCKET;
		ret = listen(fd, conf.max_backlog);
		if (ret) goto FAIL_SOCKET;

		epoll_event evt;
		stcp_t* stcp = nullptr;
		int ret = 0;
		if (!p3) goto FAIL_SOCKET;
		evt.events = EPOLLIN | EPOLLET;
		evt.data.fd = fd;
		ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &evt);
		if (ret) goto FAIL_SOCKET;
	}
	//ok
	{
		stcp_t * stcp = new stcp_t();
		stcp->conf = conf;
		stcp->epfd = epfd;
		stcp->events = (epoll_event *)malloc(conf.max_client * sizeof(epoll_event));
		return	stcp;
	}
FAIL_SOCKET:
	if(fd >=0 ) close(fd);
	if (epfd >= 0) close(epfd);
	return nullptr;
}
void            stcp_destroy(stcp_t * stcp)
{
	if (stcp->sockfd >= 0) close(stcp->sockfd);
	if (stcp->epfd >= 0) close(stcp->epfd);
	if (stcp->events) free(stcp->events);
	stcp->init();
}
void            stcp_event_cb(stcp_t* stcp, stcp_event_cb_t cb, void * ud)
{
	stcp->event_cb = cb;
	stcp->event_cb_ud = ud;
}
static int _op_poll(stcp_t * stcp, int cmd, int fd, int flag = 0)
{
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = flag;
	return epoll_ctl(stcp->epfd, cmd, fd, &ev);
}
static void _new_connx(stcp_t * stcp, int listenfd)
{
	struct sockaddr	addr;
	socklen_t	len = sizeof(addr);
	int nfd = accept(listenfd, &addr, &len);
	if (nfd == -1)
	{
		//error log
	}
	else
	{
		stcp_event_t sev;
		sev.fd = nfd;
		sev.type = stcp_event_type::STCP_CONNECTED;
		//add epollin
		_op_poll(stcp, EPOLL_CTL_ADD, nfd, EPOLLIN);
		stcp->event_cb(stcp, sev, stcp->event_cb_ud);
	}
}
static void _data_readable(stcp_t* stcp, int fd)
{
	_read_tcp_socket(stcp, fd);
}
static void _connect_check(stcp_t * stcp, int fd)
{
	int error = 0;
	socklen_t len = sizeof(int);
	if ((0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)))
	{
		stcp_event_t sev;
		sev.fd = fd;
		_op_poll(stcp, EPOLL_CTL_DEL, fd);
		if (0 == error)
		{
			sev.type = stcp_event_type::STCP_CONNECTED;
			stcp->event_cb(stcp, sev, stcp->event_cb_ud);
			stcp->reconnect = 0;
		}
		else
		{
			//sev.type = stcp_event_type::STCP_CONNECT_ERROR;
			//stcp->event_cb(stcp, sev, stcp->event_cb_ud);
			//connect error
			if (stcp->reconnect < stcp->max_reconnect)
			{
				stcp_reconnect(stcp);
			}
		}
	}
	else
	{
		//perror for errno
	}
}
static void _connect_error(stcp_t * stcp, int fd)
{
	_close_fd(stcp, fd, stcp_event_type::STCP_CONNECT_ERROR)
}
//write msg
static int _write_tcp_socket(stcp_t * stcp, int fd, const char * msg, int sz)
{
	//write app buffer ? write tcp socket directly ?
	//size
	stcp_event_t sev;
	sev.fd = fd;
	int nsize = htonl(sz + sizeof(int));
	//todo get send buffer like recv buffer

	int ret = send(fd, &nsize, sizeof(nsize), MSG_DONTWAIT | MSG_NOSIGNAL | MSG_MORE);
	if (ret != sizeof(nsize)) { 
		//error 
		return -1;
	}
RETRY:
	int ret = send(fd, msg, sz, MSG_DONTWAIT | MSG_NOSIGNAL);
	if (ret == sz)
	{
		//send ok
		sev.type = stcp_event_type::STCP_WRITE;
		sev.msg = stcp_msg_t(msg, sz);
		stcp->event_cb(stcp, sev, stcp->event_cb_ud);
		return 0;
	}
	else
	{
		if (errno == EINTR)
		{
			goto RETRY;
		}
		//just send one part , close connection
		_close_fd(stcp, fd, stcp_event_type::STCP_DATA_ERR)
	}
	//errno 
	return -2;
}
static void	_close_fd(stcp_t * stcp, int fd, int reason)
{
	stcp_event_t  sev;
	sev.type = reason;
	stcp->event_cb(stcp, sev, stcp->event_cb_ud);
	stcp->sock_recv_buffer.erase(fd);
	close(fd);
	_op_poll(stcp, EPOLL_CTR_DEL, fd);
}

msg_buffer_t * _get_recv_msg_buffer(stcp_t * stcp, int fd)
{
	auto it = stcp->sock_recv_buffer.find(fd);
	if (it == stcp->sock_recv_buffer.end())
	{
		stcp->sock_recv_buffer[fd] = msg_buffer_t();
		if (stcp->sock_recv_buffer[fd].create(stcp->conf.max_recv_buff))
		{
			//no mem error
			return nullptr;
		}
		return &(stcp->sock_recv_buffer[fd]);
	}
	return &(it->second);
}

//read msg
static int _read_tcp_socket(stcp_t * stcp, int fd)
{
	msg_buffer_t * buffer = _get_recv_msg_buffer(stcp, fd);
	stcp_event_t	sev;
	sev.type = STCP_READ;
	sev.fd = fd;
	if (buffer->current_msg_size > 0)
	{
		int remain = buffer->current_msg_size - buffer->recv_size;
		while (remain > 0)
		{
			int sz = recv(fd, buffer->buffer + buffer->current_msg_start, remain, MSG_DONTWAIT);
			if (sz == 0)
			{
				//peer close
				_close_fd(stcp, fd, stcp_event_type::STCP_CLOSED);
			}
			else if (sz < 0 &&
				errno != EINTR &&
				errno != EAGIN &&
				errno != EWOULDBLOCK)
			{
				//error
				_close_fd(stcp, fd, stcp_event_type::STCP_DATA_ERR);
			}
		}
	}
	else
	{
		//
	}

}
static void _proc(stcp_t * stcp, epoll_event & ev)
{
	//schedule todo
	if (ev.events & EPOLLIN)
	{
		if (ev.data.fd == stcp->sockfd &&
			stcp_is_server(stcp))
		{
			//new connection
			_new_connx(stcp, ev.data.fd);
		}
		else
		{
			_data_readable(stcp, ev.data.fd);
		}
	}
	else if (ev.events & EPOLLOUT)
	{
		_connect_check(stcp, ev.data.fd);
	}
	else
	{
		//error
		_connect_error(stcp, ev.data.fd);
	}
}

void            stcp_poll(stcp_t * stcp, int timeout_us, int max_proc = 100)
{
	int ms = timeout_us / 1000;
	if (ms == 0) ms = 1;
	int nproc = 0;
	for (int i = stcp->nproc; i < stcp->nevts && nproc < max_proc; ++i)
	{
		const epoll_event & ev = stcp->events[i];
		++nproc;
		_proc(stcp, stcp->nevts[i]);
	}
	stcp->nproc += nproc;
	if (stcp->nproc > stcp->nevts)
	{
		stcp->nproc = 0;
		stcp->nevts = 0;
	}
	else
	{
		//busy
		return 0;
	}
	int n = epoll_wait(stcp->epfd, stcp->events, stcp->conf.max_client);
	if (n > 0)
	{
		stcp->nproc = 0;
		stcp->nevts = n;
	}
	for (int i = 0; i < n && nproc < max_proc; ++i)
	{
		nproc++;
		_proc(stcp, stcp->nevts[i]);
	}
}
int				stcp_send(stcp_t * stcp, int fd, const stcp_msg_t & msg)
{
	return _write_tcp_socket(stcp, fd, msg.buff, msg.buff_sz);
}
int				stcp_reconnect(stcp_t* stcp)
{
	if (stcp_is_server(stcp))
	{
		return -1;
	}
	socklen_t addrlen = sizeof(stcp->connect_addr);
	int ret = connect(stcp->sockfd, &stcp->connect_addr, addrlen);
	if (ret && errno != EALREADY &&
		errno != EINPROGRESS)
	{
		//error no
		return -1;
	}
	return _op_poll(stcp->epfd, EPOLL_CTL_ADD, stcp->sockfd, EPOLLOUT);
}
int             stcp_connect(stcp_t * stcp, const stcp_addr_t & addr, int retry)
{
	if (stcp_is_server(stcp))
	{
		//not type
		return -1;
	}
	int on = 1;
	_set_socket_opt(stcp->sockfd, SO_KEEPALIVE, &on, sizeof(on));
	struct sockaddr addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(addr.ip.c_str());
	addr.sin_port = htons(addr.port);

	stcp->connect_addr = addr;
	stcp->max_reconnect = retry;
	stcp->reconnect = 1;
	return stcp_reconnect(stcp);
}
bool            stcp_is_server(stcp_t * stcp)
{
	return stcp->conf.is_server;
}
