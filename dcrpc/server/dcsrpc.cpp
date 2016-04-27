#include "../../base/stdinc.h"
#include "../../base/logger.h"
#include "../../base/dcutils.hpp"
#include "../../base/dctcp.h"


#include "dcsrpc.h"

NS_BEGIN(dcsutil)

struct RpcServerImpl{
    dctcp_t * svr { nullptr };
    std::unordered_map<string, RpcService *> dispatcher;
};


RpcServer::RpcServer(){
    this->impl = nullptr;
}
RpcServer::~RpcServer(){
    destroy();
}
int    RpcServer::init(){
    if (this->impl){
        GLOG_ERR("error init server again £¡");
        return -1;
    }
    impl = new RpcServerImpl();
    dctcp_config_t dctcp;
    dctcp.server_mode = true;
    dctcp.max_recv_buff = 1024 * 1024 * 2;
    dctcp.max_send_buff = 1024 * 1024 * 1;
    dctcp.max_tcp_recv_buff_size = 512 * 1024;
    dctcp.max_tcp_send_buff_size = 1 * 1024 * 1024;
    impl->svr = dctcp_create(dctcp);
    if (!impl->svr){
        return -2;
    }
    
    dctcp_event_cb(impl->svr, [](dctcp_t* dc, const dctcp_event_t & ev, void * ud)->int {
        GLOG_DBG("log event :%d ", ev.type);
        switch (ev.type){
        case DCTCP_CONNECTED:
            assert(false && "no connected event !");
            break;
        case DCTCP_NEW_CONNX:
            break;
        case DCTCP_CLOSED:
            break;
        case DCTCP_READ:
            GLOG_IFO("dispatch svc:%s", ev.msg->buff);
            [dc, &ev](const char * svc){
                GLOG_IFO("call svc:%s", svc);
                dctcp_send(dc, ev.fd, dctcp_msg_t("hello", 5));
            }(ev.msg->buff);
            break;
        case DCTCP_WRITE:
            break;
        default:
            assert(false && "error event !");
            return -1;
        }
        return 0;
    }, nullptr);
    
    if (dctcp_listen(impl->svr, "127.0.0.1:8888") < 0){
        GLOG_ERR("bind address and start listening error !");
        return -3;
    }
    return 0;
}
int    RpcServer::update(){
    return dctcp_poll(impl->svr, 1000 * 10);
}
void   RpcServer::destroy(){
    if (impl){
        if (impl->svr){
            dctcp_destroy(impl->svr);
            impl->svr = nullptr;
        }
        delete impl;
        impl = nullptr;
    }
}
int    RpcServer::regis(RpcService * svc){
    impl->dispatcher[svc->name()] = svc;
}



RpcService::RpcService(const std::string  & name_){
    this->name_ = name_;
}



NS_END()
