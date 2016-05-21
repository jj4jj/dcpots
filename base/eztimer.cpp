#include "eztimer.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(__APPLE__)
#include <sys/time.h>
#endif

//the file core (timer) is grabed from cloudwu - skynet

//cas
#define LOCK(q) while (__sync_lock_test_and_set(&(q)->lock,1)) {}
#define UNLOCK(q) __sync_lock_release(&(q)->lock);

#define TIME_NEAR_SHIFT 8
#define TIME_NEAR (1 << TIME_NEAR_SHIFT)



#define TIME_LEVEL_SHIFT 6
#define TIME_LEVEL (1 << TIME_LEVEL_SHIFT)
#define TIME_NEAR_MASK (TIME_NEAR-1)
#define TIME_LEVEL_MASK (TIME_LEVEL-1)

struct timer_event {
    uint32_t    ud;
    uint16_t    sz;
    uint32_t    period;
    char        data[1];
};

struct timer_node {
	struct timer_node *next;
	uint32_t    expire;
};

struct link_list {
	struct timer_node head;
	struct timer_node *tail;
};

struct timer {
	//level 1
	struct link_list near[TIME_NEAR];
	//level 2-5
	struct link_list t[4][TIME_LEVEL];
	int lock;
	uint32_t time;
	uint32_t current;
	uint32_t starttime;
	uint64_t current_point;
	uint64_t origin_point;
    eztimer_dispatcher  dispather;
    char     last_error_msg[256];
};

#define MM_ALLOC(sz)    malloc((sz))
#define MM_FREE(p)      free((p))
#if defined(ENABLE_DEBUG)
    #define debug_print(ARGS...)    fprintf(stderr,##ARGS);
#else
    #define debug_print(ARGS...)    (void)
#endif



static struct timer * TI = NULL;

static inline struct timer_node * link_clear(struct link_list *list) {
	struct timer_node * ret = list->head.next;
	list->head.next = NULL;
	list->tail = &(list->head);

	return ret;
}

static inline void link(struct link_list *list,struct timer_node *node) {
	list->tail->next = node;
	list->tail = node;
	node->next= NULL;
}

static inline int unlink(struct link_list *list,struct timer_node *node) {
    //todo change to double list
    struct timer_node * prev = &(list->head), * found = list->head.next;
    while(found && found != node)
    {
        prev = found;
        found = found->next;
    }
    if(found == node)
    {                    
        prev->next = node->next;
        if(found == list->tail)
        {
            list->tail = prev;            
        }
        MM_FREE(node);
        return E_EZTMR_OK;
    }  
    return E_EZTMR_NOT_FOUND_ID;
}


static void add_node(struct timer *T,struct timer_node *node) {
	uint32_t time=node->expire;
	uint32_t current_time=T->time;
	//level 1
	if ((time|TIME_NEAR_MASK)==(current_time|TIME_NEAR_MASK)) {
		link(&T->near[time&TIME_NEAR_MASK],node);
	} else {	    
		int i;
		uint32_t mask=TIME_NEAR << TIME_LEVEL_SHIFT;
		for (i=0;i<3;i++) {
            //get level
			if ((time|(mask-1))==(current_time|(mask-1))) {
				break;
			}
			mask <<= TIME_LEVEL_SHIFT;
		}
        //link to level
		link(&T->t[i][((time>>(TIME_NEAR_SHIFT + i*TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK)],node);	
	}
}

static int remove_node(struct timer *T,struct timer_node *node) {
	uint32_t time=node->expire;
	uint32_t current_time=T->time;
	//level 1
	if ((time|TIME_NEAR_MASK)==(current_time|TIME_NEAR_MASK)) {
		return unlink(&T->near[time&TIME_NEAR_MASK],node);
	} else {	    
		int i;
		uint32_t mask=TIME_NEAR << TIME_LEVEL_SHIFT;
		for (i=0;i<3;i++) {
            //get level
			if ((time|(mask-1))==(current_time|(mask-1))) {
				break;
			}
			mask <<= TIME_LEVEL_SHIFT;
		}
		return unlink(&T->t[i][((time>>(TIME_NEAR_SHIFT + i*TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK)],node);	
	}
}

static struct timer_node * timer_add(struct timer *T, uint32_t ud,const void * cb,int sz,int time) {
    
	struct timer_node *node = (struct timer_node *)MM_ALLOC(sizeof(*node)+ sizeof(timer_event) + sz);
    timer_event * ev = (timer_event*)(node+1);    
    ev->ud = ud;
    ev->sz = sz;
    ev->period = (time < 0)?-time:0;
    time = (time < 0 )? -time : time;
	memcpy(ev->data, cb, sz);
    ev->data[sz] = 0;//dump ez

	LOCK(T);
		node->expire=time+T->time;
		add_node(T,node);
	UNLOCK(T);
    return node;
}

static void move_list(struct timer *T, int level, int idx) {
	struct timer_node *current = link_clear(&T->t[level][idx]);
	while (current) {
		struct timer_node *temp=current->next;
		add_node(T,current);
		current=temp;
	}
}

static void timer_shift(struct timer *T) {
	int mask = TIME_NEAR;
	uint32_t ct = ++T->time;
	if (ct == 0) {
		move_list(T, 3, 0);
	} else {
		uint32_t time = ct >> TIME_NEAR_SHIFT;
		int i=0;

		while ((ct & (mask-1))==0) {
			int idx=time & TIME_LEVEL_MASK;
			if (idx!=0) {
				move_list(T, i, idx);
				break;				
			}
			mask <<= TIME_LEVEL_SHIFT;
			time >>= TIME_LEVEL_SHIFT;
			++i;
		}
	}
}


static inline void timer_execute(struct timer *T) {
	int idx = T->time & TIME_NEAR_MASK;
	
	while (T->near[idx].head.next) {
		struct timer_node *current = link_clear(&T->near[idx]);
		UNLOCK(T);

		// dispatch_list don't need lock T
    	do {
    		struct timer_event * event = (struct timer_event *)(current+1);
            T->dispather(event->ud, event->data, event->sz);
    		struct timer_node * temp = current;
    		current=current->next;
            if(event->period > 0)
            {
                //update expired
                temp->next = NULL;
                temp->expire = TI->time + event->period;
                add_node(TI, temp );
            }
            else
            {
    		    MM_FREE(temp);
            }
    	} while (current);

		LOCK(T);
	}
}

static void timer_update_tick(struct timer *T) {
	LOCK(T);

	// try to dispatch timeout 0 (rare condition)
	timer_execute(T);

	// shift time first, and then dispatch timer message
	timer_shift(T);

    //check agin
	timer_execute(T);

	UNLOCK(T);
}

static struct timer * timer_create_timer() 
{
	struct timer *r=(struct timer *)MM_ALLOC(sizeof(struct timer));
	memset(r,0,sizeof(*r));

	int i,j;

	for (i=0;i<TIME_NEAR;i++) {
		link_clear(&r->near[i]);
	}

	for (i=0;i<4;i++) {
		for (j=0;j<TIME_LEVEL;j++) {
			link_clear(&r->t[i][j]);
		}
	}

	r->lock = 0;
	r->current = 0;

	return r;
}

eztimer_id_t            eztimer_run_after(int time, uint32_t ud,const void * cb, int sz){
    //tick
    if(time < 0)
    {
        return E_EZTMR_TIME_INVALID;
    }
    time /= 10;
	if (time == 0) {
        TI->dispather(ud, cb, sz);
	} else {
		return (eztimer_id_t)(timer_add(TI, ud, cb, sz, time));
	}
}
eztimer_id_t            eztimer_run_every(int time, uint32_t ud,const void * cb, int sz){
    if(time < 0)
    {
        return E_EZTMR_TIME_INVALID;
    }
    //tick
    time /= 10;
	if (time == 0) {
        TI->dispather(ud, cb, sz);
        return eztimer_run_after(1,ud,cb,sz);
	} else {
		return (eztimer_id_t)(timer_add(TI, ud, cb, sz, -time));
	}
}
int						eztimer_cancel(eztimer_id_t tid)
{
    return remove_node(TI, (struct timer_node *)tid);
}

// centisecond: 1/100 second    (10ms)
static void
systime(uint32_t *sec, uint32_t *cs) {
#if !defined(__APPLE__)
	struct timespec ti;
	clock_gettime(CLOCK_REALTIME, &ti);
	*sec = (uint32_t)ti.tv_sec;
	*cs = (uint32_t)(ti.tv_nsec / 10000000);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*sec = tv.tv_sec;
	*cs = tv.tv_usec / 10000;
#endif
}

static uint64_t
gettime() {
	uint64_t t;
#if !defined(__APPLE__)

#ifdef CLOCK_MONOTONIC_RAW
#define CLOCK_TIMER CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TIMER CLOCK_MONOTONIC
#endif

	struct timespec ti;
	clock_gettime(CLOCK_TIMER, &ti);
	t = (uint64_t)ti.tv_sec * 100;
	t += ti.tv_nsec / 10000000;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = (uint64_t)tv.tv_sec * 100;
	t += tv.tv_usec / 10000;
#endif
	return t;
}


#define _timer_set_error_msg(tmr, ARGS...)   do{\
    snprintf((tmr)->last_error_msg,sizeof((tmr)->last_error_msg),##ARGS);\
}while(0)


int eztimer_update() 
{
	uint64_t cp = gettime();
	if(cp < TI->current_point) {
		_timer_set_error_msg(TI,"time diff error: change from %lu to %lu",
            cp, TI->current_point);
		TI->current_point = cp;
        return E_EZTMR_TIME_BACK;
	} else if (cp != TI->current_point) {
		uint32_t diff = (uint32_t)(cp - TI->current_point);
		TI->current_point = cp;//?

		uint32_t oc = TI->current;
		TI->current += diff;
		if (TI->current < oc) {
			// when cs > 0xffffffff(about 497 days), time rewind
			TI->starttime += 0xffffffff / 100;
		}
		int i;
		for (i=0;i<diff;i++) {
			timer_update_tick(TI);
		}
	}
    return 0;
}
    
int eztimer_init(void) {
    if(TI)
    {
        return E_EZTMR_ALRADY_INITED;
    }
	TI = timer_create_timer();
	systime(&TI->starttime, &TI->current);
	uint64_t point = gettime();
	TI->current_point = point;
	TI->origin_point = point;
    TI->dispather = NULL;
    return 0;
}
int				   		eztimer_destroy()
{
    //do nothing
    return 0;
}
void					eztimer_set_dispatcher(eztimer_dispatcher dispatcher)
{
    TI->dispather = dispatcher;
}

const char *            eztimer_get_last_error()
{
    return TI->last_error_msg;
}
uint64_t                eztimer_ms()
{
    return TI->current_point*10;
}
