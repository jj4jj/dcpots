#include "stdinc.h"
#include "dcutils.hpp"
#include "msg_buffer.hpp"
#include "msg_proto.hpp"
#include "cmdline_opt.h"
#include "eztimer.h"
#include "logger.h"
#include "dctcp.h"
#include "dcsmq.h"
#include "dcshm.h"
#include "dcdebug.h"

///////////////////////////////////////////////////
#include "app.hpp"

NS_BEGIN(dcsutil)

struct AppImpl {
    std::string		version { "0.0.1" };
    cmdline_opt_t * cmdopt{ nullptr };
    bool			stoping{ false };
    bool			reloading{ false };
	bool			restarting{ false };
	dctcp_t		*   stcp{ nullptr };
	std::unordered_map<uint32_t, App::timer_task_t>	task_pool;
	uint32_t		task_id{ 0 };
	int				console{ -1 };
	int				interval { 1000 * 10 };
	int				maxtps{ 500 };
	uint64_t		next_tick_time{ 0 };
};

static App * s_app_instance{ nullptr };
static inline void _app_register(App * app){
	if (s_app_instance){
		GLOG_FTL("repeat register app !");
		assert("repeat register app !" && false);
		return;
	}
	s_app_instance = app;
}
App & App::instance(){
	return *s_app_instance;
}

App::App(const char * ver){
    impl_ = new AppImpl;
    if (ver){
        impl_->version = ver;
    }
	_app_register(this);
}
App::~App(){
    if (impl_){
        if (impl_->cmdopt){
            delete impl_->cmdopt;
        }
		if (impl_->stcp){
			dctcp_destroy(impl_->stcp);
		}
        delete impl_;
    }
}

int App::on_init(const char  * config){
    UNUSED(config);
    GLOG_WAR("process initialize !");
    return 0;
}
int App::on_exit(){
    GLOG_WAR("process will exit !");
    return 0;
}
int  App::on_loop(){
    return 0;
}
void App::on_idle(){
}
bool App::on_stop(){
    GLOG_WAR("process will stop ...");
    return true;
}
int App::on_reload(){
    GLOG_WAR("process will reload config file:%p...", cmdopt().getoptstr("config"));
    return 0;
}
bool App::on_restart(){
    GLOG_WAR("process will stop for restarting...");
    return true;
}
const char * App::on_control(const char * cmdline){
    GLOG_IFO("process received a command line:%s", cmdline);
    return cmdline;
}
//typedef std::function<void()>   timer_task_t;
//ms: > 0 (just after ms excute once),0:(excute now),<0(period ms excute);
void App::shedule(timer_task_t task, int ms){
	impl_->task_id++;
	int max_try = INT_MAX;
	while (max_try-- > 0 && impl_->task_pool.find(impl_->task_id) !=
		impl_->task_pool.end()){
		impl_->task_id = impl_->task_id + 1;
		if (impl_->task_id == 0){
			impl_->task_id = 1;
		}
	}
	if (max_try < 0){
		GLOG_FTL("schedule reach max !");
		return;
	}
	impl_->task_pool[impl_->task_id] = task;	
	if (ms > 0){
		eztimer_run_after(ms, impl_->task_id, &impl_, sizeof(impl_));
    }
    else if (ms < 0){
		eztimer_run_every(ms, impl_->task_id, &impl_, sizeof(impl_));
    }
    else {
        task();
    }
}
std::string App::options(){
    return "";
}
static inline int init_command(App & app, const char * pidfile){
	if (app.cmdopt().hasopt("stop")){
		if (!pidfile){
			fprintf(stderr, "lacking command line option pid-file ...\n");
			return -1;
		}
		int killpid = lockpidfile(pidfile, SIGTERM, false);
		fprintf(stderr, "stoped process with normal stop mode [%d]\n", killpid);
		exit(0);
	}
	if (app.cmdopt().hasopt("restart")){
		if (!pidfile){
			fprintf(stderr, "lacking command line option pid-file ...\n");
			return -1;
		}
		int killpid = lockpidfile(pidfile, SIGUSR1, false);
		fprintf(stderr, "stoped process with restart mode [%d]\n", killpid);
		exit(0);
	}
	if (app.cmdopt().hasopt("reload")){
		if (!pidfile){
			fprintf(stderr, "lacking command line option pid-file ...\n");
			return -1;
		}
		int killpid = lockpidfile(pidfile, SIGUSR2, false);
		fprintf(stderr, "reloaded process [%d]\n", killpid);
		exit(0);
	}
	if (app.cmdopt().hasopt("console-shell")){
		const char * console = app.cmdopt().getoptstr("console-listen");
		if (!console){
			fprintf(stderr, "has no console-listen option open console shell error !\n");
			exit(-1);
		}
		string console_server = "tcp://";
		console_server += console;
		printf("connecting to %s ...\n", console_server.c_str());
		int confd = openfd(console_server.c_str(), "w", 3000);
		if (!confd){			
			fprintf(stderr, "connect error %s!\n", strerror(errno));
			exit(-1);
		}
		enum { CONSOLE_BUFFER_SIZE = 1024*1024};
		char * console_buffer = new char[CONSOLE_BUFFER_SIZE];
		printf("console server connected ! command <quit> will exit shell\n");
		while (true){
			int n = readfd(confd, console_buffer, CONSOLE_BUFFER_SIZE,
				"token:\r\n\r\n", 1000 * 3600);
			if (n < 0){
				fprintf(stderr, "console server closed [ret=%d]!\n", n);
				break;
			}
			printf("%s\n%s$", console_buffer, console);
			const char * command = fgets(console_buffer, CONSOLE_BUFFER_SIZE, stdin);
			if (strcasecmp(command, "quit") == 0){
				break;
			}
			dcsutil::writefd(confd, command, 0, "token:\r\n\r\n");
		}
		closefd(confd);
		delete console_buffer;
		exit(0);
	}
	return 0;
}
static inline int 
app_console_command(AppImpl * , const char * msg, int msgsz, int fd, dctcp_t * dc){
	string cmd(msg, msgsz);
	GLOG_IFO("session:%d recv msg:%s", cmd.c_str());
	const char * resp = App::instance().on_control(cmd.c_str());
	if (resp){
		return dctcp_send(dc, fd, dctcp_msg_t(resp, strlen(resp)));
	}
	return -1;
}
//typedef int(*dctcp_event_cb_t)(dctcp_t*, const dctcp_event_t & ev, void * ud);
static inline int 
app_console_listener(dctcp_t * dc, const dctcp_event_t & ev, void * ud){
    AppImpl * impl = (AppImpl*)ud;
    switch (ev.type){
	case DCTCP_NEW_CONNX:
		GLOG_IFO("console open session fd:%d", ev.fd);
		return dctcp_send(dc, ev.fd,
			dctcp_msg_t("Welcome login console !"));
		break;
	case DCTCP_CLOSED:
		GLOG_IFO("console close session fd:%d", ev.fd);
		break;
	case DCTCP_READ:
		return app_console_command(impl, ev.msg->buff, ev.msg->buff_sz, ev.fd, dc);
		break;
	default:
		return -1;
	}
	return 0;
}

static inline int
app_stcp_listener(dctcp_t * dc, const dctcp_event_t & ev, void * ud){
    GLOG_TRA("app stcp listener ev.type:%d ev.fd:%d ev.listenfd:%d",
        ev.type, ev.fd, ev.listenfd);
    AppImpl * impl = (AppImpl*)ud;
	GLOG_ERR("stcp listen fd:%d no listener !", ev.listenfd);
    UNUSED(dc);
    UNUSED(impl);
    return -1;
}

static int app_timer_dispatch(uint32_t ud, const void * cb, int sz){
	assert(sz == sizeof(AppImpl*));
	AppImpl * app = *(AppImpl**)cb;
	auto it = app->task_pool.find(ud);
	if (it != app->task_pool.end()){
		it->second();
	}
	return 0;
}
static inline void app_tick_update(AppImpl * impl_){
	uint64_t t_time_now = dcsutil::time_unixtime_us();
	if (t_time_now > impl_->next_tick_time){
		eztimer_update();
		dctcp_poll(impl_->stcp, impl_->maxtps);
		impl_->next_tick_time = t_time_now + impl_->interval;
	}
}

int App::init(int argc, const char * argv[]){
    int ret = 0;
    impl_->cmdopt = new cmdline_opt_t(argc, argv);
    string cmdopt_pattern;
	const char * program_name = dcsutil::path_base(argv[0]);
	#define		MAX_CMD_OPT_OPTION_LEN	(1024*4)
	size_t lpattern = strnprintf(cmdopt_pattern, 1024*4, ""
		"console-shell:n::console shell;"
		"start:n:S:start process normal mode;"
		"stop:n:T:stop process normal mode;"
		"restart:n:R:stop process with restart mode;"
		"name:r:n:set process name:%s;"
		"daemon:n:D:daemonlize start mode;"
		"log-dir:r::log dir settings:/tmp;"
		"log-file:r::log file pattern settings:%s;"
		"log-level:r::log level settings:INFO;"
		"log-size:r::log single file max size settings:20480000;"
		"log-roll:r::log max rolltation count settings:20;"
		"config:o:C:config file path;"
		"pid-file:r::pid file for locking (eg./tmp/%s.pid);"
		"console-listen:r::console command listen (tcp address);"
		"shm:r::keep process state shm key/path;"
		"tick-interval:r::tick update interval time (microseconds):10000;"
		"tick-maxproc:r::tick proc times once:1000;"
		"", program_name, program_name, program_name);

	snprintf((char*)(cmdopt_pattern.data()+lpattern), MAX_CMD_OPT_OPTION_LEN - lpattern,
			"%s", options().c_str());

    impl_->cmdopt->parse(cmdopt_pattern.data(), impl_->version.c_str());
    //////////////////////////////////////////////////////////////
	const char * pidfile = cmdopt().getoptstr("pid-file");
	//1.control command
	//"start:n:S:start process normal mode;"
	//"stop:n:T:stop process normal mode;"
	//"restart:n:R:stop process with restart mode;"
	ret = init_command(*this, pidfile);
	if (ret){
		GLOG_ERR("control command init error !");
		return -1;
	}

	//"start:n:S:start process normal mode;"
	if (!cmdopt().hasopt("start")){
		exit(0);
	}
	//2.daemonlization and pid running checking
	if (cmdopt().hasopt("daemon")){
		daemonlize(1, 0, pidfile);
	}	
	if (pidfile && getpid() != dcsutil::lockpidfile(pidfile)){
		fprintf(stderr, "process should be unique running ...");
		return -2;
	}
	dcsutil::signalh_ignore(SIGPIPE);
    struct signalh_function {
        static void term_stop(int, siginfo_t *, void *){
		    App::instance().stop();
	    }
        static void usr1_restart(int, siginfo_t *, void *){
		    App::instance().restart();
	    }
        static void usr2_reload(int, siginfo_t *, void *){
		    App::instance().reload();
	    }
        static void segv_crash(int signo, siginfo_t * info, void * ucontex){
		    if (ucontex) {
			    GLOG_ERR("program crash info: \n"
				"info.si_signo = %d \n"
				"info.si_errno = %d \n"
				"info.si_code  = %d (%s) \n"
				"info.si_addr  = %p\n",
				signo, info->si_errno,
				info->si_code,
				(info->si_code == SEGV_MAPERR) ? "SEGV_MAPERR" : "SEGV_ACCERR",
				info->si_addr);
                /////////////////////////////////////////////////////////////////
                //print stack info
                ucontext_t *uc = (ucontext_t *)ucontex;
                string strstack;
                const char * stackinfo = dcsutil::stacktrace(strstack, 0, 16, uc);
                GLOG_ERR("program crash stack info:\n%s", stackinfo);
		    }
            signalh_ignore(SIGSEGV);
	    }
    };
	dcsutil::signalh_push(SIGTERM, signalh_function::term_stop);
	dcsutil::signalh_push(SIGUSR1, signalh_function::usr1_restart);
	dcsutil::signalh_push(SIGUSR2, signalh_function::usr2_reload);
	dcsutil::signalh_push(SIGSEGV, signalh_function::segv_crash);

	//////////////////////////////////////////////////////////////
	//3.global logger
	logger_config_t lconf;
	lconf.dir = cmdopt().getoptstr("log-dir");
	lconf.pattern = cmdopt().getoptstr("log-file");
	lconf.lv = INT_LOG_LEVEL(cmdopt().getoptstr("log-level"));
	lconf.max_file_size = cmdopt().getoptint("log-size");
	lconf.max_roll = cmdopt().getoptint("log-roll");
	ret = global_logger_init(lconf);
	if (ret){
		fprintf(stderr, "logger init error = %d", ret);
		return -2;
	}
    //init timer
    ret = eztimer_init();
    if (ret){
        GLOG_ERR("eztimer init error :%d", ret);
        return -3;
    }
	eztimer_set_dispatcher(app_timer_dispatch);

	dctcp_config_t dconf;
	dconf.max_send_buff = 1024 * 1024;
	dconf.max_recv_buff = 1024 * 1024;
	dconf.max_tcp_send_buff_size = 1024 * 1024 * 4;
	dconf.max_tcp_recv_buff_size = 1024 * 1024 * 4;
	impl_->stcp = dctcp_create(dconf);
	if (!impl_->stcp){
		GLOG_SER("stcp init error !");
		return -4;
	}
	dctcp_event_cb(impl_->stcp, app_stcp_listener, impl_);
	//control
	const char * console_listen = cmdopt().getoptstr("console-listen");
	if (console_listen){
		impl_->console = dctcp_listen(impl_->stcp, console_listen, "token:\r\n\r\n",
            app_console_listener, impl_);
		if (impl_->console < 0){
			GLOG_SER("console init listen error : %d!", impl_->console);
			return -5;
		}
	}
	impl_->interval = cmdopt().getoptint("tick-interval");
	impl_->maxtps = cmdopt().getoptint("tick-maxproc");
    return on_init(cmdopt().getoptstr("config"));
}
int App::run(){
    int  iret = 0;
    bool bret = 0;
    while (true){
		app_tick_update(impl_);
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
dctcp_t       * App::stcp(){
    return impl_->stcp;
}

cmdline_opt_t & App::cmdopt(){
    return *impl_->cmdopt;
}

NS_END()
