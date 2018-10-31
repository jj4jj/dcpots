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
#include "dcshmobj.hpp"
#include "datetime.h"
///////////////////////////////////////////////////
#include "app_stat.h"
#include "app.h"

NS_BEGIN(dcs)

void AppStatItem::clear() {
    total_time_ns = 0;
    max_time_ns = 0;
    min_time_ns = 0;
    ncall = 0;
    nproc = 0;
}
void AppStatItem::begin() {
    call_begin_time_ns = dcs::time_unixtime_ns();
}
int AppStatItem::end(int nproc_) {
    uint64_t cost_time_ns = dcs::time_unixtime_ns() - call_begin_time_ns;
    total_time_ns += cost_time_ns;
    if (cost_time_ns > max_time_ns) {
        max_time_ns = cost_time_ns;
    }
    if (cost_time_ns < min_time_ns) {
        min_time_ns = cost_time_ns;
    }
    ++ncall;
    if (nproc_ > 0) {
        nproc += nproc_;
    }
    return (cost_time_ns + 999) / 1000;
}
void AppRunStat::start() {
    start_time_us = dcs::time_unixtime_us();
    total_time_us = 0;
    next_update_time_s = start_time_us / (1000 * 1000) + stat_update_period;
    strlog.reserve(1024 + 256 * APP_RUN_STAT_ITEM_MAX);
}
void AppRunStat::update(uint64_t time_now_us) {
    total_time_us = time_now_us - start_time_us;
    uint32_t timenow_s = time_now_us / (1000 * 1000);
    if (timenow_s > next_update_time_s) {
        //clear
        for (int i = 0; i < APP_RUN_STAT_ITEM_MAX; ++i) {
            item[i].clear();
        }
        next_update_time_s = timenow_s + stat_update_period;
    }
}
const char * AppRunStat::log() {
    std::string strtime;
    int n = dcs::strprintf(strlog, "#AppRuningStat# period:%d start-time:%s total-run-time:%ds ",
        stat_update_period, dcs::strftime(strtime, start_time_us / (1000 * 1000)),
        total_time_us / (1000 * 1000));
    static const char * item_name_dict[APP_RUN_STAT_ITEM_MAX] = { "idle","tick", "loop", };
    for (int i = 0; i < APP_RUN_STAT_ITEM_MAX; ++i) {
        n += snprintf((char*)strlog.data() + n, 192, " | name:%s time total:%lums max:%uns min:%uns avg:%luns ncall:%lu nproc:%lu",
            item_name_dict[i], item[i].total_time_ns / (1000 * 1000),
            item[i].max_time_ns, item[i].min_time_ns,
            item[i].ncall > 0 ? (item[i].total_time_ns / item[i].ncall) : 0,
            item[i].ncall, item[i].nproc);

    }
    ((char*)strlog.data())[n] = 0;
    return strlog.data();
}
//////////////////////////////////////////////////////////////////////////
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
	int				max_proc_tick{ 500 };
    uint64_t        time_now_us {0};
	uint64_t		next_tick_time_us{ 0 };
    dcshmobj_pool   shm_pool;
	DateTime		datetime;
    AppRunStat      stat;
    int             hz {0};
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
int	App::on_cmd_opt(){
    return 0;
}
int	App::on_create(int argc, const char * argv[]){//once, 0 is ok , error code
    UNUSED(argc);
    UNUSED(argv);
    return 0;
}
int App::on_init(){
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
int App::on_tick(){
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
const char * App::coredump(){
    return "some important info when crash ocurred !";
}
void  App::cmdopt(cmdline_opt_t & cmdopt){
    assert("cmdline options must be set most once !" && !this->impl_->cmdopt);
    this->impl_->cmdopt = &cmdopt;
}

//typedef std::function<void()>   timer_task_t;
//ms: > 0 (just after ms excute once),0:(excute now),<0(period ms excute);
void App::schedule(timer_task_t task, int ms){
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
		eztimer_run_every(-ms, impl_->task_id, &impl_, sizeof(impl_));
    }
    else {
        task();
    }
}
std::string App::options(){
    return "";
}
std::vector<dcshmobj_user_t*>   App::shm_users() {
    return std::vector<dcshmobj_user_t*>();
}
static inline int _shm_command(App & app){
    const char * shm_keypath = app.cmdopt().getoptstr("shm");
    bool need_exit = false;
    bool clear_shm_recover = app.cmdopt().hasopt("shm-clear-recover");
    bool clear_shm_backup = app.cmdopt().hasopt("shm-clear-backup");
    if (!shm_keypath && (clear_shm_recover || clear_shm_backup)){
        GLOG_ERR("not found shm path param for clearing shm!");
        return -1;
    }
    if (clear_shm_recover){
        dcshm_set_delete(dcshm_path_key(shm_keypath, 1));
        need_exit = true;
    }
    if (clear_shm_backup){
        dcshm_set_delete(dcshm_path_key(shm_keypath, 2));
        need_exit = true;
    }
    if (need_exit){
        return 1;
    }
    return 0;
}
//return 0: continue, 
//return -1(<0): error
//return 1(>0): exit success
static inline int init_command(App & app, const char * pidfile){
    int ret = 0;
	if (app.cmdopt().hasopt("stop")){
		if (!pidfile){
			fprintf(stderr, "lacking command line option pid-file ...\n");
			return -1;
		}
		int killpid = lockpidfile(pidfile, SIGTERM, true);
		fprintf(stderr, "stoped process with normal stop mode [%d]\n", killpid);
        return 1;
	}
	if (app.cmdopt().hasopt("restart")){
		if (!pidfile){
			fprintf(stderr, "lacking command line option pid-file ...\n");
			return -1;
		}
		int killpid = lockpidfile(pidfile, SIGUSR1, true);
		fprintf(stderr, "stoped process with restart mode [%d]\n", killpid);
        return 1;
    }
	if (app.cmdopt().hasopt("reload")){
		if (!pidfile){
			fprintf(stderr, "lacking command line option pid-file ...\n");
			return -1;
		}
		int killpid = lockpidfile(pidfile, SIGUSR2, true, nullptr, true);
		fprintf(stderr, "reloaded process [%d]\n", killpid);
        return 1;
    }
	if (app.cmdopt().hasopt("console-shell")){
		const char * console = app.cmdopt().getoptstr("console-listen");
		if (!console){
			fprintf(stderr, "has no console-listen option open console shell error !\n");
			return -1;
		}
		string console_server = "tcp://";
		console_server += console;
		printf("connecting to %s ...\n", console_server.c_str());
		int confd = openfd(console_server.c_str(), "w", 3000);
		if (!confd){			
			fprintf(stderr, "connect error %s!\n", strerror(errno));
			return -1;
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
			dcs::writefd(confd, command, 0, "token:\r\n\r\n");
		}
		closefd(confd);
		delete console_buffer;
        return 1;
    }
    ///////////////////////////////////////////////////////////////////
    ret = _shm_command(app);
    if (ret < 0){
        GLOG_ERR("shm command check error:%d !", ret);
        return -1;
    }
    if (ret > 0){
        return 1;
    }
	return app.on_cmd_opt();
}
static inline int app_console_command(AppImpl * , const char * msg, int msgsz, int fd, dctcp_t * dc){
	string cmd(msg, msgsz);
	GLOG_IFO("session:%d recv msg:%s", cmd.c_str());
	const char * resp = App::instance().on_control(cmd.c_str());
	if (resp){
		return dctcp_send(dc, fd, dctcp_msg_t(resp, strlen(resp)));
	}
	return -1;
}
//typedef int(*dctcp_event_cb_t)(dctcp_t*, const dctcp_event_t & ev, void * ud);
static inline int app_console_listener(dctcp_t * dc, const dctcp_event_t & ev, void * ud){
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

static inline int app_stcp_listener(dctcp_t * dc, const dctcp_event_t & ev, void * ud){
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
const   char *  App::program() const {
    return const_cast<App*>(this)->cmdopt().program();
}
void        dcs::App::stat_interval(int interval)  {
    impl_->stat.stat_update_period = interval;
}

const AppRunStat & dcs::App::stat() const{
    return impl_->stat;
}
const   char *  App::name() const {
    return const_cast<App*>(this)->cmdopt().getoptstr("name");
}
static inline void init_signal(){
    struct signalh_function {
        static void term_stop(int sig, siginfo_t *, void *){
            App::instance().stop();
            dcs::signalh_ignore(sig);
        }
        static void usr1_restart(int sig, siginfo_t *, void *){
            App::instance().restart();
            dcs::signalh_ignore(sig);
        }
        static void usr2_reload(int, siginfo_t *, void *){
            App::instance().reload();
        }
        static void segv_crash(int signo, siginfo_t * info, void * ucontex){
            if (ucontex) {
                //
                const char * namep = App::instance().name();                
                std::string strfcname;
                strfcname.reserve(128);
                std::string strft;
                string strstack;
                dcs::strprintf(strfcname, "/tmp/core.%s.%s.log", namep, dcs::strftime(strft, time(NULL), "%y%m%d_%H%M%S"));
                FILE * fp = fopen(strfcname.c_str(), "w");
                if(fp){
                    fprintf(fp, "crash app stamp:[%s]\n", App::instance().coredump());
                    fflush(fp);
                    fprintf(fp, "program crash info: \n"
                             "info.si_signo = %d \n"
                             "info.si_errno = %d \n"
                             "info.si_code  = %d (%s) \n"
                             "info.si_addr  = %p\n",
                             signo, info->si_errno,
                             info->si_code,
                             (info->si_code == SEGV_MAPERR) ? "SEGV_MAPERR" : "SEGV_ACCERR",
                             info->si_addr);
                    fflush(fp);
                    /////////////////////////////////////////////////////////////////////////
                    //print stack info
                    ucontext_t *uc = (ucontext_t *)ucontex;
                    const char * stackinfo = dcs::stacktrace(strstack, 0, 16, uc);
                    fprintf(fp, "program crash stack info:\n%s\n", stackinfo);
                    fflush(fp);
                    fclose(fp);
                }
            }
            dcs::signalh_default(signo);
        }
    };
    /////////////////////////////////////////////////////////////////
    dcs::signalh_ignore(SIGPIPE);
    dcs::signalh_push(SIGTERM, signalh_function::term_stop);
    dcs::signalh_push(SIGUSR1, signalh_function::usr1_restart);
    dcs::signalh_push(SIGUSR2, signalh_function::usr2_reload);
    //dcs::signalh_push(SIGSEGV, signalh_function::segv_crash);
}
static inline int init_facilities(App & app, AppImpl * impl_){
    //3.global logger
    logger_config_t lconf;
    lconf.path = app.cmdopt().getoptstr("log-path");
    lconf.lv = INT_LOG_LEVEL(app.cmdopt().getoptstr("log-level"));
    lconf.max_file_size = app.cmdopt().getoptint("log-size");
    lconf.max_roll = app.cmdopt().getoptint("log-roll");
    int ret = default_logger_init(lconf);
    if (ret){
        fprintf(stderr, "logger init error = %d", ret);
        return -2;
    }
    impl_->interval = app.cmdopt().getoptint("tick-interval");
    if(impl_->interval == 0){
        impl_->interval = 10*1000;
    }
    impl_->max_proc_tick = app.cmdopt().getoptint("tick-max-proc");
    impl_->stat.stat_update_period = app.cmdopt().getoptint("run-stat-interval");
    impl_->hz = (1000*1000)/impl_->interval;
    //init timer
    ret = eztimer_init();
    if (ret){
        GLOG_ERR("eztimer init error :%d", ret);
        return -3;
    }
    eztimer_set_dispatcher(app_timer_dispatch);
    impl_->stcp = dctcp_default_pump();
    if (!impl_->stcp){
        GLOG_SER("dctcp loop init error !");
        return -4;
    }
    dctcp_event_cb(impl_->stcp, app_stcp_listener, impl_);
    //control
    const char * console_listen = app.cmdopt().getoptstr("console-listen");
    if (console_listen){
        impl_->console = dctcp_listen(impl_->stcp, console_listen, "token:\r\n\r\n",
            app_console_listener, impl_);
        if (impl_->console < 0){
            GLOG_SER("console init listen error : %d!", impl_->console);
            return -5;
        }
    }
    //tzo set
    app.gmt_tz_offset(impl_->cmdopt->getoptint("tzo"));    
    //////////////////////////////////////////////////////////////////////////////////
    std::vector<dcshmobj_user_t*>   shmusers = app.shm_users();
    if (!shmusers.empty()){
        const char * shmkey = app.cmdopt().getoptstr("shm");
        if (!shmkey){
            GLOG_ERR("not config shm key path setting !");
            return -6;
        }
        for (size_t i = 0; i < shmusers.size(); ++i){
            impl_->shm_pool.regis(shmusers[i]);
        }

        ret = impl_->shm_pool.start(shmkey);
        if (ret){
            GLOG_ERR("shm pool start error ret:%d shm path:%s !", ret, shmkey);
            return -7;
        }
    }
    //////////////////////////////////////////////////////////////////////////

    return 0;
}
static inline int init_arguments(int argc, const char * argv[], AppImpl * impl_, App & app){
    assert(!impl_->cmdopt);
    impl_->cmdopt = new cmdline_opt_t(argc, argv);
    string cmdopt_pattern;
    const char * program_name = dcs::path_base(argv[0]);
    #define		MAX_CMD_OPT_OPTION_LEN	(1024*32)
    size_t lpattern = strnprintf(cmdopt_pattern, 1024 * 8, ""
                                    "console-shell:n::console shell(connect with --console-listen);"
                                    "start:n:S:start process normal mode;"
                                    "stop:n:T:stop process normal mode;"
                                    "restart:n:R:stop process with restart mode;"
                                    "reload:n:r:reload process config and some settings;"
                                    "shm-clear-recover:n::clear the master share memory for recover;"
                                    "shm-clear-backup:n::clear the backup share memory for recover;"
                                    "name:r:N:set process name:%s;"
                                    "tzo:r::set app gmt timezone offset to east of UTC:+0800;"
                                    "daemon:n:D:daemonlize start mode;"
                                    "log-path:r::log file pattern settings:stdout;"
                                    "log-level:r::log level settings:INFO;"
                                    "log-size:r::log single file max size settings:20480000;"
                                    "log-roll:r::log max rolltation count settings:20;"
                                    "pid-file:r::pid file for locking (eg./tmp/%s.pid);"
                                    "console-listen:r::console command listen (tcp address);"
                                    "shm:r::keep process state shm key/path;"
                                    "tick-interval:r::tick update interval time (us):10000;"
                                    "tick-max-proc:r::tick proc times once:5000;"
                                    "run-stat-interval:o::update run stat period (s):60;"
                                    "", program_name, program_name);
    snprintf((char*)(cmdopt_pattern.data() + lpattern), MAX_CMD_OPT_OPTION_LEN - lpattern,
                "%s", app.options().c_str());
    impl_->cmdopt->parse(cmdopt_pattern.data(), impl_->version.c_str());
    return 0;
}
//////////////////////////////////////////////////////////
static inline void _app_reload_env(AppImpl * impl_){
	UNUSED(impl_);
}
int App::init(int argc, const char * argv[]){
    srand(time(NULL));
    impl_->time_now_us = dcs::time_unixtime_us();
    impl_->stat.start();
    //random env
    int ret = on_create(argc, argv);
    if (ret){
        GLOG_ERR("app check start error:%d !", ret);
        return -1;
    }
    ret = init_arguments(argc, argv, impl_, *this);
    if (ret){
        GLOG_ERR("init argumets error :%d !", ret);
        return -1;
    }
    _app_reload_env(impl_);
    //////////////////////////////////////////////////////////////
    const char * pidfile = cmdopt().getoptstr("pid-file");
    //1.control command
    ret = init_command(*this, pidfile);
    if (ret){
        if (ret < 0){
            GLOG_ERR("control command init error:%d !", ret);
            return -1;
        }
        else {
            exit(0);
        }
    }
    //"start:n:S:start process normal mode;"
    if (!cmdopt().hasopt("start")){
        exit(-1);
    }
    //2.daemonlization and pid running checking
    if (cmdopt().hasopt("daemon")){
        daemonlize(1, 0, pidfile);
    }
    if (pidfile && getpid() != dcs::lockpidfile(pidfile)){
        fprintf(stderr, "process should be unique running ...");
        return -2;
    }
    init_signal();
    //////////////////////////////////////////////////////////////
    ret = init_facilities(*this, impl_);
    if (ret){
        GLOG_ERR("init facilities error:%d !", ret);
        return -1;
    }
    GLOG_IFO("app framework init success !");
    //////////////////////////////////////////////////////////////////////////////////
    return on_init();
}
static inline int _app_tick(App * app, AppImpl * impl_){
    impl_->datetime.update();
    impl_->time_now_us = dcs::time_unixtime_us();
    impl_->stat.update(impl_->time_now_us);
    int nproc = 0;
    if (impl_->time_now_us > impl_->next_tick_time_us) {
        impl_->stat.item[APP_RUN_STAT_ITEM_TICK].begin();
        nproc += app->on_tick();
        eztimer_update();
        impl_->stat.item[APP_RUN_STAT_ITEM_TICK].end(nproc);
        impl_->next_tick_time_us = impl_->time_now_us + impl_->interval;
    }
    return nproc;
}
static inline void _app_idle(App * app, AppImpl * impl_){
    impl_->stat.item[APP_RUN_STAT_ITEM_IDLE].begin();
    int sleep_time_us = impl_->interval;
    uint64_t start_time = dcs::time_unixtime_us();
    app->on_idle();
    int rest_time_us = sleep_time_us - (dcs::time_unixtime_us() - start_time);
    if (rest_time_us > 0) {
        usleep(rest_time_us);
    }
    impl_->stat.item[APP_RUN_STAT_ITEM_IDLE].end();
}
int App::start(){
	int  nproc = 0;/* , ntick = 0;*/
    bool bstat = 0;
    while (true){
        nproc = 0;
        //ntick = _app_tick(this, impl_);
		_app_tick(this, impl_);
        if (impl_->stoping){ //need stop
            bstat = on_stop();
            if (bstat){
                break;
            }
        }
        else if (impl_->restarting){//need restart
            bstat = on_restart();
            if (bstat){
                break;
            }
        }
        else if (impl_->reloading){//need reload
            _app_reload_env(impl_);
            int rret = on_reload();
            if (rret){
                GLOG_ERR("reload error ret :%d !", nproc);
            }
            impl_->reloading = false;
        }
        //////////////////////////////////////////////
        impl_->stat.item[APP_RUN_STAT_ITEM_LOOP].begin();
        //running
        if (impl_->stcp) { //one tick , one us ?
            nproc += dctcp_poll(impl_->stcp, impl_->interval, impl_->max_proc_tick);
        }
        nproc += on_loop();
        impl_->stat.item[APP_RUN_STAT_ITEM_LOOP].end(nproc);
        if (nproc == 0){
            _app_idle(this, impl_);
        }
    }
    nproc = impl_->shm_pool.stop();
    if (nproc){
        GLOG_ERR("shm pool stop error ret:%d", nproc);
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
dctcp_t       * App::evloop(){
    return impl_->stcp;
}

cmdline_opt_t & App::cmdopt(){
    return *impl_->cmdopt;
}
void            App::tick_interval(int interval){
    if(interval > 0){
        impl_->interval = interval;
        impl_->hz = (1000 * 1000) / impl_->interval;
    }
}
int             App::tick_hz() const {
    return impl_->hz;
}
int             App::tick_interval() const{
    return impl_->interval;
}
void             App::tick_maxproc(int maxproc){
    impl_->max_proc_tick = maxproc;
}
int             App::tick_maxproc() const{
    return impl_->max_proc_tick;
}
const DateTime &	App::datetime() const {
	return impl_->datetime;
}
int             App::gmt_tz_offset() const{
	int n = impl_->datetime.gmt_offset();
	return n/3600*100 + n/60%60;
}
void            App::gmt_tz_offset(int tzo){
    int hourof = (tzo / 100) % 100;
    int minof = tzo%100;
    if (impl_->datetime.set_gmt_offset(hourof*3600 + minof*60)){
		GLOG_ERR("error gmt tz offset :%+05d", tzo);
		return;
    }
    GLOG_IFO("set time zone gmt offset to east of UTC:[%+05d] now:[%s] timezone:[%s]",
	 tzo, datetime().format(datetime().now()).c_str(), datetime().timezone());
}
int				App::time_offset() const {
	return impl_->datetime.time_offset();
}
void			App::add_time_offset(int seconds) {
	if (impl_->datetime.add_time_offset(seconds)) {
		GLOG_ERR("error set time offset :%d", seconds);
		return;
	}
	GLOG_IFO("add time seconds offset %d", seconds);
}
void            App::clear_time_offset(){
    impl_->datetime.add_time_offset(-1 * impl_->datetime.time_offset());
}
NS_END()
