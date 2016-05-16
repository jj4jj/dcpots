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
typedef dcsutil::sequence_number_t<2, 30>    rpcall_transaction_sn;
typedef msgproto_t<RpcMsg>  dcrpc_msg_t;

////////////////////////////////////////////////////////////////////
struct  RpcClientImpl {
    dctcp_t * cli{ nullptr };
    int fd{ -1 };
    std::unordered_map<uint64_t, RpcClient::RpcCallNotify>  rpcall_cbs;
    std::unordered_map<string, RpcClient::RpcCallNotify>    notify_cbs;
    //todo timer queue
    msg_buffer_t send_msg_buff;
    std::vector<std::string> rpc_server_addrs;
    int  connect_server_retry{ 3 };
    std::queue<dcrpc_msg_t>  sending_queue;
    int  queue_max_size{ 0 };
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
        uint64_t sn = rpcall_transaction_sn::next();
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
        GLOG_ERR("unpack rpc msg error ! buff:%64s len:%d", buff, ibuff);
        return;
    }
    GLOG_TRA("recv [%s]", rpc_msg.Debug());
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
int RpcClient::init(const std::string & svraddrs, int queue_size){
    if (impl){
        return -1;
    }
    impl = new RpcClientImpl;
    dctcp_config_t dctcp;
    impl->cli = dctcp_create(dctcp);
    if (!impl->cli){
        return -2;
    }
    impl->queue_max_size = queue_size;
    dctcp_event_cb(impl->cli, [](dctcp_t* dc, const dctcp_event_t & ev, void * ud)->int {
        RpcClientImpl * impl = (RpcClientImpl *)ud;
        switch (ev.type){
        case DCTCP_CONNECTED:
            impl->fd = ev.fd;
            break;
        case DCTCP_CLOSED:
            impl->fd = -1;
            GLOG_TRA("connection closed [reason:%d] ! reconnecting ...", ev.reason);
            dctcp_connect(dc, impl->select_server(), impl->connect_server_retry);
            break;
        case DCTCP_READ:
            _distpach_remote_service_smsg(impl, ev.msg->buff, ev.msg->buff_sz);
            break;
        default:
            return -1;
        }
		return 0;
    }, impl);
    dcsutil::strsplit(svraddrs, ",", impl->rpc_server_addrs);
    return dctcp_connect(impl->cli, impl->select_server(), impl->connect_server_retry);
}
static inline int _send_directly(RpcClientImpl * impl, const dcrpc_msg_t & tosend_msg){
	if (!tosend_msg.Pack(impl->send_msg_buff)){
		GLOG_ERR("pack msg error when send msg to svc:%s", tosend_msg.path().c_str());
		return -1;
	}
	int ret = dctcp_send(impl->cli, impl->fd, dctcp_msg_t(impl->send_msg_buff.buffer, impl->send_msg_buff.valid_size));
	GLOG_TRA("send [%d] [%s] [%s]", ret, tosend_msg.path().c_str(), tosend_msg.Debug());
	if (ret){
		GLOG_SER("tcp send error = %d when send msg to svc:%s buff len:%d",
			ret, tosend_msg.path().c_str(), impl->send_msg_buff.valid_size);
		return -2;
	}
	return 0;
}
static inline int _check_sending_queue(RpcClientImpl * impl){
	int ret = 0;
	while (impl->fd != -1 && !impl->sending_queue.empty()){
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
int RpcClient::update(){
	_check_sending_queue(impl);
	int nproc = dctcp_poll(impl->cli, 1000);
	_check_sending_queue(impl);
	return nproc;
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
	return 0;
}
bool RpcClient::ready() const {
    return impl->fd != -1;
}

int RpcClient::notify(const string & svc, RpcCallNotify cb){
    impl->notify_cbs[svc]=cb;
    return 0;
}
static inline int _send_msg(RpcClientImpl * impl, const dcrpc_msg_t & rpc_msg){
    if (impl->fd == -1 && (int)impl->sending_queue.size() >= impl->queue_max_size){
        GLOG_ERR("connection not ready error send msg to svc:%s", rpc_msg.path().c_str());
        return -1;
    }
	_check_sending_queue(impl); //checking queue
	if (!impl->sending_queue.empty() || impl->fd == -1){ //not send all
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
    return _send_msg(impl, rpc_msg);
}
int RpcClient::call(const string & svc, const RpcValues & args, RpcCallNotify result_cb){
    dcrpc_msg_t rpc_msg;
    RpcMsg::Cookie & ck = *rpc_msg.mutable_cookie();
    rpc_msg.set_path(svc);
    auto margs = (decltype(rpc_msg.mutable_request()->mutable_args()))const_cast<RpcValues&>(args).data();
    rpc_msg.mutable_request()->mutable_args()->CopyFrom(*margs);
    ck.set_transaction(impl->append_callback(result_cb));
    return _send_msg(impl, rpc_msg);
}
//int call(const std::string & svc, const char * buff, int ibuff, callback_result_t result_cb);
//int call(const std::string & svc, const ::google::protobuf::Message & msg, callback_result_t result_cb);


NS_END()