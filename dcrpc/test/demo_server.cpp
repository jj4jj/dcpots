#include "../server/dcsrpc.h"
#include "../../base/logger.h"
#include "../../base/dcutils.hpp"
#include "../../base/cmdline_opt.h"

using namespace dcsutil;




int main(int argc, char ** argv){

    RpcServer server;
    //global_logger_init(logger_config_t());
    //logger_set_level(global_logger(), LOG_LVL_PROF);
    if (server.init()){
        GLOG_ERR("create server errror !");
        return -1;
    }
    struct EchoSvc : public RpcService {
        EchoSvc(const string & n):RpcService(n){
        }
        int call(){
            return 0;
        }
    };
    EchoSvc es("echo");
    server.regis(&es);
    while (true){
        server.update();
    }
    return 0;



}
