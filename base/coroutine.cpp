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

struct Coroutine {
	CoroutineScheduler::coroutine_func func;
	void *ud;
	ucontext_t ctx;
	struct CoroutineSchedulerImpl * sch;
	ptrdiff_t cap;
	ptrdiff_t size;
	int status;
	char *stack;
};
#define MAIN_STACK_SIZE (1024*1024*4)
struct CoroutineSchedulerImpl {
	char stack[MAIN_STACK_SIZE];
	ucontext_t main;
	int nco;
	int cap;
	int running;
	struct Coroutine **co;
};
static inline Coroutine * _co_new(struct CoroutineSchedulerImpl *S, 
							CoroutineScheduler::coroutine_func func, void *ud) {
	Coroutine * co = (Coroutine*)malloc(sizeof(*co));
	co->func = func;
	co->ud = ud;
	co->sch = S;
	co->cap = 0;
	co->size = 0;
	co->status = COROUTINE_STATE_READY;
	co->stack = NULL;
	return co;
}
static inline void _co_delete(Coroutine *co) {
	if (co->stack) {
		free(co->stack);
		co->stack = nullptr;
	}
	free(co);
}
static void mainfunc(uint32_t low32, uint32_t hi32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	CoroutineScheduler * cs = (CoroutineScheduler *)ptr;
	CoroutineSchedulerImpl * S = cs->impl;
	int id = S->running;
	Coroutine *C = S->co[id];
	C->func(C->ud, cs);
	_co_delete(C);
	S->co[id] = NULL;
	--S->nco;
	S->running = -1;
}
CoroutineScheduler::CoroutineScheduler(int nmax) {
	impl = (CoroutineSchedulerImpl*)malloc(sizeof(*impl));
	impl->nco = 0;
	impl->cap = nmax;
	impl->running = -1;
	impl->co = (Coroutine**)malloc(sizeof(Coroutine *) * impl->cap);
	memset(impl->co, 0, sizeof(Coroutine*) * impl->cap);
}
CoroutineScheduler::~CoroutineScheduler() {
	if (impl) {
		for (int i = 0;i < impl->cap;i++) {
			Coroutine * co = impl->co[i];
			if (co) {
				_co_delete(co);
			}
		}
		free(impl->co);
		impl->co = nullptr;
		free(impl);
		impl = nullptr;
	}
}

int CoroutineScheduler::status(int id) {
	assert(id >= 0 && id < impl->cap);
	if (impl->co[id] == NULL) {
		return COROUTINE_STATE_DEAD;
	}
	return impl->co[id]->status;
}

int CoroutineScheduler::running() {
	return impl->running;
}

void CoroutineScheduler::resume(int id) {
	assert(impl->running == -1);
	assert(id >= 0 && id < impl->cap);
	Coroutine *C = impl->co[id];
	if (C == NULL)
		return;
	int status = C->status;
	uintptr_t ptr = (uintptr_t)this;
	switch (status) {
	case COROUTINE_STATE_READY:
		getcontext(&C->ctx);
		C->ctx.uc_stack.ss_sp = impl->stack;
		C->ctx.uc_stack.ss_size = MAIN_STACK_SIZE;
		C->ctx.uc_link = &impl->main;
		impl->running = id;
		C->status = COROUTINE_STATE_RUNNING;
		makecontext(&C->ctx, (void(*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
		swapcontext(&impl->main, &C->ctx);
		break;
	case COROUTINE_STATE_SUSPEND:
		memcpy(impl->stack + MAIN_STACK_SIZE - C->size, C->stack, C->size);
		impl->running = id;
		C->status = COROUTINE_STATE_RUNNING;
		swapcontext(&impl->main, &C->ctx);
		break;
	default:
		assert(0);
	}
}
static inline void _save_stack(Coroutine *C, char *top) {
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

void CoroutineScheduler::yield() {
	int id = impl->running;
	assert(id >= 0);
	Coroutine * C = impl->co[id];
	assert((char *)&C > impl->stack);
	_save_stack(C, impl->stack + MAIN_STACK_SIZE);
	C->status = COROUTINE_STATE_SUSPEND;
	impl->running = -1;
	swapcontext(&C->ctx, &impl->main);
}

int CoroutineScheduler::spawn(coroutine_func func, void *ud) {
	Coroutine *co = _co_new(impl, func, ud);
	if (impl->nco >= impl->cap) {
		int id = impl->cap;
		impl->co = (Coroutine**)realloc(impl->co, impl->cap * 2 * sizeof(Coroutine *));
		memset(impl->co + impl->cap, 0, sizeof(Coroutine *) * impl->cap);
		impl->co[impl->cap] = co;
		impl->cap *= 2;
		++impl->nco;
		return id;
	}
	else {
		for (int i = 0;i < impl->cap;i++) {
			int id = (i + impl->nco) % impl->cap;
			if (impl->co[id] == NULL) {
				impl->co[id] = co;
				++impl->nco;
				return id;
			}
		}
	}
	assert(0);
	return -1;
}
