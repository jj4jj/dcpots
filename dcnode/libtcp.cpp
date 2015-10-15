#include "libtcp.h"
#include "msg_proto.hpp"

typedef	std::shared_ptr<msg_buffer_t>	msg_buffer_ptr_t;

//client
struct stcp_connecting_t {
	int				reconnect;
	int				max_reconnect;
	sockaddr_in 	connect_addr;
	stcp_connecting_t() {
		bzero(this, sizeof(*this));
	}
};

struct stcp_t
{
	stcp_config_t	conf;
	int				epfd;	//epfd for poller
	int				listenfd;	//listen fd or client fd
	std::unordered_map<int, stcp_connecting_t>		connectings;
	///////////////////////////////
	stcp_event_cb_t	event_cb;
	void		*	event_cb_ud;
	///////////////////////////////
	epoll_event	*	events;
	int				nproc;
	int				nevts;
	//////////////////////////////
	std::unordered_map<int, msg_buffer_ptr_t>	sock_recv_buffer;
	std::unordered_map<int, msg_buffer_ptr_t>	sock_send_buffer;
	
	void init()
	{
		listenfd = epfd = - 1;
		event_cb = nullptr;
		event_cb_ud = nullptr;
		events = nullptr;
		nevts = nproc = 0;
		sock_recv_buffer.clear();
		connectings.clear();
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
static int 	_init_socket_options(int fd, int sendbuffsize,int recvbuffsize)
{
	int on = 1;
	int ret = _set_socket_opt(fd, TCP_NODELAY, &on, sizeof(on));
	ret |= _set_socket_opt(fd, SO_REUSEADDR, &on, sizeof(on));
	size_t buffsz = recvbuffsize;
	ret |= _set_socket_opt(fd, SO_RCVBUF, &buffsz, sizeof(buffsz));
	buffsz = sendbuffsize;
	ret |= _set_socket_opt(fd, SO_SNDBUF, &buffsz, sizeof(buffsz));
	ret |= _set_socket_ctl(fd, O_NONBLOCK, true); //nblock
	if (ret != 0)
	{
		close(fd);
		return ret;
	}
	return 0;
}
static int _create_tcpsocket(int sendbuffsize, int recvbuffsize)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return -1;
	int ret = _init_socket_options(fd , sendbuffsize, recvbuffsize);
	if (ret) return -2;
	return fd;
}

struct stcp_t * stcp_create(const stcp_config_t & conf)
{
	stcp_t * stcp = new stcp_t();
	stcp->conf = conf;
	stcp->events = (epoll_event *)malloc(conf.max_client * sizeof(epoll_event));
	if (!stcp->events) { stcp_destroy(stcp); return nullptr; }

	stcp->epfd = epoll_create(conf.max_client);
	if (stcp->epfd < 0) { stcp_destroy(stcp); return nullptr; }

	if (conf.is_server)
	{
		int fd = _create_tcpsocket(conf.max_tcp_send_buff_size, conf.max_tcp_recv_buff_size);
		if (fd < 0) { stcp_destroy(stcp); return nullptr; }
		stcp->listenfd = fd;
		sockaddr_in addrin;
		memset(&addrin, 0, sizeof(addrin));
		addrin.sin_family = AF_INET;
		addrin.sin_port = htons(conf.listen_addr.port);
		addrin.sin_addr.s_addr = conf.listen_addr.u32ip();
		int ret = bind(fd, (struct sockaddr *)&addrin, sizeof(struct sockaddr));
		if (ret) { stcp_destroy(stcp); return nullptr; }
		ret = listen(fd, conf.max_backlog);
		if (ret) { stcp_destroy(stcp); return nullptr; }

		epoll_event evt;
		evt.events = EPOLLIN;
		evt.data.fd = fd;
		ret = epoll_ctl(stcp->epfd, EPOLL_CTL_ADD, fd, &evt);
		if (ret) { stcp_destroy(stcp); return nullptr; };
	}
	return stcp;
}
void            stcp_destroy(stcp_t * stcp)
{
	if (stcp->listenfd >= 0) close(stcp->listenfd);
	for (auto & it : stcp->connectings) close(it.first);
	if (stcp->epfd >= 0) close(stcp->epfd);
	if (stcp->events) free(stcp->events);
	stcp->init();
	delete stcp;
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

static void	_close_fd(stcp_t * stcp, int fd, stcp_close_reason_type reason)
{
	_op_poll(stcp, EPOLL_CTL_DEL, fd);
	close(fd);
	stcp->connectings.erase(fd);
	stcp->sock_recv_buffer.erase(fd);
	stcp->sock_send_buffer.erase(fd);
	//notify
	stcp_event_t  sev;
	sev.type = stcp_event_type::STCP_CLOSED;
	sev.reason = reason;
	stcp->event_cb(stcp, sev, stcp->event_cb_ud);
}

static void _new_connx(stcp_t * stcp, int listenfd)
{
	struct sockaddr	addr;
	socklen_t	len = sizeof(addr);
	int nfd = accept(listenfd, &addr, &len);
	if (nfd >= 0)
	{
		stcp_event_t sev;
		sev.fd = nfd;
		sev.type = stcp_event_type::STCP_NEW_CONNX;
		_init_socket_options(nfd, stcp->conf.max_tcp_send_buff_size, stcp->conf.max_tcp_recv_buff_size);
		//add epollin
		_op_poll(stcp, EPOLL_CTL_ADD, nfd, EPOLLIN);
		stcp->event_cb(stcp, sev, stcp->event_cb_ud);
	}
	else
	{
		//error
	}
}
msg_buffer_t * _get_sock_msg_buffer(stcp_t * stcp, int fd, bool for_recv)
{
	auto * fmap = &stcp->sock_recv_buffer;
	int bufsize = stcp->conf.max_recv_buff;
	if (!for_recv)
	{
		fmap = &stcp->sock_send_buffer;
		bufsize = stcp->conf.max_send_buff;
	}
	auto it = fmap->find(fd);
	if (it == fmap->end())
	{
		fmap->insert(std::make_pair(fd, msg_buffer_ptr_t(new msg_buffer_t())));
		if ((*fmap)[fd]->create(bufsize))
		{
			fmap->erase(fd);
			//no mem error
			return nullptr;
		}
		return ((*fmap)[fd]).get();
	}
	return it->second.get();
}
static int _read_msg_error(stcp_t * stcp, int fd, int read_ret)
{
	if (read_ret == 0)
	{
		//peer close
		_close_fd(stcp, fd, stcp_close_reason_type::STCP_PEER_CLOSE);
		return -1;
	}
	else //if (sz < 0 )
	{
		if (errno != EAGAIN &&
			errno != EINTR &&
			errno != EWOULDBLOCK )
		{
			//error
			_close_fd(stcp, fd, stcp_close_reason_type::STCP_SYS_ERR);
			return -2;
		}
	}
	return 0;
}
static int _read_tcp_socket(stcp_t * stcp, int fd)
{
	msg_buffer_t * buffer = _get_sock_msg_buffer(stcp, fd, true);
	stcp_event_t	sev;
	sev.type = STCP_READ;
	sev.fd = fd;
	int nmsg = 0;
READ_MSG_DISPATCH:
	if (buffer->ctx_msg_size > 0)
	{
		int remain = buffer->ctx_msg_size - buffer->valid_size;
		while (remain > 0)
		{
			int sz = recv(fd, buffer->buffer + buffer->valid_size, remain, MSG_DONTWAIT);
			if (sz > 0)
			{
				remain -= sz;
				buffer->valid_size += sz;
				if (remain == 0)
				{
					stcp_msg_t smsg = stcp_msg_t(buffer->buffer + sizeof(int32_t), buffer->ctx_msg_size - sizeof(int32_t));
					sev.msg = &smsg;
					stcp->event_cb(stcp, sev, stcp->event_cb_ud);
					buffer->ctx_msg_size = 0;
					buffer->valid_size = 0;
					nmsg++;
					goto READ_MSG_DISPATCH;
				}
			}
			else if (sz == -1 && errno == EINTR)
			{
				continue;
			}
			else
			{
				return _read_msg_error(stcp, fd, sz);
			}
		}
	}
	else
	{
	TRY_READ_HEAD_AGAIN:
		//read head
		int sz = recv(fd, buffer->buffer + buffer->valid_size, sizeof(int32_t), MSG_DONTWAIT);
		if (sz > 0)
		{
			buffer->valid_size += sz;
			if (buffer->valid_size == sizeof(int32_t))
			{
				//get head
				int32_t * headlen = (int32_t*)(buffer->buffer + buffer->valid_size);
				buffer->ctx_msg_size = ntohl(*headlen);
				if (buffer->ctx_msg_size > buffer->max_size)
				{
					//errror msg , too big 
					_close_fd(stcp, fd, stcp_close_reason_type::STCP_MSG_ERR);
					return -3;
				}
				else
				{
					goto READ_MSG_DISPATCH;
				}
			}
		}
		else if (sz == -1 && errno == EINTR)
		{
			goto TRY_READ_HEAD_AGAIN;
		}
		else
		{
			return _read_msg_error(stcp, fd, sz);
		}
	}
	return nmsg;
}
static void _data_readable(stcp_t* stcp, int fd)
{
	_read_tcp_socket(stcp, fd);
}
static void _connect_check(stcp_t * stcp, int fd)
{
	int error = 0;
	auto it = stcp->connectings.find(fd);
	if (it == stcp->connectings.end())
	{
		//not connecting fd
		return ;
	}
	auto & cnx = it->second;
	socklen_t len = sizeof(int);
	if ((0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)))
	{
		stcp_event_t sev;
		sev.fd = fd;
		_op_poll(stcp, EPOLL_CTL_DEL, fd);
		if (0 == error)
		{
			//clear
			stcp->connectings.erase(fd);
			sev.type = stcp_event_type::STCP_CONNECTED;
			stcp->event_cb(stcp, sev, stcp->event_cb_ud);
			_op_poll(stcp, EPOLL_CTL_ADD, fd, EPOLLIN);
		}
		else
		{
			if (cnx.reconnect < cnx.max_reconnect)
			{
				//reconnect
				stcp_reconnect(stcp, fd);
			}
			else
			{
				_close_fd(stcp, fd, stcp_close_reason_type::STCP_CONNECT_ERR);
			}
		}
	}
	else
	{
		_close_fd(stcp, fd, stcp_close_reason_type::STCP_INVAL_CALL);
	}
}

//write msg
static int _write_tcp_socket(stcp_t * stcp, int fd, const char * msg, int sz)
{
	//write app buffer ? write tcp socket directly ?
	//size
	stcp_event_t sev;
	sev.fd = fd;
	sev.type = stcp_event_type::STCP_WRITE;

	int total = sz + sizeof(int32_t);
	if (total > stcp->conf.max_send_buff)
	{
		return -1;
	}
	msg_buffer_t* msgbuff = _get_sock_msg_buffer(stcp, fd, false);
	*(int32_t*)(msgbuff->buffer) = htonl(total);
	memcpy(msgbuff->buffer + sizeof(int32_t), msg, sz);
	
RETRY_WRITE_MSG:
	int ret = send(fd, msgbuff->buffer, total, MSG_DONTWAIT | MSG_NOSIGNAL);
	if (ret == sz)
	{
		//send ok
		stcp_msg_t smsg = stcp_msg_t(msg, sz);
		sev.msg = &smsg;
		stcp->event_cb(stcp, sev, stcp->event_cb_ud);
		return 0;
	}
	else
	{
		if ( ret == -1 && errno == EINTR)
		{
			goto RETRY_WRITE_MSG;
		}
		//just send one part , close connection
		_close_fd(stcp, fd, stcp_close_reason_type::STCP_MSG_ERR);
	}
	//errno 
	return -2;
}
static void _proc(stcp_t * stcp,const epoll_event & ev)
{
	//schedule todo 
	if (ev.events & EPOLLIN)
	{
		if (ev.data.fd == stcp->listenfd )
		{
			assert(stcp_is_server(stcp));
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
		_close_fd(stcp, ev.data.fd, stcp_close_reason_type::STCP_POLL_ERR);
	}
}

int            stcp_poll(stcp_t * stcp, int timeout_us, int max_proc)
{
	int ms = timeout_us / 1000;
	if (ms == 0) ms = 1;
	int nproc = 0;
	for (int i = stcp->nproc; i < stcp->nevts && nproc < max_proc; ++i)
	{
		++nproc;
		_proc(stcp, stcp->events[i]);
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
		return nproc;
	}
	int n = epoll_wait(stcp->epfd, stcp->events, stcp->conf.max_client, ms);
	if (n > 0)
	{
		stcp->nproc = 0;
		stcp->nevts = n;
	}
	for (int i = 0; i < n && nproc < max_proc; ++i)
	{
		nproc++;
		_proc(stcp, stcp->events[i]);
	}
	return nproc;
}
int				stcp_send(stcp_t * stcp, int fd, const stcp_msg_t & msg)
{
	return _write_tcp_socket(stcp, fd, msg.buff, msg.buff_sz);
}
int				stcp_reconnect(stcp_t* stcp, int fd)
{
	auto it = stcp->connectings.find(fd);
	stcp_connecting_t * cnx = nullptr;
	if (it != stcp->connectings.end())
	{
		cnx = &it->second;
	}
	socklen_t addrlen = sizeof(cnx->connect_addr);
	int ret = connect(fd, (sockaddr*)&cnx->connect_addr, addrlen);
	if (ret && errno != EALREADY &&
		errno != EINPROGRESS)
	{
		//error no
		return -1;
	}
	cnx->reconnect++;
	return _op_poll(stcp, EPOLL_CTL_ADD, fd, EPOLLOUT);
}
int             stcp_connect(stcp_t * stcp, const stcp_addr_t & addr, int retry)
{
	//allocate
	int fd = _create_tcpsocket(stcp->conf.max_tcp_send_buff_size, stcp->conf.max_tcp_recv_buff_size);
	if (fd < 0) return -1;//socket error

	int on = 1;
	_set_socket_opt(fd, SO_KEEPALIVE, &on, sizeof(on));

	sockaddr_in saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = addr.u32ip();
	saddr.sin_port = htons(addr.port);

	stcp_connecting_t cnx;
	cnx.max_reconnect = retry;
	cnx.reconnect = 0;
	cnx.connect_addr = saddr;
	stcp->connectings[fd] = cnx;

	return stcp_reconnect(stcp, fd);
}
bool            stcp_is_server(stcp_t * stcp)
{
	return stcp->conf.is_server;
}
