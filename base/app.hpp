#pragma  once
//a general app framwork
struct cmdline_opt_t;
struct dctcp_t;

namespace dcsutil {
struct AppImpl;
class App {
public:
	static App & instance();
public:
    virtual std::string		options();
	virtual int				on_init(const char * config);//once, 0 is ok , error code
    virtual int				on_loop();//running return for procssing
    virtual void			on_idle();//when idle
    virtual int				on_reload();//0 is ok , error code
    virtual bool			on_stop();//true: is ok
    virtual bool			on_restart();//true is ok
    virtual int				on_exit();//once
    virtual const char *	on_control(const char * cmdline);
public:
    int			init(int argc, const char * argv[]);
    int			run();
    void		stop();
    void		reload();
    void		restart();

public:
    cmdline_opt_t &					cmdopt();
    dctcp_t       *                 stcp();
    typedef std::function<void()>   timer_task_t;
    //ms: > 0 (just after ms excute once),0:(excute now),<0(period ms excute);
    void		shedule(timer_task_t task, int ms);

protected:
	App(const char * version = __DATE__);
	virtual ~App();
protected:
	AppImpl * impl_{ nullptr};
};

};


