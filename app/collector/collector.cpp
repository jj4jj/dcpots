#include "reporter/dccollector.h"
#include "base/logger.h"

int main(int argc, char * argv[]){
	logger_config_t		logger;
	logger.dir = "./";
	logger.pattern = "collector.log";
	if (global_logger_init(logger)){
		return -1;
	}
	if (collector_init("msgq:///tmp")){
		return -2;
	}
	while (true){
		collector_update(10);
		usleep(10 * 1000);
	}
	collector_destroy();

	return 0;
}