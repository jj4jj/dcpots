#include "dagent/dagent.h"
#include "dcnode/logger.h"
#include "../collector/report_colect.h"

int reporter_init(const char * keypath, const char * name){
	dagent_config_t conf;
	conf.localkey = keypath;
	conf.name = name;
	conf.routermode = false;
	if (dagent_init(conf)){
		LOGP("dagent init error !");
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
		LOGP("error input param or k !");
		return -1;
	}
	std::ostringstream os;
	os << k << ":" << val;
	return dagent_send("collector", REPORT_MSG_SET, msg_buffer_t(os.str()));
}
int	report_inc(const char * k, int inc = 1, const char * param = nullptr){
	if (strchr(k, ':') || (param && strchr(param, ':'))){
		LOGP("error input param or k !");
		return -1;
	}
	std::ostringstream os;
	os << k << ":" << inc;
	if (param){
		os << ":" << param;
	}
	return dagent_send("collector", REPORT_MSG_INC, msg_buffer_t(os.str()));
}
int report_dec(const char * k, int dec = 1, const char * param = nullptr){
	if (strchr(k, ':') || (param && strchr(param, ':'))){
		LOGP("error input param or k !");
		return -1;
	}
	std::ostringstream os;
	os << k << ":" << dec;
	if (param){
		os << ":" << param;
	}	
	return dagent_send("collector", REPORT_MSG_DEC, msg_buffer_t(os.str()));
}
static bool seted = false;
int reporter_using(){
	if (seted){
		return -1;
	}
	else if(dagent_ready()){
		LOGP("dagent is ready , so send msg ....");
		report_set("online", "200");
		report_inc("charge", 200);
		report_inc("charge", 400);
		report_inc("charge", 6700);
		report_dec("charge", 600);
		seted = true;
	}
	return 0;
}

int main(int argc, char * argv[]){
	logger_config_t		logger;
	logger.path = "./";
	logger.pattern = "reporter.log";
	if (global_logger_init(logger)){
		return -1;
	}
	if (reporter_init("/tmp/report-collector","reporter")){
		return -2;
	}
	while (true){
		reporter_update(10);
		reporter_using();
		usleep(10 * 1000);
	}
	reporter_destroy();

	return 0;
}