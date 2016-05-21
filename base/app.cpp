#include "stdinc.h"
#include "dcutils.hpp"
#include "app.hpp"
#include "cmdline_opt.h"
#include "eztimer.h"
#include "logger.h"

NS_BEGIN(dcsutil)

struct AppImpl {
    std::string  version { "0.0.1" };
    cmdline_opt_t * cmdopt{ nullptr };
    bool        stoping{ false };
    bool        reloading{ false };
    bool        restarting{ false };
};

App::App(const char * ver){
    impl_ = new AppImpl;
    if (ver){
        impl_->version = ver;
    }
}
App::~App(){
    if (impl_){
        if (impl_->cmdopt){
            delete impl_->cmdopt;
        }
        delete impl_;
    }
}
int App::on_init(const char  * config){
    UNUSED(config);
    return 0;
}
int App::on_exit(){
    return 0;
}
int  App::on_loop(){
    return 0;
}
void App::on_idle(){
}
bool App::on_stop(){
    return true;
}
int App::on_reload(){
    return 0;
}
bool App::on_restart(){
    return true;
}
const char * App::on_control(const char * cmdline){
    return cmdline;
}
//typedef std::function<void()>   timer_task_t;
//ms: > 0 (just after ms excute once),0:(excute now),<0(period ms excute);
void App::shedule(timer_task_t task, int ms){
    if (ms > 0){
        eztimer_run_after(ms, 0, nullptr, 0);
    }
    else if (ms < 0){
        eztimer_run_every(ms, 0, nullptr, 0);
    }
    else {
        task();
    }
}
std::string App::options(){
    return "";
}

int App::init(int argc, const char * argv[]){
    int ret = 0;
    impl_->cmdopt = new cmdline_opt_t(argc, argv);
    string pattern;
    strnprintf(pattern, 1024, ""
        "daemon:n:D:daemonlize start mode;"
        "log-dir:r::log dir settings:/tmp;"
        "log-file:r::log file pattern settings:%s.log;"
        "log-level:r::log level settings:INFO;"
        "log-size:r::log single file max size settings:20480000;"
        "log-roll:r::log max rolltation count settings:20;"
        "config:o:C:config file path;"
        "pid-file:r::pid file for locking:/tmp/%s.pid;"
        "", argv[0], argv[0]);
    pattern += options();
    impl_->cmdopt->parse(pattern.data(), impl_->version.c_str());
    //////////////////////////////////////////////////////////////
    //init log
    //init pid.lock
    //init timer
    ret = eztimer_init();
    if (ret){
        GLOG_ERR("eztimer init error :%d", ret);
        return ret;
    }


    return on_init(cmdopt().getoptstr("config"));
}
int App::run(){
    int  iret = 0;
    bool bret = 0;
    while (true){
        eztimer_update();
        if (impl_->stoping){ //need stop
            bret = on_stop();
            if (bret){
                break;
            }
            continue;
        }
        if (impl_->restarting){//need restart
            bret = on_restart();
            if (bret){
                break;
            }
            continue;
        }
        if (impl_->reloading){//need reload
            iret = on_reload();
            if (iret){
                GLOG_ERR("reload error ret :%d !", iret);
            }
            impl_->reloading = false;
        }
        //running
        iret = on_loop();
        if (iret == 0){
            on_idle();
        }
    }
    return on_exit();
}
void App::stop(){
    impl_->stoping = true;
}
void App::reload(){
    impl_->reloading = true;
}
void App::restart(){
    impl_->restarting = true;
}
cmdline_opt_t & App::cmdopt(){
    return *impl_->cmdopt;
}

NS_END()
