#include "../../base/stdinc.h"
#include "../../base/logger.h"
#include "../../base/dctcp.h"
#include "../../base/dcutils.hpp"
#include "dccrpc.h"
NS_BEGIN(dcsutil)

struct  RpcClientImpl {
    dctcp_t * cli{ nullptr };
    RpcClient::NotifyCallBack notify;
    int fd{ -1 };
};

RpcClient::RpcClient(){
    impl = nullptr;
}
RpcClient::~RpcClient(){
    destroy();
}

int RpcClient::init(){
    if (impl){
        return -1;
    }
    impl = new RpcClientImpl;
    dctcp_config_t dctcp;
    impl->cli = dctcp_create(dctcp);
    if (!impl->cli){
        return -2;
    }
    dctcp_event_cb(impl->cli, [](dctcp_t*, const dctcp_event_t & ev, void * ud)->int {
        RpcClientImpl * impl = (RpcClientImpl *)ud;
        switch (ev.type){
        case DCTCP_CONNECTED:
            impl->fd = ev.fd;
            break;
        case DCTCP_CLOSED:
            impl->fd = -1;
            GLOG_WAR("connection closed ! todo reconnecting ...");
            break;
        case DCTCP_READ:
            //msg->cookie->callcb
            GLOG_IFO("recv read msg:%s", ev.msg->buff);
            break;
        default:
            return -1;
        }
    }, this);

    return dctcp_connect(impl->cli, "127.0.0.1:8888", 3);
}
int RpcClient::update(){
    return    dctcp_poll(impl->cli, 1000 * 10);
}
int RpcClient::destroy(){
    if (impl){
        if (impl->cli){
            dctcp_destroy(impl->cli);
            impl->cli = nullptr;
        }
        delete impl;
        impl = nullptr;
    }
}
int RpcClient::notify(NotifyCallBack cb){
    impl->notify = cb;
    return 0;
}
int RpcClient::call(const string & svc){
    if (impl->fd == -1){
        return -1;
    }
    dctcp_send(impl->cli, impl->fd, dctcp_msg_t(svc.data(), svc.length()));
    return 0;
}


NS_END()