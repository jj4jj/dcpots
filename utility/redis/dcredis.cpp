#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>

#include "../../base/logger.h"
#include "../../base/dcutils.hpp"
#include "../../base/dcseqnum.hpp"
#include "dcredis.h"

NS_BEGIN(dcsutil)
typedef dcsutil::sequence_number_t<>	redis_evsn;

struct command_callback_argument {
	RedisAsyncAgentImpl* impl{ nullptr };
	uint64_t			 sn{ 0 };
};
struct RedisCallBackPoolItem {
	RedisAsyncAgent::CommandCallBack cb{ 0 };
	uint32_t timestamp{ 0 };
	command_callback_argument * param{ nullptr };
};
struct RedisAsyncAgentImpl {
	redisAsyncContext * rc{ nullptr };
	typedef std::unordered_map<uint64_t, RedisCallBackPoolItem> RedisCallBackPool;
	RedisCallBackPool	 callback_pool;
	int		             time_max_expired{ 10 };
    string               addrs;
    bool                 connected{ false };
    int                  retry_connect{ 0 };
};

static std::unordered_map<const redisAsyncContext *, RedisAsyncAgentImpl*>  s_redis_context_map_impl;
static void connectCallback(const redisAsyncContext *c, int status) {
	if (status != REDIS_OK) {
		GLOG_ERR("redis connect Error: %s", c->errstr);
	}
	else {
		GLOG_IFO("redis Connected...");
        s_redis_context_map_impl[c]->connected = true;
	}
}
static void disconnectCallback(const redisAsyncContext *c, int status);
static inline int redis_reconnect(RedisAsyncAgentImpl* impl){
    if (impl->rc){
        redisAsyncFree(impl->rc);
        s_redis_context_map_impl.erase(impl->rc);
        impl->rc = nullptr;
    }
#warning "todo change the ip by config param"
    impl->rc = redisAsyncConnect("127.0.0.1", 6379);
    if (!impl->rc){
        GLOG_SER("redis connect error !");
        return -2;
    }
    if (impl->rc->err){
        GLOG_ERR("redis connect error :%s", impl->rc->errstr);
        return -3;
    }
    redisLibevAttach(EV_DEFAULT_ impl->rc);
    redisAsyncSetConnectCallback(impl->rc, connectCallback);
    redisAsyncSetDisconnectCallback(impl->rc, disconnectCallback);
    ///////////////////////////////////////////////////////////////
    s_redis_context_map_impl[impl->rc] = impl;
    return 0;
}
static void disconnectCallback(const redisAsyncContext *c, int status) {
    RedisAsyncAgentImpl * impl = s_redis_context_map_impl[c];
    impl->connected = false;
    if (status != REDIS_OK) {
		printf("redis diconnect Error: %s", c->errstr);
	}
	else {
		GLOG_WAR("redis Disconnected...");
	}
    ++impl->retry_connect;
    GLOG_IFO("redis reconnecting times:%d....", impl->retry_connect);
    redis_reconnect(impl);
}
int	RedisAsyncAgent::init(const string & addrs){
	if (impl){
		return -1;
	}
	impl = new RedisAsyncAgentImpl();
    impl->addrs = addrs;
    return redis_reconnect(impl);
}
static inline void redis_check_time_expired_callback(RedisAsyncAgentImpl * impl){
	auto it = impl->callback_pool.begin();
	uint32_t now = dcsutil::time_unixtime_s();
	while (it != impl->callback_pool.end()){
		auto nowit = it++;
		if (nowit->second.timestamp + impl->time_max_expired < now){
			nowit->second.cb(-1, nullptr);
			delete nowit->second.param;
			nowit->second.param = nullptr;
			impl->callback_pool.erase(nowit);
		}
	}
}
int RedisAsyncAgent::update(){
	ev_loop(EV_DEFAULT_ EVRUN_NOWAIT);
	redis_check_time_expired_callback(impl);
	return 0;
}
int	RedisAsyncAgent::destroy(){
	if (impl){
		if (impl->rc){
			redisAsyncFree(impl->rc);
			impl->rc = nullptr;
		}
		delete impl;
		impl = nullptr;
	}
	return 0;
}
bool RedisAsyncAgent::ready(){
    return impl->connected;
}
static void _command_callback(redisAsyncContext *, void *r, void *privdata){
	command_callback_argument* param = (command_callback_argument*)privdata;
	uint64_t sn = param->sn;
	RedisAsyncAgentImpl * impl = param->impl;
	delete param;
	auto cb = impl->callback_pool[sn].cb; 
	cb(0, (redisReply*)r);
	impl->callback_pool.erase(sn);
}
//typedef std::function<void(int error, const char * buff, int ibuff)> CommandCallBack;
int RedisAsyncAgent::command(CommandCallBack cb, const char * fmt, ...){
	uint64_t sn = redis_evsn::next();
	auto param = new command_callback_argument;
	param->impl = impl;
	param->sn = sn;
	va_list ap;
	va_start(ap, fmt);
	int r = redisvAsyncCommand(impl->rc, _command_callback,
		param, fmt, ap);
	va_end(ap);
	if (r != REDIS_OK){
		GLOG_ERR("redis command execute error = %d !", r);
		return -1;
	}
	impl->callback_pool[sn].cb = cb;
	impl->callback_pool[sn].timestamp = dcsutil::time_unixtime_s();;
	impl->callback_pool[sn].param = param;
	return 0;
}
RedisAsyncAgent::RedisAsyncAgent(){
}
RedisAsyncAgent::~RedisAsyncAgent(){
	destroy();
}

NS_END()