#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#if __APPLE__ && __MACH__
#include <sys/ucontext.h>
#else 
#include <ucontext.h>
#endif 
#include "coroutine.h"

//this implementation of coroutine is refer to cloudwu(coroutine)

struct Coroutine {
	CoroutineScheduler::coroutine	func;
	void *							ud;
	ucontext_t						ctx;
	struct CoroutineSchedulerImpl * sch;
	ptrdiff_t						cap;
	ptrdiff_t						size;
	char						   *stack;
	char							name[16];
	int								from;
};
//#define MAIN_STACK_SIZE (1024*1024*8)
#define SHARE_STACK_SIZE (1024*1024*4)
#define MAX_CO_CALL_DEPTH (64)
#define MAX_CO_BT_BUFFER_SIZE	(MAX_CO_CALL_DEPTH*16)
struct CoroutineSchedulerImpl {
	char		share_stack[SHARE_STACK_SIZE];
	ucontext_t	ctx;
	int			nco;
	int			cap;
	int			max;
	int			running;
	struct Coroutine ** cos;
	char bts[MAX_CO_BT_BUFFER_SIZE];
};
static inline void _co_init(Coroutine * co, int id, const char * name) {
	if (name) {
		snprintf(co->name, sizeof(co->name), "%d(%s)", id, name);
	}
	else {
		snprintf(co->name, sizeof(co->name), "%d", id);
	}
}
static inline Coroutine * _co_new(struct CoroutineSchedulerImpl *S, CoroutineScheduler::coroutine func, void *ud) {
	Coroutine * co = (Coroutine*)malloc(sizeof(*co));
	co->func = func;
	co->ud = ud;
	co->sch = S;
	co->cap = 0;
	co->size = 0;
	co->stack = NULL;
	co->name[0] = 0;
	co->from = -2;
	return co;
}
static inline void _co_delete(Coroutine *co) {
	//cache it ?
	if (co->stack) {
		free(co->stack);
		co->stack = nullptr;
	}
	free(co);
}
static void _mainfunc(uint32_t low32, uint32_t hi32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	CoroutineScheduler * cs = (CoroutineScheduler *)ptr;
	CoroutineSchedulerImpl * S = cs->impl;
	int id = S->running;
	Coroutine *C = S->cos[id];
	C->func(C->ud, cs);
	int from = C->from;
	_co_delete(C);//normal return
	S->cos[id] = NULL;
	--S->nco;	
	S->running = -1;
	//for makecontext auto return uplayer ?
	if (from >= 0) {
		cs->resume(from);
	}
}
CoroutineScheduler * CoroutineScheduler::default_scheduler() {
	static CoroutineScheduler * s_sch = nullptr;
	if(s_sch){return s_sch;}
	s_sch = new CoroutineScheduler();
	return s_sch;
}
CoroutineScheduler::CoroutineScheduler(int nmax) {
	impl = (CoroutineSchedulerImpl*)malloc(sizeof(*impl));
	impl->nco = 0;
	impl->cap = 16;
	impl->max = nmax;
	impl->running = -1;
	impl->cos = (Coroutine**)malloc(sizeof(Coroutine *) * impl->cap);
	memset(impl->cos, 0, sizeof(Coroutine*) * impl->cap);
}

CoroutineScheduler::~CoroutineScheduler() {
	if (impl) {
		for (int i = 0;i < impl->cap;i++) {
			Coroutine * co = impl->cos[i];
			if (co) {
				_co_delete(co);
			}
		}
		free(impl->cos);
		impl->cos = nullptr;
		free(impl);
		impl = nullptr;
	}
}

CoroutineState CoroutineScheduler::status(int id) {
	assert(id >= 0 && id < impl->cap);
	if (impl->cos[id] == NULL) {
		return COROUTINE_STATE_DEAD;
	}
	if (impl->running == id) {
		return COROUTINE_STATE_RUNNING;
	}
	if (impl->cos[id]->from == -2) {
		return COROUTINE_STATE_READY;
	}
	return COROUTINE_STATE_SUSPEND;
}
static inline ucontext_t * _save_stack(Coroutine *C, char *top) {
	char dummy = 0; //current stack top
	assert(top - &dummy <= SHARE_STACK_SIZE);
	if (C->cap < top - &dummy) {
		free(C->stack);
		C->cap = top - &dummy;
		C->stack = (char *)malloc(C->cap);
	}
	C->size = top - &dummy;
	memcpy(C->stack, &dummy, C->size);
	return &(C->ctx);
}
static inline void _co_switch(CoroutineScheduler * cs, int tid) {
	//1. all co share the stack, but save then when susspend
	CoroutineSchedulerImpl * impl = cs->impl;
	uintptr_t ptr = (uintptr_t)cs;
	Coroutine * co = nullptr;
	ucontext * _ctx_save = nullptr;
	if (impl->running >= 0) { //in a co
		_ctx_save = _save_stack(impl->cos[impl->running], impl->share_stack + SHARE_STACK_SIZE);
		if (tid >= 0) {
			//susppend current and switch to tid
			co = impl->cos[tid];
			if (co->from == -2) { //is ready state , need create 
				co->from = impl->running;
				impl->running = tid;
				getcontext(&co->ctx); //initialize the context
				co->ctx.uc_stack.ss_sp = impl->share_stack; //set the running stack
				co->ctx.uc_stack.ss_size = SHARE_STACK_SIZE;
				co->ctx.uc_link = _ctx_save;
				makecontext(&co->ctx, (void(*)(void)) _mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
				swapcontext(_ctx_save, &co->ctx);
			}
			else { //must be suspend , recover stack
				co->from = impl->running;
				impl->running = tid;
				memcpy(impl->share_stack + SHARE_STACK_SIZE - co->size, co->stack, co->size);
				swapcontext(_ctx_save, &co->ctx);
			}
		}
		else { //just yield
			if (impl->cos[impl->running]->from == -1) { //yield to main thread
				impl->running = -1;
				swapcontext(_ctx_save, &impl->ctx);
			}
			else {
				assert(impl->cos[impl->running]->from >= 0 && "must yield then resume to a valid co !");
				co = impl->cos[impl->cos[impl->running]->from];
				impl->running = impl->cos[impl->running]->from;
				memcpy(impl->share_stack + SHARE_STACK_SIZE - co->size, co->stack, co->size);
				swapcontext(_ctx_save, &co->ctx);
			}
		}
	}
	else { //in main thread
		assert(tid >= 0 && "main thread switch a co must be a valid !");
		co = impl->cos[tid];
		impl->running = tid;
		if (co->from == -2) { //is ready state , need create 
			co->from = -1;
			getcontext(&co->ctx); //initialize the context
			co->ctx.uc_stack.ss_sp = impl->share_stack; //set the running stack
			co->ctx.uc_stack.ss_size = SHARE_STACK_SIZE;
			co->ctx.uc_link = &(impl->ctx);
			makecontext(&co->ctx, (void(*)(void)) _mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
			//switch running , save current context (main) to curC, then start tarC 
			swapcontext(&(impl->ctx), &co->ctx);
		}
		else { //must be suspend , recover stack
			co->from = -1;
			memcpy(impl->share_stack + SHARE_STACK_SIZE - co->size, co->stack, co->size);
			swapcontext(&(impl->ctx), &co->ctx);
		}
	}
}
void CoroutineScheduler::resume(int id) { _co_switch(this, id); }
void CoroutineScheduler::yield() { _co_switch(this, -1);}
int CoroutineScheduler::start(coroutine func, void * ud, const char * name) { 
	int coid = spawn(func, ud, name);
	if (coid >= 0) { _co_switch(this, coid); return coid;}
	return -1;
}
int CoroutineScheduler::spawn(coroutine func, void *ud, const char * name) {
	Coroutine *co = _co_new(impl, func, ud);
	if (impl->nco >= impl->cap) {
		int id = impl->cap;
		if (impl->cap * 2 >= impl->max) {return -1;}
		impl->cos = (Coroutine**)realloc(impl->cos, impl->cap * 2 * sizeof(Coroutine *));
		memset(impl->cos + impl->cap, 0, sizeof(Coroutine *) * impl->cap);
		impl->cos[impl->cap] = co;
		impl->cap *= 2;
		++impl->nco;
		_co_init(co, id, name);
		return id;
	}
	else {
		for (int i = 0;i < impl->cap;i++) {
			int id = (i + impl->nco) % impl->cap;
			if (impl->cos[id] == NULL) {
				impl->cos[id] = co;
				++impl->nco;
				_co_init(co, id, name);
				return id;
			}
		}
	}
	assert(false);
	return -1;
}
//////////////////////////////////////////////////////////////////////////

int CoroutineScheduler::running() {
	return impl->running;
}
const char * CoroutineScheduler::backtrace() {
	int cid = impl->running;
	int len = 0;
	impl->bts[0] = 0;
	while (cid >= 0 && len < MAX_CO_BT_BUFFER_SIZE) {
		Coroutine * co = impl->cos[cid];
		len += snprintf(impl->bts + len, MAX_CO_BT_BUFFER_SIZE - len, "%s<-", co->name);
		cid = co->from;
	}
	if (len < MAX_CO_BT_BUFFER_SIZE) {
		len += snprintf(impl->bts + len, MAX_CO_BT_BUFFER_SIZE - len, "+|");
	}
	return impl->bts;
}
int	CoroutineScheduler::pending(int * cos) {
	int sum = 0;
	for (int i = 0;i < impl->cap; ++i) {
		if (impl->cos[i] && impl->cos[i]->from == -1) {
			if (cos) {
				cos[sum++] = i;
			}
			else {
				++sum;
			}
		}
	}
	return sum;
}
const char * CoroutineScheduler::name(int co) {
	if (co < impl->cap && impl->cos[co]) {
		return impl->cos[co]->name;
	}
	return nullptr;
}
