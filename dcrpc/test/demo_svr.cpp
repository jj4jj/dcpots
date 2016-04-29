#include "../server/dcsrpc.h"
#include "../../base/logger.h"
#include "../../base/dcutils.hpp"
#include "../../base/cmdline_opt.h"

using namespace dcsutil;
using namespace dcrpc;
int main(int argc, char ** argv){
    RpcServer server;
    //global_logger_init(logger_config_t());
    //logger_set_level(global_logger(), LOG_LVL_PROF);
    if (server.init("127.0.0.1:8888")){
        GLOG_ERR("create server errror !");
        return -1;
    }
    RpcService echo("echo");
    server.regis(&echo);
    logger_set_level(global_logger(), LOG_LVL_INFO);
    while (true){
        server.update();
    }
    return 0;



}
