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
	CoroutineScheduler::coroutine func;
	void * ud;
	ucontext_t ctx;
	struct CoroutineSchedulerImpl * sch;
	ptrdiff_t cap;
	ptrdiff_t size;
	CoroutineState status;
	char *stack;
	char name[16];
	int  from;
};
#define MAIN_STACK_SIZE (1024*1024*4)
#define MAX_CO_CALL_DEPTH (64)
#define MAX_CO_BT_BUFFER_SIZE	(MAX_CO_CALL_DEPTH*16)
struct CoroutineSchedulerImpl {
	char stack[MAIN_STACK_SIZE];
	int nco;
	int cap;
	int running;
	struct Coroutine main;
	struct Coroutine ** cos;
	char bts[MAX_CO_BT_BUFFER_SIZE];
};
static inline void _co_init(Coroutine * co, int id, const char * name, int from) {
	if (name) {
		snprintf(co->name, sizeof(co->name), "%d(%s)", id, name);
	}
	else {
		snprintf(co->name, sizeof(co->name), "%d", id);
	}
	co->from = from;
}
static inline Coroutine * _co_new(struct CoroutineSchedulerImpl *S, CoroutineScheduler::coroutine func, void *ud) {
	Coroutine * co = (Coroutine*)malloc(sizeof(*co));
	co->func = func;
	co->ud = ud;
	co->sch = S;
	co->cap = 0;
	co->size = 0;
	co->status = COROUTINE_STATE_READY;
	co->stack = NULL;
	co->name[0] = 0;
	co->from = -1;
	return co;
}
static inline void _co_delete(Coroutine *co) {
	co->status = COROUTINE_STATE_DEAD;
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
	impl->cap = nmax;
	impl->running = -1;
	impl->cos = (Coroutine**)malloc(sizeof(Coroutine *) * impl->cap);
	memset(impl->cos, 0, sizeof(Coroutine*) * impl->cap);
	//////////////////////////////////////////////////////////////////
	impl->main.cap = MAIN_STACK_SIZE;
	impl->main.func = nullptr;
	impl->main.sch = impl;
	impl->main.stack = &impl->stack[0];
	impl->main.size = MAIN_STACK_SIZE;
	impl->main.status = COROUTINE_STATE_RUNNING;
	impl->main.ud = nullptr;
	_co_init(&impl->main, -1, "main", -1);
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

int CoroutineScheduler::status(int id) {
	assert(id >= 0 && id < impl->cap);
	if (impl->cos[id] == NULL) {
		return COROUTINE_STATE_DEAD;
	}
	return impl->cos[id]->status;
}
static inline void _save_stack(Coroutine *C, char *top) {
	if (&(C->sch->main) == C) {
		return;
	}
	char dummy = 0; //current stack top
	assert(top - &dummy <= MAIN_STACK_SIZE);
	if (C->cap < top - &dummy) {
		free(C->stack);
		C->cap = top - &dummy;
		C->stack = (char *)malloc(C->cap);
	}
	C->size = top - &dummy;
	memcpy(C->stack, &dummy, C->size);
}
static inline void _co_switch(CoroutineScheduler * cs, int tid, bool active) {
	CoroutineSchedulerImpl * impl = cs->impl;
	int status = 0;
	uintptr_t ptr = (uintptr_t)cs;
	Coroutine * ctar = nullptr, * ccur = nullptr;
	if (impl->running < 0) {
		ccur = &impl->main;
	}
	else {
		ccur = impl->cos[impl->running];
	}
	if (tid == -1) {
		assert("tid is less than 0 , must not be active " && active == false);
		if (ccur->from >= 0 ) {
			assert(impl->cos[ccur->from] && impl->cos[ccur->from]->status == COROUTINE_STATE_SUSPEND);
			ctar = impl->cos[ccur->from];
			tid = ccur->from;
		}
		else {
			ctar = &impl->main;
		}
	}
	else {
		ctar = impl->cos[tid];
	}
	///////////////////////////////////////////////////////////////////////
	assert("current stack has been overflowed !" && (char *)&ccur > impl->stack);
	assert("target stack has been overflowed !" && (char *)&ctar > impl->stack);
	assert("current coroutine is same with target !" &&  ctar != ccur);
	//save current stack
	_save_stack(ccur, impl->stack + MAIN_STACK_SIZE);
	ccur->status = COROUTINE_STATE_SUSPEND;
	status = ctar->status;
	if (active) {
		ctar->from = impl->running;
	}
	impl->running = tid;
	ctar->status = COROUTINE_STATE_RUNNING;
	//////////////////////////////////////////////////////////////////////////
	//switch (may be new) resume
	ptr = (uintptr_t)cs;
	switch (status) {
	case COROUTINE_STATE_READY:
		getcontext(&ctar->ctx); //initialize the context
		ctar->ctx.uc_stack.ss_sp = impl->stack; //set the running stack
		ctar->ctx.uc_stack.ss_size = MAIN_STACK_SIZE;
		//ctar->ctx.uc_link = 0;
		ctar->ctx.uc_link = &ccur->ctx; // from caller , return to caller
										//set start entry point
		makecontext(&ctar->ctx, (void(*)(void)) _mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
		//switch running , save current context (main) to curC, then start tarC 
		swapcontext(&ccur->ctx, &ctar->ctx);
		break;
	case COROUTINE_STATE_SUSPEND:
		//resume running , restore stack
		memcpy(impl->stack + MAIN_STACK_SIZE - ctar->size, ctar->stack, ctar->size);
		swapcontext(&ccur->ctx, &ctar->ctx);
		break;
	default:
		assert("error coroutine state " && false);
	}
}
void CoroutineScheduler::resume(int id) { 
	assert(id >= 0);
	_co_switch(this, id, true);
}
void CoroutineScheduler::yield() { 
	assert(impl->running >= 0 );
	_co_switch(this, -1, false);
}
int CoroutineScheduler::start(coroutine func, void * ud, const char * name) { 
	int coid = spawn(func, ud, name);
	resume(coid);
	if (this->status(coid) != COROUTINE_STATE_DEAD) {
		return coid;
	}
	return -1;
}
int CoroutineScheduler::spawn(coroutine func, void *ud, const char * name) {
	Coroutine *co = _co_new(impl, func, ud);
	if (impl->nco >= impl->cap) {
		int id = impl->cap;
		impl->cos = (Coroutine**)realloc(impl->cos, impl->cap * 2 * sizeof(Coroutine *));
		memset(impl->cos + impl->cap, 0, sizeof(Coroutine *) * impl->cap);
		impl->cos[impl->cap] = co;
		impl->cap *= 2;
		++impl->nco;
		_co_init(co, id, name, impl->running);
		return id;
	}
	else {
		for (int i = 0;i < impl->cap;i++) {
			int id = (i + impl->nco) % impl->cap;
			if (impl->cos[id] == NULL) {
				impl->cos[id] = co;
				++impl->nco;
				_co_init(co, id, name, impl->running);
				return id;
			}
		}
	}
	assert("has no more memory to allocate coroutine !");
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
		if (impl->cos[i] && impl->cos[i]->status == COROUTINE_STATE_SUSPEND && impl->cos[i]->from == -1) {
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
