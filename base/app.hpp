#pragma  once

//a general app framwork
namespace dcsutil {
class App {
public:
	App(int argc, char * argv);
	~App();
public:
	virtual void on_init(void * config);//once
    virtual void on_stop();//once
    virtual void on_exit();//once
    virtual void on_loop();//tick
    virtual void on_idle();
    virtual void on_reload();
    virtual void on_recover();
    virtual const char * on_contrl(const char * cmdline);
public:
    typedef std::function<void()>   timer_task_t;
    //ms: > 0 (just after ms excute once),0:(excute now),<0(period ms excute);
    void shedule(timer_task_t task, int ms);
    int run();
    int stop();
private:
	void * app;
};
}


