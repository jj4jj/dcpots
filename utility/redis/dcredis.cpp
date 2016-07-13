#include "hiredis/hiredis.h"
#include "hiredis/async.h"
#include "hiredis/adapters/libev.h"

#include "../../base/logger.h"
#include "../../base/dcutils.hpp"
#include "../../base/dcseqnum.hpp"
#include "dcredis.h"

NS_BEGIN(dcsutil)
typedef dcsutil::sequence_number_t<>	redis_evsn;

enum CommandOptions {
    COMMAND_FLAG_NO_EXPIRED = 1,
};
struct RedisCallBackPoolItem {
	RedisAsyncAgent::CommandCallBack cb{ 0 };
	uint32_t                         timestamp{ 0 };
    int32_t                          opt{ 0 };
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
    bool                 stop{ false };
    uint32_t             last_reconnect_time{ 0 };
};


static std::unordered_map<const redisAsyncContext *, RedisAsyncAgentImpl*>  s_redis_context_map_impl;
static void disconnectCallback(const redisAsyncContext *c, int status);
static void connectCallback(const redisAsyncContext *c, int status);
static redisAsyncContext * _create_redis_context(RedisAsyncAgentImpl * impl, redisConnectCallback onconn = nullptr, redisDisconnectCallback ondisconn = nullptr);
static inline void _clear_redis_context(RedisAsyncAgentImpl* impl) {
    GLOG_WAR("clear redis context impl:%p main ctx:%p", impl, impl->rc);
    if (impl->rc) {
        s_redis_context_map_impl.erase(impl->rc);
        redisAsyncContext * trc = impl->rc;
        impl->rc = nullptr;
        redisAsyncFree(trc);
    }
}
static inline int _connect_redis(RedisAsyncAgentImpl* impl) {
    assert("redis reconnect must cleared !" && impl->rc == nullptr);
    ++impl->retry_connect;
    GLOG_IFO("redis reconnecting times:%d ...", impl->retry_connect);
    impl->rc = _create_redis_context(impl, connectCallback, disconnectCallback);
    if (!impl->rc) {
        GLOG_SER("redis reconnecting error retry times:%d ...", impl->retry_connect);
        return -1;
    }
    ///////////////////////////////////////////////////////////////
    s_redis_context_map_impl[impl->rc] = impl;
    return 0;
}
static void connectCallback(const redisAsyncContext *c, int status) {
    auto fit = s_redis_context_map_impl.find(c);
    assert("redis connect callback must has a context" && fit != s_redis_context_map_impl.end());
	if (status != REDIS_OK) {
		GLOG_ERR("redis (ctx:%p -> impl:%p)connect Error: %s",c, fit->second, c->errstr);
        fit->second->connected = false;
        _clear_redis_context(fit->second);
    }
	else {
		GLOG_IFO("redis Connected...");
        fit->second->connected = true;
	}
}
static void default_connectCallback(const redisAsyncContext *c, int status) {
    GLOG_IFO("redis default_connectCallback connect c:%p status:%d", c, status);
    if (status != REDIS_OK) {
        GLOG_ERR("redis connect Error: %s", c->errstr);
    }
    else {
        GLOG_IFO("redis Connected...");
    }
}
static void default_disconnectCallback(const redisAsyncContext *c, int status) {
    GLOG_IFO("redis default_disconnectCallback c:%p status:%d", c, status);
    if (status != REDIS_OK) {
        GLOG_ERR("redis diconnect Error: %s", c->errstr);
    }
    else {
        GLOG_WAR("redis Disconnected...");
    }
}
static redisAsyncContext * _create_redis_context(RedisAsyncAgentImpl * impl, redisConnectCallback onconn, redisDisconnectCallback ondisconn){
    std::vector<std::string> vs;
    dcsutil::strsplit(impl->addrs, ":", vs);
    if (vs.size() != 2){
        GLOG_ERR("error redis address :%s", impl->addrs.c_str());
        return nullptr;
    }
    redisAsyncContext * ctx = redisAsyncConnect(vs[0].c_str(), std::stoi(vs[1]));
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
static void disconnectCallback(const redisAsyncContext *c, int status) {
    auto fit = s_redis_context_map_impl.find(c);
    if (fit == s_redis_context_map_impl.end()) {
        GLOG_ERR("redis connection disconnect status:%d not found context:%p !", status, c);
        return;
    }
    RedisAsyncAgentImpl * impl = fit->second;
    impl->connected = false;
    if (status != REDIS_OK) {
        GLOG_ERR("redis diconnect Error: %s", c->errstr);
	}
	else {
		GLOG_WAR("redis Disconnected...");
	}
    if (impl->stop) {
        return;
    }
    _clear_redis_context(impl);
}
int	RedisAsyncAgent::init(const string & addrs){
	if (impl){
		return -1;
	}
	impl = new RedisAsyncAgentImpl();
    impl->addrs = addrs;
    impl->stop = false;
    return _connect_redis(impl);
}
static inline void redis_check_time_expired_callback(RedisAsyncAgentImpl * impl){
	auto it = impl->callback_pool.begin();
	uint32_t now = dcsutil::time_unixtime_s();
	while (it != impl->callback_pool.end()){
		auto nowit = it++;
        if (!(nowit->second.opt & COMMAND_FLAG_NO_EXPIRED) &&
            nowit->second.timestamp + impl->time_max_expired < now){
			nowit->second.cb(-1, nullptr);
            GLOG_DBG("update check expired callback event sn :%lu", nowit->first);
			impl->callback_pool.erase(nowit);
		}
	}
}
int RedisAsyncAgent::update(){
    uint32_t time_now = dcsutil::time_unixtime_s();
    if (impl->rc == nullptr && 
        (time_now > impl->last_reconnect_time + 10)) {
        impl->last_reconnect_time = time_now;
        _connect_redis(impl);
    }
    if (impl->rc) {
        ev_loop(EV_DEFAULT_ EVRUN_NOWAIT);
    }
	redis_check_time_expired_callback(impl);
	return 0;
}
int	RedisAsyncAgent::destroy(){
	if (impl){
        impl->stop = true;
        _clear_redis_context(impl);
        for (int i = 0; i < (int)impl->subscribers.size(); ++i){
            redisAsyncFree(impl->subscribers[i]);
        }
        impl->subscribers.clear();

		delete impl;
		impl = nullptr;
	}
	return 0;
}
bool RedisAsyncAgent::ready(){
    return impl->connected;
}
static void _command_callback(redisAsyncContext * c, void *r, void *privdata){
    uint64_t sn = (uint64_t)privdata;
    auto iit =  s_redis_context_map_impl.find(c);
    if (iit == s_redis_context_map_impl.end()) {
        GLOG_FTL("redis context impl not found by ctx:%p - event sn:%lu", c, sn);
        return;
    }
    RedisAsyncAgentImpl * impl = iit->second;
    auto cpit = impl->callback_pool.find(sn);
    if (cpit == impl->callback_pool.end()) {
        GLOG_ERR("redis callback not found by event sn:%lu c:%p reply:%p impl:%p(%s)",
                 sn, c, r, impl, impl->addrs.c_str());
        return;
    }

    if (!c || !r){
        GLOG_ERR("redis callback error param event sn:%lu (c:%p, reply:%p) impl:%p(%s)",
            sn, c, r, impl, impl->addrs.c_str());
        return;
    }
    cpit->second.cb(0, (redisReply*)r);
    if (!(cpit->second.opt & COMMAND_FLAG_NO_EXPIRED)) {
        GLOG_DBG("callback free redis event sn:%lu", sn);
        impl->callback_pool.erase(cpit);
    }
}
static inline int _command(RedisAsyncAgentImpl * impl, redisAsyncContext* ctx, int opt,
            RedisAsyncAgent::CommandCallBack cb, const char * fmt, va_list ap){
    uint64_t sn = redis_evsn::next();
    GLOG_DBG("redis command create event ctx:%p sn :%lu", ctx, sn);
    int r = redisvAsyncCommand(ctx, _command_callback, (void*)sn, fmt, ap);
    if (r != REDIS_OK){
        GLOG_ERR("redis command execute error = %d event sn :%lu command:%s", r, sn, fmt);
        return -1;
    }

    impl->callback_pool[sn].cb = cb;
    impl->callback_pool[sn].timestamp = dcsutil::time_unixtime_s();
    impl->callback_pool[sn].opt = opt;

    return 0;
}
static inline int _subscribe(RedisAsyncAgentImpl * impl, RedisAsyncAgent::CommandCallBack cb, const string & command){
    redisAsyncContext * ctx = _create_redis_context(impl);
    if (!ctx){
        GLOG_ERR("create redis async context error !");
        return -1;
    }
    impl->subscribers.push_back(ctx);
    s_redis_context_map_impl[ctx] = impl;
    GLOG_IFO("redis subscribe context impl:%p ctx:%p count:%zd command:%s", impl, ctx, impl->subscribers.size(), command.c_str());
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
    if (!impl->connected) {
        GLOG_ERR("redis not connected when excute command:%s", fmt);
        return -1;
    }
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
