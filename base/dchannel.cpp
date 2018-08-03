
#include "stdinc.h"
#include "dctcp.h"
#include "dcutils.hpp"
#include "dcsmq.h"
#include "logger.h"
#include "msg_buffer.hpp"
#include "dchannel.h"

NS_BEGIN(dcs)

enum ChannelPointType {
    CHANNEL_POINT_UDP = 0,
    CHANNEL_POINT_TCP = 1,
    CHANNEL_POINT_SMQ = 2,
};

struct ChannelPoint {
    uint32_t            session_id {0};
    ChannelPointType    cpt;
    int                 fd {-1};
    dcsmq_t     *       mq {nullptr};
    OpenFuture          on_open {nullptr};
    CloseFuture         on_close{ nullptr };
    RecvFuture          on_recv{ nullptr };
    msg_buffer_t        msgbuff;
};

struct AsyncChannelImpl {
    uint32_t        session_id_seq {0};
    dctcp_t *       stcp {nullptr};
    std::vector<ChannelPoint*>  vecPassivePoints;

};

static void _CheckInit(AsyncChannelImpl * * pimpl){
    if( nullptr == *pimpl){
        *pimpl = new AsyncChannelImpl();
    }
}

static int dcsmq_on_msg(dcsmq_t *, uint64_t src, const dcsmq_msg_t & msg, void * ud){
    ChannelPoint * cp = (ChannelPoint*)(ud);
    if(cp->on_recv){
        cp->on_recv(cp, msg.buffer, msg.sz);
    }
    return 0;
}

ChannelPoint * dcs::AsyncChannel::Open(const char * addr, int max_channel_size, OpenFuture onopen, CloseFuture onclose){
    _CheckInit(&impl);
    if(strstr(addr, "smq://") == addr){
        dcsmq_config_t    dcc;
        dcc.keypath = addr+6;
        dcc.attach = false;
        dcc.passive = false;
        dcc.msg_buffsz = 1024*1024;
        dcc.max_queue_buff_size = max_channel_size;
        dcc.min_queue_buff_size = 1024*1024;
        dcsmq_t * smq = dcsmq_create(dcc);
        if(!smq){
            GLOG_ERR("create smq error !");
            return nullptr;
        }
        uint32_t session_id = getpid();
        dcsmq_set_session(smq, session_id);
        auto cp = new ChannelPoint();
        cp->cpt = CHANNEL_POINT_SMQ;
        cp->mq = smq;
        cp->session_id = session_id;
        return cp;
    }
    return nullptr;
}

ChannelPoint * dcs::AsyncChannel::Listen(const char * addr, int max_channel_size, RecvFuture onrecv, CloseFuture onclose){
    _CheckInit(&impl);
    if (strstr(addr, "smq://") == addr) {
        dcsmq_config_t    dcc;
        dcc.keypath = addr + 6;
        dcc.attach = false;
        dcc.passive = true;
        dcc.msg_buffsz = 1024 * 1024;
        dcc.max_queue_buff_size = max_channel_size;
        dcc.min_queue_buff_size = 1024*1024;
        dcsmq_t * smq = dcsmq_create(dcc);
        if (!smq) {
            GLOG_ERR("create smq error !");
            return nullptr;
        }
        auto cp = new ChannelPoint();
        cp->cpt = CHANNEL_POINT_SMQ;
        cp->mq = smq;
        cp->on_recv = onrecv;
        cp->msgbuff.create(dcc.msg_buffsz);
        dcsmq_msg_cb(smq, dcsmq_on_msg, cp);
        impl->vecPassivePoints.push_back(cp);
        return cp;
    }
    return nullptr;
}

void dcs::AsyncChannel::Close(ChannelPoint * point){
    if(!impl){return;}
    for (int i = 0; i < (int)impl->vecPassivePoints.size(); ++i){
        if(point == impl->vecPassivePoints[i]){
            impl->vecPassivePoints.erase(impl->vecPassivePoints.begin()+i);
            break;
        }
    }
    if(point->cpt == CHANNEL_POINT_SMQ){
        dcsmq_destroy(point->mq);
        delete point;
    }
}

int dcs::AsyncChannel::Send(ChannelPoint * point, const char * buff, int ibuff){
    if(!impl || !point){return -1;}
    //int ret = 0 ;
    if(point->cpt == CHANNEL_POINT_SMQ){
        return  dcsmq_send(point->mq, point->session_id, dcsmq_msg_t((char*)buff, ibuff));
    }

    return 0;
}

int dcs::AsyncChannel::Recv(ChannelPoint * point, char * buff, int maxbuff, bool block){
    if(!point){return -1;}
    if(point->cpt == CHANNEL_POINT_SMQ){
        dcsmq_msg_t recv_msg(point->msgbuff.buffer, point->msgbuff.max_size);
        uint64_t session = dcsmq_recv(point->mq, recv_msg, block);
        if(session == (uint64_t)-1){
            return -2;
        }
        if(recv_msg.sz > maxbuff){
            return -3;
        }
        memcpy(buff, recv_msg.buffer, recv_msg.sz);
        return recv_msg.sz;
    }
    return -4;
}

int dcs::AsyncChannel::Update(){
    if(!impl){return 0;}
    //smq
    int npoll = 0;
    for(int i = 0 ;i < (int)impl->vecPassivePoints.size(); ++i){
        ChannelPoint *  cp = impl->vecPassivePoints[i];
        if(nullptr == cp->on_recv){
            continue;
        }
        if(cp->cpt == CHANNEL_POINT_SMQ){
            npoll += dcsmq_poll(cp->mq, 1000);
        }
    }
    return 0;
}


NS_END()