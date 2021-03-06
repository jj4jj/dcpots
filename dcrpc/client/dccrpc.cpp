#include "../../base/stdinc.h"
#include "../../base/logger.h"
#include "../../base/dctcp.h"
#include "../../base/dcutils.hpp"
#include "../../base/dcseqnum.hpp"
#include "../../base/msg_proto.hpp"
#include "../../base/msg_buffer.hpp"
/////////////////////////////////////////////////
#include "../share/dcrpc.pb.h"
#include "../share/dcrpc.h"
#include "dccrpc.h"
NS_BEGIN(dcrpc)
//using namespace google::protobuf;
using dcrpc::RpcValues;
typedef dcs::sequence_number_t<16, 16>    rpcall_transaction_sn;
typedef msgproto_t<RpcMsg>  dcrpc_msg_t;

////////////////////////////////////////////////////////////////////
struct  RpcClientImpl {
    dctcp_t * stcp{ nullptr };
    bool      stcp_owner{ false };
    int       cnnxfd{ -1 };    
    std::unordered_map<uint64_t, RpcClient::RpcCallNotify>  rpcall_cbs;
    std::unordered_map<string, RpcClient::RpcCallNotify>    notify_cbs;
    //todo timer queue
    msg_buffer_t send_msg_buff;
    std::vector<std::string> rpc_server_addrs;
    int  connect_server_retry{ 3 };
    std::queue<dcrpc_msg_t>  sending_queue;
    int  queue_max_size{ 0 };
    uint32_t    next_check_connx_time{ 0 };
public:
    const string select_server(){
        return rpc_server_addrs[rand() % rpc_server_addrs.size()];
    }
    RpcClientImpl(){
        send_msg_buff.create(dcrpc::MAX_RPC_MSG_BUFF_SIZE);
    }
    ~RpcClientImpl(){
        send_msg_buff.destroy();
    }

public:
    uint64_t append_callback(RpcClient::RpcCallNotify cb, int expired = 0){
        uint64_t sn = rpcall_transaction_sn::next(getpid());
        rpcall_cbs[sn] = cb;
		UNUSED(expired);
        return sn;
    }
    RpcClient::RpcCallNotify remove_callback(uint64_t transid){
        auto it = rpcall_cbs.find(transid);
        if (it == rpcall_cbs.end()){
            return nullptr;
        }
        auto ret = it->second;
        rpcall_cbs.erase(it);
        return ret;
    }
};

RpcClient::RpcClient(){
    impl = nullptr;
}
RpcClient::~RpcClient(){
    destroy();
}

static inline void 
_distpach_remote_service_smsg(RpcClientImpl * impl, const char * buff, int ibuff){
    dcrpc_msg_t rpc_msg;
    if (!rpc_msg.Unpack(msg_buffer_t(buff, ibuff))){
        GLOG_ERR("unpack rpc msg error ! buff:%s len:%d", buff, ibuff);
        return;
    }
    GLOG_TRA("rpc recv [%s]", rpc_msg.Debug());
    uint64_t transid = rpc_msg.cookie().transaction();
    if (transid > 0){ //call back
        auto cb = impl->remove_callback(transid);
        if (cb == nullptr){
            GLOG_ERR("not found callback trnasid:%lu", transid);
            return;
        }
        if (rpc_msg.status() == RpcMsg::StatusCode::RpcMsg_StatusCode_RPC_STATUS_SUCCESS){
            auto & response = rpc_msg.response();
            const RpcValuesImpl *data = (const RpcValuesImpl *)&response.result();
            RpcValues vals(*data);
            cb(response.status(), vals);
            if (response.status() != 0){
                GLOG_WAR("service :%s return error:%d transid:%lu", rpc_msg.path().c_str()
                    , response.status(), transid);
            }
        }
        else {
            GLOG_ERR("rpc call error svc:%s error:%d", rpc_msg.path().c_str(), rpc_msg.status());
        }
    }
    else { //notify
        auto it = impl->notify_cbs.find(rpc_msg.path());
        if (it == impl->notify_cbs.end()){
            GLOG_IFO("notify msg not register :%s", rpc_msg.path().c_str());
        }
        else {
            const RpcValuesImpl *data = (const RpcValuesImpl *)&rpc_msg.notify().result();
            RpcValues vals(*data);
            it->second(0, vals);
        }
    }
}
static int _rpc_client_stcp_dispatch(dctcp_t* dc, const dctcp_event_t & ev, void * ud){
    RpcClientImpl * impl = (RpcClientImpl *)ud;
    UNUSED(dc);
    switch (ev.type){
    case DCTCP_CONNECTED:
        impl->cnnxfd = ev.fd;
        break;
    case DCTCP_CLOSED:
        impl->cnnxfd = -2;
        GLOG_TRA("connection closed [reason:%d] ! ", ev.reason);
        break;
    case DCTCP_READ:
        GLOG_TRA("read rpc client fd:%d msg buff sz:%d", ev.fd, ev.msg->buff_sz);
        _distpach_remote_service_smsg(impl, ev.msg->buff, ev.msg->buff_sz);
        break;
    default:
        return -1;
    }
    return 0;
}
int RpcClient::init(const std::string & svraddrs, int queue_size, dctcp_t * stcp){
    if (impl){
        return -1;
    }
    impl = new RpcClientImpl;
    if (!stcp){
        dctcp_config_t dctcp;
        dctcp.max_tcp_send_buff_size = 1024*1024*10;
        dctcp.max_tcp_recv_buff_size = 1024*1024*10;
        impl->stcp = dctcp_create(dctcp);
        if (!impl->stcp){
            return -2;
        }
        impl->stcp_owner = true;
    }
    else {
        impl->stcp = stcp;
    }
    impl->queue_max_size = queue_size;
    dcs::strsplit(svraddrs, ",", impl->rpc_server_addrs);
    return dctcp_connect(impl->stcp, impl->select_server(), impl->connect_server_retry, "msg:sz32",
        _rpc_client_stcp_dispatch, impl);
}
static inline int _send_directly(RpcClientImpl * impl, const dcrpc_msg_t & tosend_msg){
	if (!tosend_msg.Pack(impl->send_msg_buff)){
		GLOG_ERR("pack msg error when send msg to svc:%s", tosend_msg.path().c_str());
		return -1;
	}
	int ret = dctcp_send(impl->stcp, impl->cnnxfd, dctcp_msg_t(impl->send_msg_buff.buffer, impl->send_msg_buff.valid_size));
    GLOG_TRA("send [%d] [%p:%d] [%s]", ret, impl->send_msg_buff.buffer, impl->send_msg_buff.valid_size,
                tosend_msg.Debug());
	if (ret){
		GLOG_SER("tcp send error = %d when send msg to svc:%s buff len:%d",
			ret, tosend_msg.path().c_str(), impl->send_msg_buff.valid_size);
		return -2;
	}
	return 0;
}
#define  CHECK_CNNX_TIME_INTERVAL   (3)
static inline void _check_connections(RpcClientImpl * impl){
    if (!impl->stcp || impl->cnnxfd != -2){
        return;
    }
    else {
        uint32_t t_times_now = dcs::time_unixtime_s();
        if (t_times_now > impl->next_check_connx_time){
            impl->next_check_connx_time = t_times_now + CHECK_CNNX_TIME_INTERVAL;
            GLOG_ERR("rpc connection lost , reconnecting ...");
            impl->cnnxfd = -1;
            dctcp_connect(impl->stcp, impl->select_server(), impl->connect_server_retry, "msg:sz32",
                _rpc_client_stcp_dispatch, impl);
        }
    }
}
static inline int _check_sending_queue(RpcClientImpl * impl){
	int ret = 0;
	while (impl->cnnxfd >= 0 && !impl->sending_queue.empty()){
		const dcrpc_msg_t & tosend_msg = impl->sending_queue.front();
		ret = _send_directly(impl, tosend_msg);
		if (ret == 0 || ret == -1){ //directly or pack error
			impl->sending_queue.pop();
		}
		else {
			return ret;
		}
	}
	return ret;
}
int RpcClient::update(int tickus){
    if (!impl){
        return 0;
    }
    _check_connections(impl);
	_check_sending_queue(impl);
    int nproc = 0;
    if (impl->stcp_owner){
        nproc = dctcp_poll(impl->stcp, tickus, 100);
    }
	_check_sending_queue(impl);
	return nproc;
}
int RpcClient::destroy(){
    if (impl){
        if (impl->stcp_owner && impl->stcp){
            dctcp_destroy(impl->stcp);
        }
        impl->stcp = nullptr;
        delete impl;
        impl = nullptr;
    }
	return 0;
}
bool RpcClient::ready() const {
    return impl->cnnxfd >= 0;
}

int RpcClient::notify(const string & svc, RpcCallNotify cb){
    impl->notify_cbs[svc]=cb;
    return 0;
}
static inline int _rpc_send_msg(RpcClientImpl * impl, const dcrpc_msg_t & rpc_msg){
    if (impl->cnnxfd < 0 && (int)impl->sending_queue.size() >= impl->queue_max_size){
        GLOG_ERR("connection fd:%d qsize:%d not ready error send msg to svc:%s", impl->cnnxfd, (int)impl->sending_queue.size(), rpc_msg.path().c_str());
        return -1;
    }
	_check_sending_queue(impl); //checking queue
	if (!impl->sending_queue.empty() || impl->cnnxfd < 0){ //not send all
        impl->sending_queue.push(rpc_msg);
    }
	else { //send all
		int ret = _send_directly(impl, rpc_msg);
		if (ret && ret != -1){ //ret == -2 , send error
			impl->sending_queue.push(rpc_msg);
		}
    }    
	return 0;
}
int RpcClient::push(const std::string & svc, const dcrpc::RpcValues & args){
    dcrpc_msg_t rpc_msg;
    rpc_msg.set_path(svc);
    auto margs = (decltype(rpc_msg.mutable_request()->mutable_args()))const_cast<RpcValues&>(args).data();
    rpc_msg.mutable_request()->mutable_args()->CopyFrom(*margs);
    return _rpc_send_msg(impl, rpc_msg);
}
int RpcClient::call(const string & svc, const RpcValues & args, RpcCallNotify result_cb){
    dcrpc_msg_t rpc_msg;
    RpcMsg::Cookie & ck = *rpc_msg.mutable_cookie();
    rpc_msg.set_path(svc);
    auto margs = (decltype(rpc_msg.mutable_request()->mutable_args()))const_cast<RpcValues&>(args).data();
    rpc_msg.mutable_request()->mutable_args()->CopyFrom(*margs);
    ck.set_transaction(impl->append_callback(result_cb));
    return _rpc_send_msg(impl, rpc_msg);
}
//int call(const std::string & svc, const char * buff, int ibuff, callback_result_t result_cb);
//int call(const std::string & svc, const ::google::protobuf::Message & msg, callback_result_t result_cb);
NS_END()