#include "../../base/stdinc.h"
#include "../client/dccrpc.h"
#include "../../base/dcutils.hpp"
#include "../../base/logger.h"
#include "../share/dcrpc.h"

using namespace  dcsutil;
using namespace dcrpc;
int fire=0;
static void sigh(int sig, siginfo_t * sig_info, void * ucontex){
    GLOG_DBG("signal:%d", sig);
    fire = 1;
}
int main(){
    RpcClient rpc;
    if (rpc.init("127.0.0.1:8888")){
        return -1;
    }
    //dcsutil::nonblockfd(0);
    //	typedef void(*sah_handler)(int sig, siginfo_t * sig_info, void * ucontex);
    signalh_push(SIGINT, sigh);
    rpc.notify("push", [](int, const RpcValues & v){
        GLOG_IFO("push message:%d", v.length());
    });
    while (true){
        if (fire == 1 ){
            RpcValues args;
            args.adds("Hello,World!");
            rpc.call("echo", args, [](int ret, const RpcValues & v){
                GLOG_IFO("echo:%s", v.gets().c_str());
            });
            fire++;
        }
        if (fire == 2){
            RpcValues args;
            args.adds("Hello,World Pushing!");
            rpc.push("echo", args);
            fire == 0;
        }

        rpc.update();
        usleep(1000 * 10);
    }

    return 0;
}