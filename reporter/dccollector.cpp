#include "dcreport_collect.h"
#include "dccollector.h"
#include "../base/logger.h"
#include "../base/dcutils.hpp"
#include "../dcagent/dcagent.h"

struct {
    collector_event_cb_t        cb;
    void                        *cb_ud;
} CTX = { nullptr, nullptr };


int on_report_set(const msg_buffer_t &  msg, const char * src){
    GLOG_TRA("recv from :%s set msg:%s", src, msg.buffer);
    if (!CTX.cb){
        return 0;
    }
    char * sep = strchr(msg.buffer, ':');
    if (!sep){
        return -1;
    }
    *sep = 0;
    struct collector_event_data evd;
    evd.key = msg.buffer;
    evd.change = 0;
    evd.param = sep + 1;
    return CTX.cb(CTX.cb_ud, src, REPORT_MSG_SET, &evd);
}
int	on_report_inc(const msg_buffer_t &  msg, const char * src){
    GLOG_TRA("recv from :%s inc msg:%s", src, msg.buffer);
    if (!CTX.cb){
        return 0;
    }
    char * sep = strchr(msg.buffer, ':');
    if (!sep){
        return -1;
    }
    *sep = 0;
    struct collector_event_data evd;
    evd.key = msg.buffer;
    evd.change = strtol(sep + 1, (char**)&evd.param, 10);
    if (*evd.param == ':'){
        evd.param++;
    }
    else {
        evd.param = nullptr;
    }
    return CTX.cb(CTX.cb_ud, src, REPORT_MSG_INC, &evd);
}
int on_report_dec(const msg_buffer_t &  msg, const char * src){
    GLOG_TRA("recv from :%s dec msg:%s", src, msg.buffer);
    if (!CTX.cb){
        return 0;
    }
    char * sep = strchr(msg.buffer, ':');
    if (!sep){
        return -1;
    }
    *sep = 0;
    struct collector_event_data evd;
    evd.key = msg.buffer;
    evd.change = strtol(sep + 1, (char**)&evd.param, 10);
    if (*evd.param == ':'){
        evd.param++;
    }
    else {
        evd.param = nullptr;
    }
    return CTX.cb(CTX.cb_ud, src, REPORT_MSG_DEC, &evd);
}

int collector_init(const char * addr){
    dagent_config_t conf;
    conf.addr = "pull:";
    conf.addr += addr;
    conf.name = "dccollector";
    if (dagent_init(conf)){
        GLOG_TRA("dagent init error !");
        return -1;
    }
    dagent_cb_push(REPORT_MSG_SET, on_report_set);
    dagent_cb_push(REPORT_MSG_INC, on_report_set);
    dagent_cb_push(REPORT_MSG_DEC, on_report_set);
    return 0;
}
void collector_destroy(){
    dagent_destroy();
}

void collector_update(int timeout_ms){
    dagent_update(timeout_ms);
}
