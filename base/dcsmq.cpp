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
    smq->stat.msg_rtime = ::dcs::time_unixtime_us(); \
    ++smq->stat.recv_num; \
    smq->stat.recv_size += msize; \
    smq->stat.recv_last = mtype; \
}while (false)

#define STAT_ON_RECV_ERROR()  do{\
    smq->stat.msg_rtime = ::dcs::time_unixtime_us(); \
    ++smq->stat.recv_num; \
    ++smq->stat.recv_error;\
}while (false)

#define STAT_ON_SEND(mtype, msize)  do{\
    smq->stat.msg_stime = ::dcs::time_unixtime_us(); \
    ++smq->stat.send_num; \
    smq->stat.send_size += msize; \
    smq->stat.send_last = mtype; \
}while (false)

#define STAT_ON_SEND_ERROR()  do{\
    smq->stat.msg_stime = ::dcs::time_unixtime_us(); \
    ++smq->stat.send_num; \
    ++smq->stat.send_error; \
}while (false)


int		_msgq_create(key_t key, int flag, size_t max_size){
	int id = msgget(key, flag);
	if (id < 0){
		//get error
		GLOG_SER( "msg get error key:%d !", key);
		return -1;
	}
	struct msqid_ds mds;
	int ret = msgctl(id, IPC_STAT, (struct msqid_ds *)&mds);
	if (ret != 0){
		GLOG_SER( "msgctl error id:%d !", id);
		return -2;
	}

	if (mds.msg_qbytes < max_size){
        GLOG_WAR("msgq channel size:%zd is less than :%d reset it match .", mds.msg_qbytes, max_size);
		mds.msg_qbytes = max_size;
		ret = msgctl(id, IPC_SET, (struct msqid_ds *)&mds);
		if (ret != 0){
			GLOG_SER( "msgctl error id:%d when set size:%zd !", id, max_size);
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
    int sender = -1, recver = -1;
    key_t receiver_key = -1;
    key_t sender_key = -1;
    int send_queue_buff_size = conf.max_queue_buff_size;
    int recv_queue_buff_size = conf.min_queue_buff_size;
    if (conf.passive){
        recv_queue_buff_size = conf.max_queue_buff_size;
        send_queue_buff_size = conf.min_queue_buff_size;
    }
    if (send_queue_buff_size > 0){
        if (dcs::strisint(conf.keypath)){
            sender_key = stoi(conf.keypath)*1000 + prj_id[0] - 1;
        }
        else {
			sender_key = dcs::path_token(conf.keypath.c_str(), prj_id[0]);
        }
        if (sender_key == -1){
            //error no
            GLOG_ERR("ftok error key:%s , prj_id:%d",
                conf.keypath.c_str(), prj_id[0]);
            return nullptr;
        }

        sender = _msgq_create(sender_key, flag, send_queue_buff_size);
        if (sender < 0){
            //errno
            GLOG_ERR("create msgq sender error flag :%d buff size:%u",
                flag, send_queue_buff_size);
            return nullptr;
        }
        GLOG_TRA("create sender with key:%s(%d) , prj_id:%d",
            conf.keypath.c_str(), sender_key, prj_id[0]);
    }
    if (recv_queue_buff_size > 0){
        if (dcs::strisint(conf.keypath)){
            receiver_key = stoi(conf.keypath)*1000 + prj_id[1] - 1;
        }
        else {
            receiver_key = dcs::path_token(conf.keypath.c_str(), prj_id[1]);
        }
        if (receiver_key == -1){
            //error no
            GLOG_ERR("ftok error key:%s , prj_id:%d",
                conf.keypath.c_str(), prj_id[1]);
            return nullptr;
        }
        recver = _msgq_create(receiver_key, flag, recv_queue_buff_size);
        if (recver < 0){
            //errno
            GLOG_ERR("create msgq recver error flag :%d buff size:%u",
                flag, recv_queue_buff_size);
            return nullptr;
        }
        GLOG_TRA("create recver with key:%s(%d) , prj_id:%d",
            conf.keypath.c_str(), receiver_key, prj_id[1]);
    }
    if (recver == -1 && sender == -1){
        GLOG_ERR("no create msgq from send and recver is inited state , pls checking the config ...");
        return nullptr;
    }

	dcsmq_t * smq = new dcsmq_t();
	if (!smq){
		//memalloc
		GLOG_FTL("malloc dcsmsg error");
		return nullptr;
	}
	smq->sendbuff = (msgbuf	*)malloc(conf.msg_buffsz + sizeof(size_t));
	smq->recvbuff = (msgbuf	*)malloc(conf.msg_buffsz + sizeof(size_t));
	if (!smq->sendbuff ||
		!smq->recvbuff){
		//mem alloc
        if (smq->sendbuff) {
            free(smq->sendbuff);
        }
        if (smq->recvbuff) {
            free(smq->recvbuff);
        }
        delete smq;
		GLOG_FTL("malloc dcsmq msg buffer error");
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
static inline  uint64_t
_dcsmq_recv_msg(dcsmq_t * smq, int msgid, uint64_t session, dcsmq_msg_t & msg, bool block){
    ssize_t msg_sz = -1;
RETRY_RECV:
    if(block){
        msg_sz = msgrcv(msgid, msg.buffer, msg.sz, session, 0);
    }
    else {
        msg_sz = msgrcv(msgid, msg.buffer, msg.sz, session, IPC_NOWAIT);
    }
    if (msg_sz <= 0){
        if (errno == EINTR){
            return -1;
        }
        //error occur , must be break .
        if (errno != ENOMSG){ //normal error 
            STAT_ON_RECV_ERROR(); //current error
            if (errno == E2BIG){
                GLOG_ERR("msg recv a too big msg , has been ignore it !");
                msg_sz = msgrcv(msgid, msg.buffer, msg.sz, session, IPC_NOWAIT | MSG_NOERROR);
                goto RETRY_RECV;
            }
            else{
                GLOG_SER("msg recv error recever:%d session:%d errno:%d ! ", smq->recver, session, errno);
            }
        }
        return -1;
    }
    else {
        msgbuf * buf = (msgbuf*)(msg.buffer);
        STAT_ON_RECV(buf->mtype, msg_sz);
        msg.buffer += sizeof(buf->mtype);
        msg.sz = msg_sz;
        GLOG_TRA("recv msgq msg from (type) :%lu size:%zd", buf->mtype, msg_sz);
        return buf->mtype;
    }
}
int     dcsmq_poll(dcsmq_t*  smq, int max_time_us){	
	PROFILE_FUNC();
	int64_t past_us = 0, start_us, now_us;
	start_us = dcs::time_unixtime_us();
	int nproc = 0, ntotal_proc = 0;
    dcsmq_msg_t dcmsg;
    uint64_t recvmsgid;
	while (past_us < max_time_us){
        dcmsg.buffer = (char*)smq->recvbuff;
        dcmsg.sz = smq->conf.msg_buffsz;
        recvmsgid = _dcsmq_recv_msg(smq, smq->recver, smq->session, dcmsg, false);
        if (recvmsgid == (uint64_t)-1){
            break;
        }
        smq->msg_cb(smq, recvmsgid, dcmsg, smq->msg_cb_ud);
        ++nproc;
		++ntotal_proc;
		if (nproc >= 16){
			now_us = dcs::time_unixtime_us();
			past_us +=  (now_us - start_us);
			start_us = now_us;
			nproc = 0;
		}
	}
	return ntotal_proc;
}
uint64_t	 dcsmq_take(dcsmq_t* smq, dcsmq_msg_t & msg, bool block) {//send to peer like himself
    return _dcsmq_recv_msg(smq, smq->sender, smq->session, msg, block);
}
uint64_t     dcsmq_recv(dcsmq_t* smq, dcsmq_msg_t & msg, bool block){
    return _dcsmq_recv_msg(smq, smq->recver, smq->session, msg, block);
}
int     _dcsmq_sendv(dcsmq_t* smq, int msgid, uint64_t dst, const std::vector<dcsmq_msg_t> & msgv) {
    if (dst == 0) {
        GLOG_ERR("send msg error dst must not 0!");
        return -1;
    }
    int totalsz = 0;
    smq->sendbuff->mtype = dst;
    for (int i = 0; i < (int)msgv.size(); ++i) {
        const dcsmq_msg_t & msg = msgv[i];
        if ((totalsz + msg.sz) > smq->conf.msg_buffsz) {
            GLOG_ERR("send msg total:%d error sz:%d max :%d", totalsz, msg.sz, smq->conf.msg_buffsz);
            return -2;
        }
        memcpy(smq->sendbuff->mtext + totalsz, msg.buffer, msg.sz);
        totalsz += msg.sz;
    }
    int ret = 0;
    do {
        ret = msgsnd(msgid, smq->sendbuff, totalsz, IPC_NOWAIT);
    } while (ret == -1 && errno == EINTR);
    //============stat====================
    if (ret) {
        STAT_ON_SEND_ERROR();
    }
    else {
        STAT_ON_SEND(dst, totalsz);
    }
    return ret;
}
int		dcsmq_put(dcsmq_t* smq, uint64_t dst, const dcsmq_msg_t & msg){
    std::vector<dcsmq_msg_t> msgv;
    msgv.push_back(msg);
    return _dcsmq_sendv(smq, smq->recver, dst, msgv);
}
int     dcsmq_send(dcsmq_t* smq, uint64_t dst, const dcsmq_msg_t & msg){
    std::vector<dcsmq_msg_t> msgv;
    msgv.push_back(msg);
    return _dcsmq_sendv(smq, smq->sender, dst, msgv);
}
int         dcsmq_sendv(dcsmq_t* smq, uint64_t dst, const std::vector<dcsmq_msg_t> & msgv){
    return _dcsmq_sendv(smq, smq->sender, dst, msgv);
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
        GLOG_TRA("smg set session:%lu", session);
		smq->session = session;
	}
}
const dcsmq_stat_t *	dcsmq_stat(dcsmq_t * smq){
    if (::dcs::time_unixtime_ms() - smq->stat.ipc_stat_time >= 10 * 1000){
        smq->stat.ipc_stat_time = ::dcs::time_unixtime_ms();
        smq->stat.msg_cur_bytes = 0;
        smq->stat.msg_max_bytes = 0;
        struct msqid_ds mds;
        int ret = msgctl(smq->sender, IPC_STAT, (struct msqid_ds *)&mds);
        if (ret != 0){
            GLOG_SER("msgctl error id:%d !", smq->sender);
        }
        else{
            smq->stat.msg_cur_bytes = mds.msg_cbytes;
            smq->stat.msg_max_bytes = mds.msg_qbytes;
        }
    }
    return &smq->stat;
}
