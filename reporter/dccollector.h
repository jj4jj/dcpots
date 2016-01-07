#pragma once
#include "dcreport_collect.h"

struct collector_event_data {
    const char *    key;
    int             change;
    const char *    param;
};

typedef int(*collector_event_cb_t)(void * ud, const char * repoter_name, report_collect_msg_type type,const collector_event_data * data);

int         collector_init(const char * addr);
void        collector_set_event_cb(collector_event_cb_t cb, void * ud);
void        collector_destroy();
void        collector_update(int timeout_ms);
