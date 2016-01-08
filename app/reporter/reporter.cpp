#include "reporter/dcreporter.h"
#include "base/logger.h"
static bool seted = false;
static int itimes = 0;
int reporter_using(){
	//GLOG_TRA("dagent is ready , so send msg ....");
	int ret = report_set("online", "200");
    ret |= report_inc("charge", 200);
    ret |= report_inc("charge", 400);
    ret |= report_inc("charge", 6700);
    ret |= report_dec("charge", 600);
    itimes += 5;
    if (itimes > 1000000 || ret){
        std::cout << "itimes:" << itimes << " ret:" << ret << std::endl;
        return -1; 
    }
	return 0;
}

int main(int argc, char * argv[]){
	logger_config_t		logger;
	logger.dir = "./";
    logger.lv = LOG_LVL_DEBUG;
	//logger.pattern = "reporter.log";
	if (global_logger_init(logger)){
		return -1;
	}
	if (reporter_init("msgq:///tmp","reporter")){
		return -2;
	}
	while (true){
		reporter_update(1);
        if (reporter_using()){
            break;
        }
        if (reporter_ready() < 0){
            break;//ERROR
        }
	}

	reporter_destroy();
	return 0;
}