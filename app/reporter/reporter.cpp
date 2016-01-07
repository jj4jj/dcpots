#include "dcreporter.h"
#include "base/logger.h"
static bool seted = false;
static int itimes = 0;
int reporter_using(){
	if (seted){
		return -1;
	}
	else if(reporter_ready() == 1){
		GLOG_TRA("dagent is ready , so send msg ....");
		report_set("online", "200");
		report_inc("charge", 200);
		report_inc("charge", 400);
		report_inc("charge", 6700);
		report_dec("charge", 600);
        itimes += 5;
		seted = true;
	}
	return 0;
}

int main(int argc, char * argv[]){
	logger_config_t		logger;
	logger.dir = "./";
	logger.pattern = "reporter.log";
	if (global_logger_init(logger)){
		return -1;
	}
	if (reporter_init("msgq:///tmp","reporter")){
		return -2;
	}
	while (true){
		reporter_update(10);
		reporter_using();
        if (reporter_ready() < 0){
            break;//ERROR
        }
	}

	reporter_destroy();
	return 0;
}