#pragma once
#include "../share/dcrpc.h"
struct dctcp_t;
namespace dcrpc {
struct RpcValues;
struct RpcClientImpl;
class RpcClient {
public:
    RpcClient();
    ~RpcClient();
public:
    int     init(const std::string & svraddrs, int queue_size = 0, dctcp_t * stcp = nullptr);
    int     update(int tickus = 1000*10);
    int     destroy();
    typedef std::function<void(int ret, const dcrpc::RpcValues &)>   RpcCallNotify;
    int     notify(const string & svc, RpcCallNotify result_cb);//register callback
    bool    ready() const;
public:
    int     call(const std::string & svc, const dcrpc::RpcValues & args, RpcCallNotify result_cb);
    int     push(const std::string & svc, const dcrpc::RpcValues & args);
private:
    RpcClientImpl * impl { nullptr };
};

}
