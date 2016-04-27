#pragma once

namespace dcsutil {
struct RpcClientImpl;
class RpcClient {
public:
    typedef std::function<void()> NotifyCallBack;
    RpcClient();
    ~RpcClient();
public:
    int init();
    int update();
    int destroy();
    int notify(NotifyCallBack cb);
public:
    int call(const string & svc);
private:
    RpcClientImpl * impl { nullptr };
};

}
