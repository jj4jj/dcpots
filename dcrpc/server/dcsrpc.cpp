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
struct RpcServiceCallContext {
	int fd;
	dcrpc_msg_t msg;
	RpcServiceCallContext(int fd_, const dcrpc_msg_t & m) :fd(fd_){
		msg.CopyFrom(m);
	}
};
struct RpcServerImpl{
    dctcp_t * stcp_looper{ nullptr };
    bool      stcp_looper_own{ false };
    std::unordered_map<string, RpcService *> dispatcher;
    msg_buffer_t       send_buff;
    uint64_t           update_tick_time{ 0 };
	/////////////////////////////////////////////////////////////////////////////
	std::multimap<int, uint64_t>		fd_pending_cookie;
	std::unordered_map<uint64_t, RpcServiceCallContext *>	cookie_pending_context;
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
    int ret = dctcp_send(impl->stcp_looper, id, dctcp_msg_t(impl->send_buff.buffer, impl->send_buff.valid_size));
    GLOG_TRA("send [%d] [%d] [%s] [%s] [%p:%d]", ret, id, rpc_msg.path().c_str(),
        rpc_msg.Debug(), impl->send_buff.buffer, impl->send_buff.valid_size);
    if (ret){
        GLOG_SER("rpc reply msg [%s] error:%d !", rpc_msg.Debug(), ret);
    }
    return ret;
}
static inline void async_rpc_yield_call(RpcServerImpl * impl, uint64_t cookie, int fd, const dcrpc_msg_t & msg){
	if (impl->cookie_pending_context.find(cookie) != impl->cookie_pending_context.end()){
		GLOG_ERR("cookie :%lu is in pennding ...", cookie);
		return;
	}
	GLOG_TRA("rpc yield call append context fd:%d -> cookie:%lu ", fd, cookie);
	assert(impl->cookie_pending_context.find(cookie) == impl->cookie_pending_context.end());
	impl->fd_pending_cookie.insert(std::make_pair(fd, cookie));
	impl->cookie_pending_context[cookie] = new RpcServiceCallContext(fd, msg);
}
static inline void async_rpc_resume_call(RpcServerImpl * impl, uint64_t cookie){
	auto itcookie = impl->cookie_pending_context.find(cookie);
	if (itcookie == impl->cookie_pending_context.end()){
		return;
	}
	auto fd = itcookie->second->fd;
	GLOG_TRA("rpc resume call free context fd:%d -> cookie:%lu ", fd, cookie);
	auto range = impl->fd_pending_cookie.equal_range(fd);
	auto it = range.first;
	while (it != range.second){
		if (it->second == cookie){
			delete itcookie->second; //free context
			impl->cookie_pending_context.erase(itcookie); //free cookie -> context
			impl->fd_pending_cookie.erase(it); //free fd->cookie
			return;
		}
		++it;
	}
}
static inline void async_rpc_connection_lost(RpcServerImpl * impl, int fd){
	auto range = impl->fd_pending_cookie.equal_range(fd);
	auto it = range.first;
	while (it != range.second){
		auto cookieit = impl->cookie_pending_context.find(it->second);
		assert(cookieit != impl->cookie_pending_context.end());
		GLOG_WAR("connection lost erasing rpc context fd:%d -> cookie:%lu ", fd, cookieit->first);
		delete cookieit->second;
		impl->cookie_pending_context.erase(cookieit);
		++it;
	}
	impl->fd_pending_cookie.erase(range.first, range.second);
}
static inline RpcServiceCallContext * async_rpc_get_context(RpcServerImpl * impl, uint64_t cookie){
	auto it = impl->cookie_pending_context.find(cookie);
	if (it == impl->cookie_pending_context.end()){
		GLOG_ERR("async rpc call context lost cookie:%lu", cookie);
		return nullptr;
	}
	return it->second;
}
int	   RpcServer::reply(RpcService *, uint64_t cookie, const RpcValues & result, int ret, const char * error){
	RpcServiceCallContext * ctx = async_rpc_get_context(impl, cookie);
	if (ctx){
		dcrpc_msg_t & rpc_msg = ctx->msg;
		rpc_msg.mutable_response()->set_status(ret);
		if (error){
			rpc_msg.mutable_response()->set_error(error);
		}
		auto msg_result = rpc_msg.mutable_response()->mutable_result();
		msg_result->CopyFrom(*(decltype(msg_result))result.data());
		rpc_msg.clear_request();
		ret = _send_msg(impl, ctx->fd, rpc_msg);
		async_rpc_resume_call(impl, cookie);
		return ret;
	}
	else {
		return -1;
	}
}
static inline void _dispatch_client_close(RpcServerImpl * impl, int fd){
    auto it = impl->dispatcher.begin();
    while (it != impl->dispatcher.end()){
        auto cit = it++;
        auto service = cit->second;
        service->clientclosed(fd);
    }
}
static inline void _dispatch_client_update(RpcServerImpl * impl){
    auto it = impl->dispatcher.begin();
    while (it != impl->dispatcher.end()){
        auto cit = it++;
        auto service = cit->second;
        service->update();
    }
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
        RpcValues args((const RpcValuesImpl &)(rpc_msg.request().args()));
		RpcService * service = it->second;
		uint64_t transac_cookie = rpc_msg.cookie().transaction();
		if (service->isasync()){
			if (transac_cookie == 0){
				GLOG_ERR("transaction is 0 but in a async call ... %s", rpc_msg.Debug());
				return;
			}
			else {
				async_rpc_yield_call(impl, transac_cookie, fd, rpc_msg);
				int ret = service->yield(transac_cookie,
					args, *rpc_msg.mutable_response()->mutable_error(), fd);
				rpc_msg.mutable_response()->set_status(ret);
				if (ret == 0){
					return;
				}
				else {
					rpc_msg.clear_request();
				}
			}
		}
		else {
			RpcValues result;
			rpc_msg.mutable_response()->set_status(
				service->call(result, args,
                *rpc_msg.mutable_response()->mutable_error(), fd));
			auto msg_result = rpc_msg.mutable_response()->mutable_result();
			msg_result->CopyFrom(*(decltype(msg_result))result.data());
			rpc_msg.clear_request();
		}
    }
    if (rpc_msg.cookie().transaction() > 0){
        _send_msg(impl, fd, rpc_msg);
    }
}
static int rpc_server_dispatch(dctcp_t*, const dctcp_event_t & ev, void * ud) {
    RpcServerImpl * impl = (RpcServerImpl*)ud;
    switch (ev.type){
    case DCTCP_NEW_CONNX:
        break;
    case DCTCP_CLOSED:
        _dispatch_client_close(impl, ev.fd);
        break;
    case DCTCP_READ:
        GLOG_TRA("read tcp msg fd=%d buff:%p ibuff:%d", ev.fd, ev.msg->buff, ev.msg->buff_sz);
        _distpach_remote_service_cmsg(impl, ev.fd, ev.msg->buff, ev.msg->buff_sz);
        break;
    default:
        assert(false && "error event !");
        return -1;
    }
    return 0;
}
int    RpcServer::init(const std::string & addr, dctcp_t * stcp){
    if (this->impl){
        GLOG_ERR("error init server again £¡");
        return -1;
    }
    impl = new RpcServerImpl();

    impl->stcp_looper = stcp;
    if (!stcp){
        dctcp_config_t dctcp;
        dctcp.max_recv_buff = 1024 * 1024 * 2;
        dctcp.max_send_buff = 1024 * 1024 * 1;
        dctcp.max_tcp_recv_buff_size = 1 * 1024 * 1024;
        dctcp.max_tcp_send_buff_size = 1 * 1024 * 1024;
        impl->stcp_looper = dctcp_create(dctcp);
        if (!impl->stcp_looper){
            return -2;
        }
        impl->stcp_looper_own = true;
    }
    impl->send_buff.create(dcrpc::MAX_RPC_MSG_BUFF_SIZE);
    int ret = dctcp_listen(impl->stcp_looper, addr, "msg:sz32", rpc_server_dispatch, impl);
    if (ret < 0){
        GLOG_ERR("bind address and start listening error :%d !", ret);
        return -3;
    }
    return 0;
}
int    RpcServer::update(int tick_us){
    int ret = 0;
    uint64_t t_now_us = dcsutil::time_unixtime_us();
    if (impl->update_tick_time + tick_us > t_now_us){
        return 0;
    }
    impl->update_tick_time = t_now_us;
    if (impl->stcp_looper_own){
        ret = dctcp_poll(impl->stcp_looper, tick_us);
    }
    _dispatch_client_update(impl);
    return ret;
}
void   RpcServer::destroy(){
    if (impl){
        if (impl->stcp_looper){
            dctcp_destroy(impl->stcp_looper);
            impl->stcp_looper = nullptr;
        }
        impl->send_buff.destroy();
        delete impl;
        impl = nullptr;
    }
}
int    RpcServer::push(const std::string & svc, int id, const RpcValues & vals){
    dcrpc_msg_t rpc_msg;
    rpc_msg.set_path(svc);
    rpc_msg.set_status(RpcMsg_StatusCode_RPC_STATUS_SUCCESS);
    auto result = rpc_msg.mutable_notify()->mutable_result();
    result->CopyFrom(*(decltype(result))vals.data());
    return _send_msg(impl, id, rpc_msg);
}
///////////////////////////////////////////////////////////////////////////////////////
struct RpcServiceImpl {
	RpcServer * svr { nullptr };
	string	name;
	bool	isasync{ false };
};
RpcService::RpcService(const std::string  & name_, bool async_call){
	impl_ = new RpcServiceImpl();
	impl_->name = name_;
	impl_->isasync = async_call;
	impl_->svr = nullptr;
}
RpcService::~RpcService(){
	if (impl_){
		delete impl_;
	}
}
bool	RpcService::isasync() const {
	return impl_->isasync;
}
const std::string & RpcService::name() const {
	return impl_->name;
}
int	RpcService::resume(uint64_t cookie, const RpcValues & result, int ret, const char * error){
	return this->impl_->svr->reply(this, cookie, result, ret, error);
}
int RpcService::call(dcrpc::RpcValues & result, const dcrpc::RpcValues & args, string & error, int clientid){
    result = args;
	UNUSED(error);
    UNUSED(clientid);
    return 0;
}
int RpcService::yield(uint64_t cookie, const RpcValues & args, std::string & error, int clientid){
	UNUSED(error);
    UNUSED(clientid);
    return resume(cookie, args);
}
void RpcService::clientclosed(int clientid){
    UNUSED(clientid);
}
void RpcService::update(){
}


///////////////////////////////////////////////////////////////////////////////////////
int    RpcServer::regis(RpcService * svc){
	if (impl->dispatcher.find(svc->name()) != impl->dispatcher.end()){
		return -1;
	}
	impl->dispatcher[svc->name()] = svc;
	const_cast<RpcServiceImpl*>(svc->impl())->svr = this;
	return 0;
}
NS_END()
