#include "base/stdinc.h"
#include "base/cmdline_opt.h"
#include "../api/mongoproxy_api.h"
#include "base/logger.h"
#include "test.pb.h"
#include "utility/util_proto.h"

void test_cb(mongoproxy_cmd_t cmd, void * ud, const mongoproxy_result_t & result){
    GLOG_TRA("cmd:%d status:%d ok:%d nquery:%d nupdate:%d msg num:%d cb_data:%s cb_length:%d",
        cmd, result.status,
        result.ok, result.nq, result.nu,
        result.results.size(),
        result.cb_data, result.cb_size);
    for (int i = 0; i < result.results.size(); ++i){
        GLOG_DBG("result [%d] %s => %s", i,
            result.results[i].first.c_str(),
            result.results[i].second->ShortDebugString().c_str());
    }
}
int main(int argc, char ** argv){
	using namespace std;
	cmdline_opt_t cmdopt(argc, argv);
	cmdopt.parse("path:r:p:proxy communication msgq path:/tmp");
	dcsutil::protobuf_logger_init();
	//path
	if (!cmdopt.hasopt("path")){
		std::cerr << "no path option" << endl;
		cmdopt.pusage();
		return -1;
	}
	int ret = mongoproxy_init(cmdopt.getoptstr("path"));
	if (ret){
		cerr << "mongoproxy init error !" << endl;
		return -2;
	}

	mongoproxy_set_cmd_cb(test_cb, 0);
	//curd
	test_db::test_coll_person	tcp;
	tcp.set_age(20);
	tcp.set_name("ffff");
	tcp.set_zipcode("ffff");
	int i = 0;
    while (true){
        mongoproxy_poll();
        if (0 == i && !mongoproxy_insert(tcp)){
            ++i;
        }
        else if (1 == i && !mongoproxy_find(tcp)){
            ++i;
        }
        /*
        else if (2 == i && !mongoproxy_count(tcp)){
        ++i;
        }
        else if (3 == i ){//&& !mongoproxy_remove(tcp)){
        ++i;
        }
        else if (4 == i){
        const char * pcb = "hello,world!this is cb data";
        int cblen = strlen(pcb);
        tcp.set_zipcode("updated!!");
        if (!mongoproxy_update(tcp, "name", pcb, cblen)){
        ++i;
        }
        }
        */

		sleep(1);
	}
	return 0;
}