#pragma once
//couroutine implementation from cloudwu [https://github.com/cloudwu/coroutine.git]
enum CoroutineState {
	COROUTINE_STATE_DEAD = 0,
	COROUTINE_STATE_READY = 1,
	COROUTINE_STATE_RUNNING = 2,
	COROUTINE_STATE_SUSPEND = 3
};
struct CoroutineSchedulerImpl;
struct CoroutineScheduler {
	//coroutine func
	typedef void(*coroutine)(void *ud, CoroutineScheduler *);
	//coroutine id >= 0
	int				spawn(coroutine func, void * ud, const char * name = nullptr);
	int				start(coroutine func, void * ud, const char * name = nullptr);
	void			yield();
	void			resume(int id);
	int				status(int id);
	int				running();
	const char *	backtrace();
	int				pending(int * cos = nullptr);
	const char *	name(int co);
public:
	static CoroutineScheduler * default_scheduler();
	CoroutineScheduler(int init_co_num = 16);
	~CoroutineScheduler();
	CoroutineSchedulerImpl * impl;
};