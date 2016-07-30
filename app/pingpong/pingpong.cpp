#include "dcnode/dcnode.h"
#include "base/stdinc.h"
#include "base/dcutils.hpp"
#include "base/profile.h"
static int total = 0;
static int ncur = 0;
uint64_t	start_time = 0;
static int pingpong(void * ud, const char* src, const msg_buffer_t & msg)
{
	dcnode_t * dc = (dcnode_t*)(ud);
	ncur++;
	if (ncur == 1){
		if (start_time == 0){
			logger_set_level(nullptr, LOG_LVL_PROF);
		}
		start_time = dcsutil::time_unixtime_us();
	}
	if (ncur < 3){
		GLOG_TRA("recv msg from src:%s size:%d", src, msg.valid_size);
	}
	else{
		logger_set_level(nullptr,LOG_LVL_INFO);
	}
	if (ncur < total){
		dcnode_send(dc, src, msg.buffer, msg.valid_size);
	}
	else {
		uint64_t current_time = dcsutil::time_unixtime_us();
		uint64_t cost_time = current_time - start_time;
		double speed = total / cost_time;
		printf("pingpong test result msg length:%d count:%d time:%luus speed qpus:%lf qps:%lf\n",
			msg.valid_size, total, cost_time, speed, speed*1000000);
		dcnode_abort(dc);
	}
	return 0;
}

static dcnode_t* _create(const char* name, const std::string & addr ){
	dcnode_config_t dcf;
	dcf.addr = addr;
	dcf.max_channel_buff_size = 1024 * 1024;
	dcf.name = name;
	dcf.parent_heart_beat_gap = 2;
	dcf.max_children_heart_beat_expired = 5;
	auto dc = dcnode_create(dcf);
	if (!dc){
		GLOG_TRA("creat dcnode error !");
		return nullptr;
	}
	dcnode_set_dispatcher(dc, pingpong, dc);
	return dc;
}
void usage(){
	puts("./pingpong <dcnod addr> <max>");
	puts("eg:");
	puts("./pingpong push:msgq://./pingpong 10000000");
    puts("./pingpong pull:msgq://./pingpong 10000000");
    
    puts("./pingpong push:tcp://127.0.0.1:8888 10000000");
    puts("./pingpong pull:tcp://127.0.0.1:8888 10000000");

    puts("./pingpong proxy:tcp://127.0.0.1:8888->tcp://127.0.0.1:8889 10000000");
    puts("./pingpong proxy:tcp://127.0.0.1:8889->tcp://127.0.0.1:8810 10000000");
    puts("./pingpong proxy:tcp://127.0.0.1:8810->tcp://127.0.0.1:8811 10000000");
    puts("./pingpong proxy:tcp://127.0.0.1:8811->tcp://127.0.0.1:8812 10000000");
}
using namespace std;
int main(int argc, char * argv[]){
    logger_config_t loger_conf;
    default_logger_init(loger_conf);
    logger_set_level(nullptr, LOG_LVL_PROF);
    if (argc < 3){
        usage();
        return -1;
    }
    total = stoi(argv[2]);
    bool ping = false;
    string name = "pingpong";
    dcsutil::strcharsetrandom(name, 4);
    if (strstr(argv[1], "push")){
        ping = true;
    }
    if (!ping){
        logger_set_level(nullptr, LOG_LVL_INFO);
    }
    dcnode_t* dc = _create(name.c_str(), argv[1]);
    if (!dc){
        GLOG_TRA("create dncode error !");
        return -2;
    }
    int n = 0;
    string s_send_msg;
    dcsutil::strcharsetrandom(s_send_msg, 64);
    bool start = false;
	while (true)
	{
		PROFILE_FUNC();
		n = dcnode_update(dc, 100);
		if (!start && dcnode_ready(dc) == 1 && ping){
			dcnode_send(dc, "", s_send_msg.c_str(), s_send_msg.length());
			start = true;
			ncur = 1;
			start_time = dcsutil::time_unixtime_us();
		}
		if (dcnode_ready(dc) == -1){
			GLOG_TRA("dcnode stoped ....");
			return -1;
		}
	}
	return 0;
}

