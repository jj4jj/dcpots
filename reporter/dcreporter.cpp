#include "dcreporter.h"
#include "dcreport_collect.h"
#include "../dagent/dagent.h"
#include "../base/logger.h"

int reporter_ready(){
    return dagent_ready();
}

int reporter_init(const char * parent, const char * name){
    dagent_config_t conf;
    conf.addr = "push:";
    conf.addr += parent;
    conf.name = name;
    conf.extmode = false;
    if (dagent_init(conf)){
        GLOG_TRA("dagent init error !");
        return -1;
    }
    return 0;
}
void reporter_destroy(){
    dagent_destroy();
}

void reporter_update(int timeout_ms){
    return dagent_update(timeout_ms);
}

int report_set(const char * k, const char * val){
    if (strchr(k, ':')){
        GLOG_TRA("error input param or k !");
        return -1;
    }
    std::string msg = k;
    msg += ":";
    msg += val;
    return dagent_send("dccollector", REPORT_MSG_SET, msg_buffer_t(msg));
}
int	report_inc(const char * k, int inc , const char * param){
    if (strchr(k, ':') || (param && strchr(param, ':'))){
        GLOG_TRA("error input param or k !");
        return -1;
    }
    std::string msg = k;
    msg += ":";
    msg += std::to_string(inc);
    if (param){
        msg += ":";
        msg += param;
    }
    return dagent_send("dccollector", REPORT_MSG_INC, msg_buffer_t(msg));
}
int report_dec(const char * k, int dec, const char * param){
    if (strchr(k, ':') || (param && strchr(param, ':'))){
        GLOG_TRA("error input param or k !");
        return -1;
    }
    std::string msg = k;
    msg += ":";
    msg += std::to_string(dec);
    if (param){
        msg += ":";
        msg += param;
    }
    return dagent_send("dccollector", REPORT_MSG_DEC, msg_buffer_t(msg));
}