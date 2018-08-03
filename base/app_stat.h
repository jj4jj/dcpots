#pragma once
#include <cstdint>
#include <string>

namespace dcs {

struct AppStatItem {
    uint64_t        total_time_ns{ 0 };
    uint32_t        max_time_ns{ 0 };
    uint32_t        min_time_ns{ 1000 * 1000 * 1000 };
    uint64_t        call_begin_time_ns{ 0 };
    uint64_t        ncall{ 0 };
    uint64_t        nproc{ 0 };    
public:
    void        clear();
    void        begin();
    int         end(int nproc_ = 0);
};

enum AppRunStatItemType {
    APP_RUN_STAT_ITEM_IDLE = 0,
    APP_RUN_STAT_ITEM_TICK,
    APP_RUN_STAT_ITEM_LOOP,
    //////////////////////////////
    APP_RUN_STAT_ITEM_MAX,
};
struct AppRunStat {
    uint64_t        start_time_us{ 0 };
    uint64_t        total_time_us{ 0 };
    uint32_t        next_update_time_s{ 0 };
    uint32_t        stat_update_period{ 60 };
    AppStatItem     item[APP_RUN_STAT_ITEM_MAX];
    std::string     strlog;

public:
    void            start();
    void            update(uint64_t time_now_us);
    const char *    log();
};

};