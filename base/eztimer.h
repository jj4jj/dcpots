#pragma once
#include <stdint.h>
#include <time.h>

enum eztimer_error_code
{
    E_EZTMR_UD_SIZE = -1,
    E_EZTMR_OK = 0,
    E_EZTMR_ALRADY_INITED = 1,
    E_EZTMR_TIME_BACK =2,
    E_EZTMR_NOT_FOUND_ID =3,
    E_EZTMR_TIME_INVALID = 4,
};

typedef		uint64_t    eztimer_id_t;
typedef 	int			(*eztimer_dispatcher)(uint32_t ud, const void * cb, int sz);
int				 		eztimer_init();
int				   		eztimer_destroy();
void					eztimer_set_dispatcher(eztimer_dispatcher dispatcher);
eztimer_id_t			eztimer_run_after(int ms, uint32_t ud,const void * cb, int sz);
eztimer_id_t			eztimer_run_every(int ms, uint32_t ud,const void * cb, int sz);
int						eztimer_cancel(eztimer_id_t tid);
int 					eztimer_update();
uint64_t                eztimer_ms();

const char *            eztimer_get_last_error();

int						eztimer_recover(const char * buffer, int sz);
int						eztimer_save(char * buffer, int * in_out_sz);

