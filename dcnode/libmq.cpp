#include "libmq.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct smq_t
{
	smq_config_t	conf;
	int				sender;
	int				recver;
	smq_msg_cb_t	msg_cb;
	void		*	msg_cb_ud;
	msgbuf		*	sendbuff;
	msgbuf		*	recvbuff;
	
	smq_t()
	{
		init();
	}
	void init()
	{
		sender = recver = -1;
		msg_cb = nullptr;
		sendbuff = recvbuff = nullptr;
		msg_cb_ud = nullptr;
	}
};
int		_msgq_create(key_t key, int flag, size_t max_size)
{
	int id = msgget(key, flag);
	if (id < 0)
	{
		//get error
		return -1;
	}
	struct msqid_ds mds;
	int ret = msgctl(id, IPC_STAT, (struct msqid_ds *)&mds);
	if (ret != 0)
	{
		return -2;
	}

	if (mds.msg_qbytes != max_size)
	{
		mds.msg_qbytes = max_size;
		ret = msgctl(id, IPC_SET, (struct msqid_ds *)&mds);
		if (ret != 0)
		{
			return -3;
		}
	}
	return 0;
}

smq_t * smq_create(const smq_config_t & conf)
{
	//sender : recver (1:2) : client
	int prj_id[] = { 1, 2 };
	if (conf.is_server)
	{
		prj_id[0] = 2;
		prj_id[1] = 1;
	}
	int flag = 0666;
	if (!conf.attach)
	{
		flag |= IPC_CREAT;
	}
	key_t key = ftok(conf.key.c_str(), prj_id[0]);
	int sender = _msgq_create(key, flag, conf.max_queue_buff_size);
	if (sender < 0)
	{
		//errno
		return nullptr;
	}
	key = ftok(conf.key.c_str(), prj_id[1]);
	int recver = _msgq_create(key, flag, conf.max_queue_buff_size);
	if (recver < 0)
	{
		//errno
		return nullptr;
	}
	smq_t * smq = new smq_t();
	if (!smq)
	{
		//memalloc
		return nullptr;
	}
	smq->sendbuff = (msgbuf	*)malloc(conf.msg_buffsz);
	smq->recvbuff = (msgbuf	*)malloc(conf.msg_buffsz);
	if (!smq->sendbuff ||
		!smq->recvbuff)
	{
		//mem alloc
		return nullptr;
	}
	smq->conf = conf;
	smq->sender = sender;
	smq->recver = recver;
	return smq;
}
void    smq_destroy(smq_t* smq)
{
	if (smq->sender >= 0)
	{
		//no need
	}
	if (smq->recver >= 0)
	{
		//no need
	}
	if (smq->sendbuff)
	{
		free(smq->sendbuff);		
	}
	if (smq->recvbuff)
	{
		free(smq->recvbuff);
	}
	smq->init();
}
void    smq_msg_cb(smq_t * smq, smq_msg_cb_t cb, void * ud)
{
	smq->msg_cb = cb;
	smq->msg_cb_ud = ud;
}
void    smq_poll(smq_t*  smq, int timeout_us)
{	
	//todo with poll
	timeval tv1,tv2;
	gettimeofday(&tv1, NULL);
	int64_t past_us = 0;
	ssize_t sz = 0;
	int nproc = 0;
	while (past_us < timeout_us)
	{
		if (smq->conf.is_server)
		{
			sz = msgrcv(smq->recver, smq->recvbuff, smq->conf.msg_buffsz, 0, IPC_NOWAIT);
		}
		else
		{
			sz = msgrcv(smq->recver, smq->recvbuff, smq->conf.msg_buffsz, getpid(), IPC_NOWAIT);
		}
		if (sz <= 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else
			if (errno == E2BIG)
			{
				//error for msg too much , clear it then continue;
				sz = msgrcv(smq->recver, smq->recvbuff, smq->conf.msg_buffsz, getpid(), IPC_NOWAIT | MSG_NOERROR);
				continue;
			}
			else if (errno != ENOMSG)
			{
				//error log for
			}
			break;
		}
		else
		{
			msgbuf * buf = (msgbuf*)(smq->recvbuff);
			smq->msg_cb(smq, buf->mtype, smq_msg_t(buf->mtext, sz), smq->msg_cb_ud);
		}
		nproc++;
		if (nproc >= 5)
		{
			gettimeofday(&tv2, NULL);
			past_us += (tv2.tv_sec - tv1.tv_sec) * 1000000;
			past_us += (tv2.tv_usec - tv1.tv_usec);
			nproc = 0;
		}
	}
}

int     smq_send(smq_t* smq, uint64_t dst, const smq_msg_t & msg)
{
	if (msg.sz > smq->conf.msg_buffsz)
	{
		//size error
		return -1;
	}
	if (dst == 0)
	{
		//dst error
		return -2;
	}
	smq->sendbuff->mtype = dst;	
	memcpy(smq->sendbuff->mtext, msg.buffer, msg.sz);
	int ret = 0;
	do 
	{
		ret = msgsnd(smq->sender, smq->sendbuff, msg.sz, IPC_NOWAIT);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

