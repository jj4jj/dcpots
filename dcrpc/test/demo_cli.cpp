#include "../../base/stdinc.h"
#include "../client/dccrpc.h"
#include "../../base/dcutils.hpp"
#include "../../base/logger.h"
using namespace     dcsutil;

int fire=0;
static void sigh(int sig, siginfo_t * sig_info, void * ucontex){
    GLOG_DBG("signal:%d", sig);
    fire = 1;
}
int main(){
    RpcClient rpc;
    if (rpc.init()){
        return -1;
    }
    //dcsutil::nonblockfd(0);
    //	typedef void(*sah_handler)(int sig, siginfo_t * sig_info, void * ucontex);
    signalh_push(SIGINT, sigh);
    while (true){
        if (fire == 1 ){
            rpc.call("echo");
            fire = 0;
        }
        rpc.update();
        usleep(1000 * 10);
    }

    return 0;
}