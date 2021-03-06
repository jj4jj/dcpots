#pragma once
#include "dcutils.hpp"

struct logger_t;
enum log_msg_level_type {
	LOG_LVL_PROF = 0,
	LOG_LVL_TRACE = 1,
	LOG_LVL_DEBUG = 2,
	LOG_LVL_INFO = 3,
	LOG_LVL_WARNING = 4,
	LOG_LVL_ERROR = 5,
	LOG_LVL_FATAL = 6,
    LOG_LVL_NUM = 7,
	LOG_LVL_INVALID = 255
};
static const char * s_msg_level_strlv[] = {
    "PROF", "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static inline const char *	STR_LOG_LEVEL(int lv){
    if (lv < 0 || lv >= (int)(sizeof(s_msg_level_strlv) / sizeof(s_msg_level_strlv[0]))){
		return "";
	}
    return s_msg_level_strlv[lv];
}
static inline log_msg_level_type INT_LOG_LEVEL(const char * lv){
    for (int i = 0; i < LOG_LVL_NUM; ++i){
        if (strcasecmp(STR_LOG_LEVEL(i), lv) == 0){
            return (log_msg_level_type)i;
        }
    }
    return LOG_LVL_INVALID;
}

struct logger_config_t{
	string	path;
	int		max_roll;
	int		max_msg_size;
	int		max_file_size;
    log_msg_level_type	lv;
	logger_config_t();
};

int				default_logger_init(const logger_config_t & conf);
void			default_logger_destroy();
logger_t *		default_logger();
///////////////////////////////////////////////////////////////////////
void            logger_lock(logger_t * logger = nullptr);
bool            logger_try_lock(logger_t * logger = nullptr);
void            logger_unlock(logger_t * logger = nullptr);
///////////////////////////////////////////////////////////////////////
logger_t *		logger_create(const logger_config_t & conf);
void			logger_destroy(logger_t *);
void			logger_set_level(logger_t *, log_msg_level_type level);
int				logger_level(logger_t * logger = nullptr);
const char*		logger_msg(logger_t * logger = nullptr);
int				logger_errno(logger_t * logger = nullptr);
int				logger_write(logger_t *, int loglv, int sys_err_, const char* fmt, ...);

/////////////////////////////////////////////////////////////////////////////////////////////////////////


//raw log
#ifndef LOGR
#define RAW_LOG_MSG_FORMAT_PREFIX	"%s.%d|%s|"
#define RAW_LOG_MSG_FORMAT_VALUES(tag)	_s_time_alloc_,err_tv_.tv_usec,(tag)

#define LOGR(log_lv_, format,...)	do{\
    if ((log_lv_) >= logger_level((nullptr))){\
        timeval err_tv_; gettimeofday(&err_tv_, NULL); err_tv_.tv_sec += 28800;\
        char _s_time_alloc_[40]; struct tm _tmp_sftm_;gmtime_r(&err_tv_.tv_sec, &_tmp_sftm_); \
        strftime(_s_time_alloc_, sizeof(_s_time_alloc_), "%Y-%m-%dT%H:%M:%S", &_tmp_sftm_); \
        logger_write((nullptr), (log_lv_), 0, RAW_LOG_MSG_FORMAT_PREFIX format "\n", RAW_LOG_MSG_FORMAT_VALUES(STR_LOG_LEVEL((log_lv_))), ##__VA_ARGS__); \
    }\
} while (0)
#endif


//log to str
#ifndef LOGRSTR
#define LOGRSTR(str, tag, format,...)	do{\
    timeval err_tv_; gettimeofday(&err_tv_, NULL); err_tv_.tv_sec += 28800;\
    char _s_time_alloc_[40]; struct tm _tmp_sftm_;gmtime_r(&err_tv_.tv_sec, &_tmp_sftm_); \
    strftime(_s_time_alloc_, sizeof(_s_time_alloc_), "%Y-%m-%dT%H:%M:%S", &_tmp_sftm_); \
    ::dcs::strprintf((str), RAW_LOG_MSG_FORMAT_PREFIX format, RAW_LOG_MSG_FORMAT_VALUES(tag), ##__VA_ARGS__); \
} while (0)
#endif

//general log prefix and values
//2010-10-12T12:08:08.123456|TID@PID|DEBUG|FUNC:LINE|
#define LOG_MSG_FORMAT_PREFIX	"%s.%lu|%ld@%d|%s|%s:%d|%s|"
#define LOG_MSG_FORMAT_VALUES(tag)	_s_time_alloc_,err_tv_.tv_usec,gettid(),getpid(),(tag),basename(__FILE__),__LINE__,__FUNCTION__

//log to str
#ifndef LOGSTR
#define LOGSTR(str, tag, format,...)    do{\
    timeval err_tv_; gettimeofday(&err_tv_, NULL); err_tv_.tv_sec += 28800;\
    char _s_time_alloc_[40]; struct tm _tmp_sftm_;gmtime_r(&err_tv_.tv_sec, &_tmp_sftm_); \
    strftime(_s_time_alloc_, sizeof(_s_time_alloc_), "%Y-%m-%dT%H:%M:%S", &_tmp_sftm_); \
    ::dcs::strprintf((str), LOG_MSG_FORMAT_PREFIX format, LOG_MSG_FORMAT_VALUES(tag), ##__VA_ARGS__); \
} while (0)
#endif


//global logge
#ifndef LOG
#define LOG(logger_, log_lv_, sys_err_, format_, ...)	do{\
    if ((log_lv_) >= logger_level((logger_))){\
		timeval err_tv_; gettimeofday(&err_tv_, NULL); err_tv_.tv_sec += 28800;\
		char _s_time_alloc_[40]; struct tm _tmp_sftm_;gmtime_r(&err_tv_.tv_sec, &_tmp_sftm_); \
		strftime(_s_time_alloc_, sizeof(_s_time_alloc_), "%Y-%m-%dT%H:%M:%S", &_tmp_sftm_); \
        logger_write((logger_), (log_lv_), (sys_err_), LOG_MSG_FORMAT_PREFIX format_ "\n", LOG_MSG_FORMAT_VALUES(STR_LOG_LEVEL(log_lv_)), ##__VA_ARGS__); \
    }\
}while (0)
#endif
////////////////////////////////////////////////////////////////////////////////////////////////

#define LOG_TRA(logger_, format_, ...)		LOG(logger_, LOG_LVL_TRACE, 0, format_, ##__VA_ARGS__)
#define LOG_DBG(logger_, format_, ...)		LOG(logger_, LOG_LVL_DEBUG, 0, format_, ##__VA_ARGS__)
#define LOG_IFO(logger_, format_, ...)		LOG(logger_, LOG_LVL_INFO, 0, format_, ##__VA_ARGS__)
#define LOG_WAR(logger_, format_, ...)		LOG(logger_, LOG_LVL_WARNING, 0, format_, ##__VA_ARGS__)
#define LOG_ERR(logger_, format_, ...)		LOG(logger_, LOG_LVL_ERROR, 0, format_, ##__VA_ARGS__)
#define LOG_FTL(logger_, format_, ...)		LOG(logger_, LOG_LVL_FATAL, 0, format_, ##__VA_ARGS__)
#define LOG_SER(logger_, format_, ...)		LOG(logger_, LOG_LVL_ERROR, 1, format_, ##__VA_ARGS__)
#define LOG_SFT(logger_, format_, ...)		LOG(logger_, LOG_LVL_FATAL, 1, format_, ##__VA_ARGS__)



//global logger
#ifndef GLOG
#define GLOG(log_lv_, sys_err_, format_, ...)	LOG(nullptr, log_lv_, sys_err_, format_, ##__VA_ARGS__)
#endif
//////////////////////////////////////////////////////////////////////////////////

#ifndef BLOCK_RUN
#define BLOCK_RUN

#define BLOCK_RUN_BEG(logger_, log_lv_)  if ((log_lv_) >= logger_level((logger_))){
#define BLOCK_RUN_END() }

#define RUN_BLOCK_END()     BLOCK_RUN_END()
#define RUN_BLOCK_TRA()     BLOCK_RUN_BEG(nullptr, LOG_LVL_TRACE)
#define RUN_BLOCK_DBG()     BLOCK_RUN_BEG(nullptr, LOG_LVL_DEBUG)
#define RUN_BLOCK_INFO()    BLOCK_RUN_BEG(nullptr, LOG_LVL_INFO)
#define RUN_BLOCK_WAR()     BLOCK_RUN_BEG(nullptr, LOG_LVL_WARNING)
#define RUN_BLOCK_ERR()     BLOCK_RUN_BEG(nullptr, LOG_LVL_ERROR)
#define RUN_BLOCK_FTL()     BLOCK_RUN_BEG(nullptr, LOG_LVL_FATAL)



#endif


#define GLOG_TRA(format_, ...)		GLOG(LOG_LVL_TRACE, 0, format_, ##__VA_ARGS__)
#define GLOG_DBG(format_, ...)		GLOG(LOG_LVL_DEBUG, 0, format_, ##__VA_ARGS__)
#define GLOG_IFO(format_, ...)		GLOG(LOG_LVL_INFO, 0, format_, ##__VA_ARGS__)
#define GLOG_WAR(format_, ...)		GLOG(LOG_LVL_WARNING, 0, format_, ##__VA_ARGS__)
#define GLOG_SWR(format_, ...)		GLOG(LOG_LVL_WARNING, 1, format_, ##__VA_ARGS__)
#define GLOG_ERR(format_, ...)		GLOG(LOG_LVL_ERROR, 0, format_, ##__VA_ARGS__)
#define GLOG_SER(format_, ...)		GLOG(LOG_LVL_ERROR, 1, format_, ##__VA_ARGS__)
#define GLOG_FTL(format_, ...)		GLOG(LOG_LVL_FATAL, 0, format_, ##__VA_ARGS__)
#define GLOG_SFT(format_, ...)		GLOG(LOG_LVL_FATAL, 1, format_, ##__VA_ARGS__)




