#pragma once
#include "../share/dcrpc.h"
namespace dcrpc {
struct RpcValues;
struct RpcClientImpl;
class RpcClient {
public:
    RpcClient();
    ~RpcClient();
public:
    int init(const std::string & svraddrs);
    int update();
    int destroy();
    typedef std::function<void(int ret, const dcrpc::RpcValues &)>   RpcCallNotify;
    int notify(const string & svc, RpcCallNotify result_cb);//register callback
    bool ready() const;
public:
    int call(const std::string & svc, const dcrpc::RpcValues & args, RpcCallNotify result_cb);
    int push(const std::string & svc, const dcrpc::RpcValues & args);
    //int call(const std::string & svc, const char * buff, int ibuff, callback_result_t result_cb);
    //int call(const std::string & svc, const ::google::protobuf::Message & msg, callback_result_t result_cb);
private:
    RpcClientImpl * impl { nullptr };
};

}
