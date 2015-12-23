#include "dcsmq.h"
#include "dcutils.hpp"
#include "profile.h"
#include <sys/msg.h>

struct dcsmq_t {
	dcsmq_config_t	conf;
	int				sender;
	int				recver;
	dcsmq_msg_cb_t	msg_cb;
	void		*	msg_cb_ud;
	msgbuf		*	sendbuff;
	msgbuf		*	recvbuff;
	uint64_t		session;	//myself id , send and recv msg cookie. in servermode is 0.
    dcsmq_stat_t    stat;
	dcsmq_t(){
		init();
	}
	void init(){
		sender = recver = -1;
		msg_cb = nullptr;
		sendbuff = recvbuff = nullptr;
		msg_cb_ud = nullptr;
		session = 0;
        memset(&stat, 0, sizeof(stat));
	}
};

#define STAT_ON_RECV(mtype, msize)  do{\
    smq->stat.msg_rtime = dcsutil::time_unixtime_us(); \
    ++smq->stat.recv_num; \
    smq->stat.recv_size += msize; \
    smq->stat.recv_last = mtype; \
}while (false)

#define STAT_ON_RECV_ERROR()  do{\
    smq->stat.msg_rtime = dcsutil::time_unixtime_us(); \
    ++smq->stat.recv_num; \
    ++smq->stat.recv_error;\
}while (false)

#define STAT_ON_SEND(mtype, msize)  do{\
    smq->stat.msg_stime = dcsutil::time_unixtime_us(); \
    ++smq->stat.send_num; \
    smq->stat.send_size += msize; \
    smq->stat.send_last = mtype; \
}while (false)

#define STAT_ON_SEND_ERROR()  do{\
    smq->stat.msg_stime = dcsutil::time_unixtime_us(); \
    ++smq->stat.send_num; \
    ++smq->stat.send_error; \
}while (false)


int		_msgq_create(key_t key, int flag, size_t max_size){
	int id = msgget(key, flag);
	if (id < 0){
		//get error
		GLOG_ERR( "msg get error !");
		return -1;
	}
	struct msqid_ds mds;
	int ret = msgctl(id, IPC_STAT, (struct msqid_ds *)&mds);
	if (ret != 0){
		GLOG_ERR( "msgctl error !");
		return -2;
	}

	if (mds.msg_qbytes != max_size){
		mds.msg_qbytes = max_size;
		ret = msgctl(id, IPC_SET, (struct msqid_ds *)&mds);
		if (ret != 0){
			GLOG_ERR( "msgctl error !");
			return -3;
		}
	}
	return id;
}

dcsmq_t * dcsmq_create(const dcsmq_config_t & conf){
	//sender : recver (1:2) : client
	int prj_id[] = { 1, 2 };
	if (conf.passive){
		prj_id[0] = 2;
		prj_id[1] = 1;
	}
	int flag = 0666;
	if (!conf.attach){
		flag |= IPC_CREAT;
    }
    key_t sender_key = -1;
    if (dcsutil::strisint(conf.keypath)){
        sender_key = stoi(conf.keypath);
    }
    else {
        sender_key = ftok(conf.keypath.c_str(), prj_id[0]);
    }
	if (sender_key == -1){
		//error no
		GLOG_ERR( "ftok error key:%s , prj_id:%d",
			conf.keypath.c_str(), prj_id[0]);
		return nullptr;
	}
	int sender = _msgq_create(sender_key, flag, conf.max_queue_buff_size);
	if (sender < 0){
		//errno
		GLOG_ERR( "create msgq sender error flag :%d buff size:%u",
			flag, conf.max_queue_buff_size);
		return nullptr;
	}
	GLOG_TRA("create sender with key:%s(%d) , prj_id:%d",
		conf.keypath.c_str(), sender_key, prj_id[0]);

	key_t receiver_key = ftok(conf.keypath.c_str(), prj_id[1]);
    int recver = _msgq_create(receiver_key, flag, conf.max_queue_buff_size);
	if (recver < 0){
		//errno
		GLOG_ERR( "create msgq recver error flag :%d buff size:%u",
			flag, conf.max_queue_buff_size);
		return nullptr;
	}
	GLOG_TRA("create recver with key:%s(%d) , prj_id:%d",
        conf.keypath.c_str(), receiver_key, prj_id[1]);

	dcsmq_t * smq = new dcsmq_t();
	if (!smq){
		//memalloc
		GLOG_FTL("malloc error");
		return nullptr;
	}
	smq->sendbuff = (msgbuf	*)malloc(conf.msg_buffsz);
	smq->recvbuff = (msgbuf	*)malloc(conf.msg_buffsz);
	if (!smq->sendbuff ||
		!smq->recvbuff){
		//mem alloc
		GLOG_FTL("malloc error");
		return nullptr;
	}
	smq->conf = conf;
	smq->sender = sender;
	smq->recver = recver;
    //=============stat=======================
    smq->stat.sender_key = sender_key;
    smq->stat.receiver_key = receiver_key;
	return smq;
}
void    dcsmq_destroy(dcsmq_t* smq){
	if (smq->sender >= 0){
		//no need
        void();
	}
	if (smq->recver >= 0){
		//no need
        void();
    }
	if (smq->sendbuff){
		free(smq->sendbuff);		
	}
	if (smq->recvbuff){
		free(smq->recvbuff);
	}
	smq->init();
	delete smq;
}
void    dcsmq_msg_cb(dcsmq_t * smq, dcsmq_msg_cb_t cb, void * ud){
	smq->msg_cb = cb;
	smq->msg_cb_ud = ud;
}
int     dcsmq_poll(dcsmq_t*  smq, int max_time_us){	
	PROFILE_FUNC();
	int64_t past_us = 0, start_us, now_us;
	start_us = dcsutil::time_unixtime_us();
	ssize_t msg_sz = 0;
	int nproc = 0, ntotal_proc = 0;
	while (past_us < max_time_us){
		msg_sz = msgrcv(smq->recver, smq->recvbuff, smq->conf.msg_buffsz, smq->session, IPC_NOWAIT);
		if (msg_sz <= 0){
			if (errno == EINTR){
				continue;
			}
            //error occur , must be break .
            if (errno != ENOMSG){ //normal error 
                STAT_ON_RECV_ERROR(); //current error
                if (errno == E2BIG){
                    GLOG_ERR("msg recv a too big msg , has been ignore it !");
                    msg_sz = msgrcv(smq->recver, smq->recvbuff, smq->conf.msg_buffsz, smq->session, IPC_NOWAIT | MSG_NOERROR);
                    continue;
                }
                else{
                    GLOG_ERR("msg recv error recever:%d! ", smq->recver);
                }
            }
            break;
		}
		else {
            msgbuf * buf = (msgbuf*)(smq->recvbuff);
            STAT_ON_RECV(buf->mtype, msg_sz);
			GLOG_TRA("recv msgq msg from (type) :%lu size:%zu", buf->mtype, msg_sz);
			smq->msg_cb(smq, buf->mtype, dcsmq_msg_t(buf->mtext, msg_sz), smq->msg_cb_ud);
		}
		++nproc;
		++ntotal_proc;
		if (nproc >= 16){
			now_us = dcsutil::time_unixtime_us();
			past_us +=  (now_us - start_us);
			start_us = now_us;
			nproc = 0;
		}
	}
	return ntotal_proc;
}
int		dcsmq_push(dcsmq_t* smq, uint64_t dst, const dcsmq_msg_t & msg){
    if (msg.sz > smq->conf.msg_buffsz){
        return -1;
    }
    if (dst == 0){
        return -2;
    }
    smq->sendbuff->mtype = dst;
    memcpy(smq->sendbuff->mtext, msg.buffer, msg.sz);
    int ret = 0;
    do{
        ret = msgsnd(smq->recver, smq->sendbuff, msg.sz, IPC_NOWAIT);
    } while (ret == -1 && errno == EINTR);
    //============stat====================
    if (ret){
        STAT_ON_SEND_ERROR();
    }
    else{
        STAT_ON_SEND(dst, msg.sz);
    }
	return ret;
}
int     dcsmq_send(dcsmq_t* smq, uint64_t dst, const dcsmq_msg_t & msg){
	if (msg.sz > smq->conf.msg_buffsz){
		//size error
		return -1;
	}
	if (dst == 0){
		//dst error
		return -2;
	}
	smq->sendbuff->mtype = dst;	
	memcpy(smq->sendbuff->mtext, msg.buffer, msg.sz);
	int ret = 0;
	do {
		ret = msgsnd(smq->sender, smq->sendbuff, msg.sz, IPC_NOWAIT);
	} while (ret == -1 && errno == EINTR);
    //============stat====================
    if (ret){
        STAT_ON_SEND_ERROR();
    }
    else{
        STAT_ON_SEND(dst, msg.sz);
    }

	return ret;
}
bool	dcsmq_server_mode(dcsmq_t * smq){
	return smq->conf.passive;
}
uint64_t	dcsmq_session(dcsmq_t * smq){
	if (!smq) { return 0; }
	if (smq->conf.passive){ return 0; }
	return smq->session;
}
void	dcsmq_set_session(dcsmq_t * smq, uint64_t session){
	if (!smq->conf.passive){
		smq->session = session;
	}
}
const dcsmq_stat_t *	dcsmq_stat(dcsmq_t * smq){
    return &smq->stat;
}