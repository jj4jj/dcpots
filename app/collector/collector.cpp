#include "dagent/dagent.h"
#include "base/logger.h"
#include "report_colect.h"

int on_report_set(const msg_buffer_t &  msg, const char * src){
	GLOG_TRA("recv from :%s set msg:%s",src, msg.buffer);
	return 0;
}
int	on_report_inc(const msg_buffer_t &  msg, const char * src){
	GLOG_TRA("recv from :%s inc msg:%s", src, msg.buffer);
	return 0;
}
int on_report_dec(const msg_buffer_t &  msg, const char * src){
	GLOG_TRA("recv from :%s dec msg:%s", src, msg.buffer);
	return 0;
}

int collector_init(const char * keypath, const char * name){
	dagent_config_t conf;
	conf.localkey = keypath;
	conf.name = name;
	conf.routermode = true;
	if (dagent_init(conf)){
		GLOG_TRA("dagent init error !");
		return -1;
	}
	dagent_cb_push(REPORT_MSG_SET, on_report_set);
	dagent_cb_push(REPORT_MSG_INC, on_report_set);
	dagent_cb_push(REPORT_MSG_DEC, on_report_set);
	//
	return 0;
}
void collector_destroy(){
	dagent_destroy();
}

void collector_update(int timeout_ms){
	dagent_update(timeout_ms);
}

int collector_using(){
	return 0;
}

int main(int argc, char * argv[]){
	logger_config_t		logger;
	logger.dir = "./";
	logger.pattern = "collector.log";
	if (global_logger_init(logger)){
		return -1;
	}
	if (collector_init("/tmp/report-collector", "collector")){
		return -2;
	}
	while (true){
		collector_update(10);
		collector_using();
		usleep(10 * 1000);
	}
	collector_destroy();

	return 0;
}