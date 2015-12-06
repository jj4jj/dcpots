#include "base/stdinc.h"
#include "base/cmdline_opt.h"
#include "../api/mongoproxy_api.h"
#include "base/logger.h"

void test_cb(mongoproxy_cmd_t cmd, void * ud, const mongoproxy_result_t & result){
	LOGP("cmd:%d %d %p", cmd, result.status, result.msg);
}
int main(int argc, char ** argv){
	using namespace std;
	cmdline_opt_t cmdopt(argc, argv);
	cmdopt.parse("path:r:p:proxy communication msgq path");
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


	return 0;
}