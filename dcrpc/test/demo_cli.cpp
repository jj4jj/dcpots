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
    //signalh_push(SIGINT, sigh);
    rpc.notify("push", [](int, const RpcValues & v){
        GLOG_IFO("push message:%d", v.length());
    });
    static const int test_time = 10000000;
    static int times = test_time;
    RpcValues args;
    args.adds("Hello,World!");
    static uint64_t s_start_time = 0;
    while (true){
        if (rpc.ready()){
            if (s_start_time == 0){
                logger_set_level(global_logger(), LOG_LVL_INFO);
                s_start_time = time_unixtime_us();
            }
            if(times > 0){
                for (int i = 0; i < 80; ++i){
                    rpc.call("echo", args, [&](int ret, const RpcValues & v){
                        times--;
                    });
                }
            }
            else {
                GLOG_IFO("time us diff:%ldus qps:%lf", time_unixtime_us() - s_start_time,
                    1000.0*1000*test_time / (time_unixtime_us() - s_start_time));
                return 0;
            }
        }
        if (fire == 1 ){
            rpc.call("echo", args, [](int ret, const RpcValues & v){
                GLOG_IFO("echo:%s", v.gets().c_str());
            });
            fire++;
        }
        else if (fire == 2){
            RpcValues args;
            args.adds("Hello,World Pushing!");
            rpc.push("echo", args);
            fire == 0;
        }
        rpc.update();
    }

    return 0;
}
