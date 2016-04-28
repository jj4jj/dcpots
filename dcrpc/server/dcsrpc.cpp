#include "../../base/stdinc.h"
#include "../../base/logger.h"
#include "../../base/dcutils.hpp"
#include "../../base/dctcp.h"
#include "../../base/msg_buffer.hpp"
#include "../../base/msg_proto.hpp"
////////////////////////////////////////
#include "../share/dcrpc.pb.h"
#include "../share/dcrpc.h"
#include "dcsrpc.h"

typedef msgproto_t<dcrpc::RpcMsg> dcrpc_msg_t;

NS_BEGIN(dcrpc)
struct RpcServerImpl{
    dctcp_t * svr { nullptr };
    std::unordered_map<string, RpcService *> dispatcher;
    msg_buffer_t       send_buff;
};

RpcServer::RpcServer(){
    this->impl = nullptr;
}
RpcServer::~RpcServer(){
    destroy();
}
static inline int _send_msg(RpcServerImpl * impl, int id, const dcrpc_msg_t & rpc_msg ){
    if (!rpc_msg.Pack(impl->send_buff)){
        GLOG_ERR("pack rpc reply msg error ! buff length:%d", rpc_msg.ByteSize());
        return -1;
    }
    int ret = dctcp_send(impl->svr, id, dctcp_msg_t(impl->send_buff.buffer, impl->send_buff.valid_size));
    GLOG_TRA("send [%d] [%d] [%s] [%s] [%p:%d]", ret, id, rpc_msg.path().c_str(),
        rpc_msg.Debug(), impl->send_buff.buffer, impl->send_buff.valid_size);
    if (ret){
        GLOG_SER("rpc reply msg [%128s] error:%d !", rpc_msg.Debug(), ret);
    }
    return ret;
}

static inline void _distpach_remote_service_cmsg(RpcServerImpl * impl, int fd, const char * buff, int ibuff){
    dcrpc_msg_t rpc_msg;
    if (!rpc_msg.Unpack(buff, ibuff)){
        GLOG_ERR("unpack rpc msg error ! buff length:%d", ibuff);
        return;
    }
    GLOG_TRA("recv [%d] [%s] [%s]", fd, rpc_msg.path().c_str() , rpc_msg.Debug());
    auto it = impl->dispatcher.find(rpc_msg.path());
    if (it == impl->dispatcher.end()){//not found
        rpc_msg.clear_request();
        rpc_msg.set_status(RpcMsg_StatusCode_RPC_STATUS_NOT_EXIST);
    }
    else {
        rpc_msg.set_status(RpcMsg_StatusCode_RPC_STATUS_SUCCESS);        
        RpcValues result;
        RpcValues args((const RpcValuesImpl &)(rpc_msg.request().args()));
        rpc_msg.mutable_response()->set_status(
            it->second->call(result, args,
                rpc_msg.mutable_response()->mutable_error()));
        auto msg_result = rpc_msg.mutable_response()->mutable_result();
        msg_result->CopyFrom(*(decltype(msg_result))result.data());
        rpc_msg.clear_request();
    }
    if (rpc_msg.cookie().transaction() > 0){
        _send_msg(impl, fd, rpc_msg);
    }
}
int    RpcServer::init(const std::string & addr){
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
    impl->send_buff.create(dcrpc::MAX_RPC_MSG_BUFF_SIZE);
    dctcp_event_cb(impl->svr, [](dctcp_t* dc, const dctcp_event_t & ev, void * ud)->int {
        RpcServerImpl * impl = (RpcServerImpl*)ud;
        switch (ev.type){
        case DCTCP_CONNECTED:
            assert(false && "no connected event !");
            break;
        case DCTCP_NEW_CONNX:
            break;
        case DCTCP_CLOSED:
            break;
        case DCTCP_READ:
            GLOG_TRA("read tcp msg fd=%d buff:%p ibuff:%d", ev.fd, ev.msg->buff, ev.msg->buff_sz);
            _distpach_remote_service_cmsg(impl, ev.fd, ev.msg->buff, ev.msg->buff_sz);
            break;
        case DCTCP_WRITE:
            break;
        default:
            assert(false && "error event !");
            return -1;
        }
        return 0;
    }, impl);
    
    if (dctcp_listen(impl->svr, addr) < 0){
        GLOG_ERR("bind address and start listening error !");
        return -3;
    }
    return 0;
}
int    RpcServer::update(){
    return dctcp_poll(impl->svr, 1000);
}
void   RpcServer::destroy(){
    if (impl){
        if (impl->svr){
            dctcp_destroy(impl->svr);
            impl->svr = nullptr;
        }
        impl->send_buff.destroy();
        delete impl;
        impl = nullptr;
    }
}
int    RpcServer::regis(RpcService * svc){
    impl->dispatcher[svc->name()] = svc;
}
int    RpcServer::push(const std::string & svc, int id, const RpcValues & vals){
    dcrpc_msg_t rpc_msg;
    rpc_msg.set_path(svc);
    rpc_msg.set_status(RpcMsg_StatusCode_RPC_STATUS_SUCCESS);
    auto result = rpc_msg.mutable_notify()->mutable_result();
    result->CopyFrom(*(decltype(result))vals.data());
    return _send_msg(impl, id, rpc_msg);
}
///////////////////////////////////////////////////////////////////////////////////////////////
RpcService::RpcService(const std::string  & name_){
    this->name_ = name_;
}
int RpcService::call(dcrpc::RpcValues & result, const dcrpc::RpcValues & args, string * error){
    result = args;
    return 0;
}



NS_END()
