#pragma once
#include <string>
namespace dcsutil {

class RpcService;
struct RpcServerImpl;
class RpcServer {
public:
    RpcServer();
    ~RpcServer();
public:
    int    init();
    int    update();
    void   destroy();
public:
    int    regis(RpcService * svc);
private: 
    RpcServerImpl  * impl{ nullptr };
};

class RpcService {
public:
    RpcService(const std::string  & name_);
    virtual ~RpcService(){}
    const std::string & name(){
        return this->name_;
    }
public:
    virtual int call()=0;
private:
    std::string name_;

};

}
