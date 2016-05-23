#include "hiredis/hiredis.h"
#include "hiredis/async.h"
#include "hiredis/adapters/libev.h"

#include "../../base/logger.h"
#include "../../base/dcutils.hpp"
#include "../../base/dcseqnum.hpp"
#include "dcredis.h"

NS_BEGIN(dcsutil)
typedef dcsutil::sequence_number_t<>	redis_evsn;

struct command_callback_argument {
	RedisAsyncAgentImpl* impl{ nullptr };
	uint64_t			 sn{ 0 };
    int32_t              opt{ 0 };
};
enum CommandOptions {
    COMMAND_FLAG_NO_EXPIRED = 1,
};
struct RedisCallBackPoolItem {
	RedisAsyncAgent::CommandCallBack cb{ 0 };
	uint32_t                         timestamp{ 0 };
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
    std::vector<redisAsyncContext *> subscribers;
};

static std::unordered_map<const redisAsyncContext *, RedisAsyncAgentImpl*>  s_redis_context_map_impl;
static void disconnectCallback(const redisAsyncContext *c, int status);
static void connectCallback(const redisAsyncContext *c, int status) {
	if (status != REDIS_OK) {
		GLOG_ERR("redis connect Error: %s", c->errstr);
	}
	else {
		GLOG_IFO("redis Connected...");
        s_redis_context_map_impl[c]->connected = true;
	}
}
static void default_connectCallback(const redisAsyncContext *c, int status) {
    GLOG_IFO("redis connection connected c:%p", c);
    if (status != REDIS_OK) {
        GLOG_ERR("redis connect Error: %s", c->errstr);
    }
    else {
        GLOG_IFO("redis Connected...");
    }
}
static void default_disconnectCallback(const redisAsyncContext *c, int status) {
    GLOG_ERR("redis disconnected c:%p", c);
    if (status != REDIS_OK) {
        GLOG_ERR("redis diconnect Error: %s", c->errstr);
    }
    else {
        GLOG_WAR("redis Disconnected...");
    }
}
static redisAsyncContext * redis_create_context(RedisAsyncAgentImpl * impl, redisConnectCallback onconn = nullptr, redisDisconnectCallback ondisconn = nullptr){
#warning "todo change the ip by config param impl"
    redisAsyncContext * ctx = redisAsyncConnect("127.0.0.1", 6379);
    if (!ctx){
        GLOG_SER("redis connect error !");
        return nullptr;
    }
    if (ctx->err){
        GLOG_ERR("redis connect error :%s", ctx->errstr);
        redisAsyncFree(ctx);
        return nullptr;
    }
    redisLibevAttach(EV_DEFAULT_ ctx);
    if (onconn){
        redisAsyncSetConnectCallback(ctx, onconn);
    }
    else {
        redisAsyncSetConnectCallback(ctx, default_connectCallback);
    }
    if (ondisconn){
        redisAsyncSetDisconnectCallback(ctx, ondisconn);
    }
    else {
        redisAsyncSetDisconnectCallback(ctx, default_disconnectCallback);
    }
    return ctx;
}
static inline int redis_reconnect(RedisAsyncAgentImpl* impl){
    if (impl->rc){
        redisAsyncFree(impl->rc);
        s_redis_context_map_impl.erase(impl->rc);
        impl->rc = nullptr;
    }
    impl->rc = redis_create_context(impl, connectCallback, disconnectCallback);
    if (!impl->rc){
        return -1;
    }
    ///////////////////////////////////////////////////////////////
    s_redis_context_map_impl[impl->rc] = impl;
    return 0;
}
static void disconnectCallback(const redisAsyncContext *c, int status) {
    RedisAsyncAgentImpl * impl = s_redis_context_map_impl[c];
    impl->connected = false;
    if (status != REDIS_OK) {
        GLOG_ERR("redis diconnect Error: %s", c->errstr);
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
        if (!(nowit->second.param->opt & COMMAND_FLAG_NO_EXPIRED) &&
            nowit->second.timestamp + impl->time_max_expired < now){
			nowit->second.cb(-1, nullptr);
            GLOG_DBG("update check expired callback free :%p", nowit->second.param);
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
            for (int i = 0; i < (int)impl->subscribers.size(); ++i){
                redisAsyncFree(impl->subscribers[i]);
            }
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
static void _command_callback(redisAsyncContext * c, void *r, void *privdata){
    command_callback_argument* param = (command_callback_argument*)privdata;
    uint64_t sn = param->sn;
    RedisAsyncAgentImpl * impl = param->impl;
    bool erase_callback = false;
    if (!(param->opt & COMMAND_FLAG_NO_EXPIRED)){
        GLOG_DBG("callback free redis param:%p", privdata);
        delete param;
        erase_callback = true;
    }
    auto cpit = impl->callback_pool.find(sn);
    if (cpit == impl->callback_pool.end()){
        GLOG_ERR("redis callback not found by sn:%lu param c:%p reply:%p impl:%p(%s)", 
            sn, c, r, impl, impl->addrs.c_str());
        return;
    }
	auto cb = cpit->second.cb; 
    if (!c || !r){
        GLOG_ERR("redis callback error param -> privdata:%p c:%p reply:%p impl:%p(%s)",
            privdata, c, r, impl, impl->addrs.c_str());
        return;
    }
	cb(0, (redisReply*)r);
    if (erase_callback){
        impl->callback_pool.erase(sn);
    }
}
static inline int _command(RedisAsyncAgentImpl * impl, redisAsyncContext* ctx, int opt,
            RedisAsyncAgent::CommandCallBack cb, const char * fmt, va_list ap){
    uint64_t sn = redis_evsn::next();
    auto param = new command_callback_argument;
    param->impl = impl;
    param->sn = sn;
    param->opt = opt;
    GLOG_DBG("redis command create param :%p", param);
    int r = redisvAsyncCommand(ctx, _command_callback, param, fmt, ap);
    if (r != REDIS_OK){
        GLOG_ERR("redis command execute error = %d free :%p ", r, param);
        delete param;
        return -1;
    }
    impl->callback_pool[sn].cb = cb;
    impl->callback_pool[sn].timestamp = dcsutil::time_unixtime_s();
    impl->callback_pool[sn].param = param;
    return 0;
}
static inline int _subscribe(RedisAsyncAgentImpl * impl, RedisAsyncAgent::CommandCallBack cb, const string & command){
    redisAsyncContext * ctx = redis_create_context(impl);
    if (!ctx){
        GLOG_ERR("create redis async context error !");
        return -1;
    }
    impl->subscribers.push_back(ctx);
    va_list ap;
    int r = _command(impl, ctx, COMMAND_FLAG_NO_EXPIRED, cb, command.c_str(), ap);
    return r;
}
int RedisAsyncAgent::subscribe(CommandCallBack cb, const char * channel){
    string command = "SUBSCRIBE ";
    command += channel;
    return _subscribe(impl, cb, command);
}
int RedisAsyncAgent::psubscribe(CommandCallBack cb, const char * channel){
    string command = "PSUBSCRIBE ";
    command += channel;
    return _subscribe(impl, cb, command);
}
//typedef std::function<void(int error, const char * buff, int ibuff)> CommandCallBack;
int RedisAsyncAgent::command(CommandCallBack cb, const char * fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    int r = _command(impl, impl->rc, 0, cb, fmt, ap);
	va_end(ap);
	return r;
}
RedisAsyncAgent::RedisAsyncAgent(){
}
RedisAsyncAgent::~RedisAsyncAgent(){
	destroy();
}

NS_END()
