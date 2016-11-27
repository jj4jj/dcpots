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
	typedef void(*coroutine_func)(void *ud, CoroutineScheduler * _cs);
	int			spawn(coroutine_func func, void * ud);
	void		yield();
	void		resume(int id);
	int			status(int id);
	int			running();
	////////////////////////////////////////////////////////////////
public:
	CoroutineScheduler(int init_co_num = 16);
	~CoroutineScheduler();
	CoroutineSchedulerImpl * impl;
};