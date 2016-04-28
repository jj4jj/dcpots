#pragma once
#include <string>
#include <functional>
namespace dcrpc {
struct RpcValues;
class RpcService;
struct RpcServerImpl;
class RpcServer {
public:
    RpcServer();
    ~RpcServer();
public:
    int    init(const std::string & addr);
    int    update();
    void   destroy();
public:
    int    regis(RpcService * svc);
    int    push(const std::string & svc, int id, const RpcValues & val);
private: 
    RpcServerImpl  * impl{ nullptr };
};

class RpcService {
public:
    RpcService(const std::string  & name_);
    virtual ~RpcService(){}
    const std::string & name(){return this->name_;}
public:
    virtual int call(RpcValues & result, const RpcValues & args, std::string * error);
private:
    std::string name_;
};

}
