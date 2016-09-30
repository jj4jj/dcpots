#pragma  once
//a general app framwork
struct cmdline_opt_t;
struct dctcp_t;
struct dcshmobj_user_t;
struct DateTime;
namespace dcs {
struct AppImpl;
class App {
public:
	static App & instance();
public:
    virtual int				on_create(int argc, const char * argv[]);//once, 0 is ok , error code
    virtual int				on_cmd_opt();//return != 0 : processd , >0 ok , < 0 error, return 0 is not processd
    virtual int				on_init();//once, 0 is ok , error code
    virtual int				on_loop();//running return for procssing
    virtual void			on_idle();//when idle
    virtual int				on_reload();//0 is ok , error code
    virtual bool			on_stop();//true: is ok
    virtual bool			on_restart();//true is ok
    virtual int				on_exit();//once
    virtual const char *	on_control(const char * cmdline);
    virtual std::string		options();

public:
    //using shm enable
    virtual std::vector<dcshmobj_user_t*>   shm_users();

public:
    const   char *  name() const;
    void            tick_interval(int interval);
    int             tick_interval() const;
    void            tick_maxproc(int maxproc);
    int             tick_maxproc() const;

public:
	const DateTime &		datetime() const;
	int						gmt_tz_offset() const;
	void					gmt_tz_offset(int tzo);
	int						time_offset() const;
	void					add_time_offset(int seconds);


public:
    int			init(int argc, const char * argv[]);
    int			start();
    void		stop();
    void		reload();
    void		restart();

public:
    cmdline_opt_t &					cmdopt();
    dctcp_t       *                 evloop();
    typedef std::function<void()>   timer_task_t;
    //ms: > 0 (just after ms excute once),0:(excute now),<0(period ms excute);
    void		                    shedule(timer_task_t task, int ms);
    void                            cmdopt(cmdline_opt_t & cmdopt);

protected:
	App(const char * version = __DATE__);
	virtual ~App();
protected:
	AppImpl * impl_{ nullptr};
};

template <class CAPP>
int AppMain(int argc, const char * argv[]){
    CAPP app;
    int ret = app.init(argc, argv);
    if (ret){
        GLOG_ERR("App(%s) init error:%d ", typeid(CAPP).name(), ret);
        return ret;
    }
    return app.start();
}

};


