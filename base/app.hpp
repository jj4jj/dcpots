#pragma  once

//a general app framwork
namespace dcsutil {
class App {
public:
	App(int argc, char * argv);
	~App();
public:
	void on_init(int(*)(void * config));//once
	void on_stop(int(*)());//once
	void on_exit(void(*)());//once
	void on_loop(int(*)());//tick
	void on_idle(void(*)());
	void on_reload(int(*)());
	void on_recover(int(*)());
	void shedule(int aft_ms, void(*)(void *), void * arg = nullptr, int times = 1);
	void on_contrl(const char*(*)(const char * cmdline));
private:
	void * app;
};
}


