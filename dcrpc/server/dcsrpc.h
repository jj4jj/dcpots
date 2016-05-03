#pragma once
#include <string>
#include <functional>
#include "../share/dcrpc.h"
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
	int	   reply(RpcService * svc, uint64_t cookie, const RpcValues & result, int ret = 0, const char * error = nullptr);

private: 
    RpcServerImpl  * impl{ nullptr };
};

class RpcServiceImpl;
class RpcService {
public:
    RpcService(const std::string  & name_, bool async_call = false);
	virtual		~RpcService();
	const		std::string & name() const ;
	bool		isasync() const ;
	const RpcServiceImpl * impl() const { return impl_; }
public:
	int			 resume(uint64_t cookie, const RpcValues & result, int ret = 0, const char * error = nullptr);
public:
    virtual int  yield(uint64_t cookie, const RpcValues & args, std::string & error, int clientid);
    virtual int  call(RpcValues & result, const RpcValues & args, std::string & error, int clientid);
public:
    virtual void clientclosed(int clientid);
    virtual void update();
private:
	RpcServiceImpl * impl_{ nullptr };
};

}
