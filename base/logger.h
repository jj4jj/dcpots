#pragma once
#include "utility.hpp"
struct logger_t;

enum log_msg_level_type {
	LOG_LVL_PROF = 0,
	LOG_LVL_TRACE = 1,
	LOG_LVL_DEBUG = 2,
	LOG_LVL_INFO = 3,
	LOG_LVL_WARNING = 4,
	LOG_LVL_ERROR = 5,
	LOG_LVL_FATAL = 6,
};
static const char*	STR_LOG_LEVEL(int lv){
	static const char * s_strlv[] = {
		"PROF", "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
	};
	if (lv <= 0 || lv >= (int)(sizeof(s_strlv) / sizeof(s_strlv[0]))){
		lv = 0;
	}
	return s_strlv[lv];
}

struct logger_config_t{
	string	dir;
	string	pattern;
	int		max_roll;
	int		max_msg_size;
	int		max_file_size;
	log_msg_level_type	lv;
	logger_config_t() :max_roll(20),
		max_msg_size(1024 * 1024), max_file_size(1024*1024*10){
		lv = LOG_LVL_TRACE;
	}
};

int				global_logger_init(const logger_config_t & conf);
void			global_logger_destroy();

logger_t *		logger_create(const logger_config_t & conf);
void			logger_destroy(logger_t *);
void			logger_set_level(logger_t *, log_msg_level_type level);
int				logger_level(logger_t * logger = nullptr);
//last msg
const char*		logger_msg(logger_t * logger = nullptr);
//last err
int				logger_errno(logger_t * logger = nullptr);
//set last
int				logger_write(logger_t *, int err, const char* fmt, ...);

/////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOG_MSG_FORMAT_PREFIX	"%s.%lu:%d|%s:%d|%s|"
#define LOG_MSG_FORMAT_VALUES	dcsutil::strftime(_str_alloc_,err_tv_.tv_sec),err_tv_.tv_usec,getpid(),__FUNCTION__,__LINE__

#ifndef LOG_MSG
#define LOG_MSG(log_lv_, erm, killer, err_no, format, ...)	\
do{\
	if ((lv) >= logger_level((erm)))\
	{\
		timeval err_tv_; gettimeofday(&err_tv_, NULL); std::string _str_alloc_; \
		fprintf(stderr, LOG_MSG_FORMAT_PREFIX format "\n", LOG_MSG_FORMAT_VALUES, STR_LOG_LEVEL((log_lv_)), ##__VA_ARGS__); \
	}\
} while (0)
#endif
	//			error_write((erm), (err_no), (killer), "(%lu.%lu:%d|%s:%d)" fmt "\n", tv.tv_sec, tv.tv_usec, getpid(),__FUNCTION__, __LINE__, ##__VA_ARGS__);

#ifndef LOGR
#define RAW_LOG_MSG_FORMAT_PREFIX	"%lu.%lu:%d|%s|"
#define RAW_LOG_MSG_FORMAT_VALUES	err_tv_.tv_sec,err_tv_.tv_usec,getpid()
#define LOGR(log_lv_, format,...)	\
do{\
	if ((log_lv_) >= logger_level()){\
			timeval err_tv_; gettimeofday(&err_tv_, NULL); std::string _str_alloc_; \
			fprintf(stderr, RAW_LOG_MSG_FORMAT_PREFIX format "\n", RAW_LOG_MSG_FORMAT_VALUES, STR_LOG_LEVEL((log_lv_)), ##__VA_ARGS__); \
	}\
} while (0)
#endif

#ifndef LOGP
#define LOGP(format,...)	\
do{\
	const int log_lv_ = LOG_LVL_DEBUG; \
	if ((log_lv_) >= logger_level()){\
		timeval err_tv_; gettimeofday(&err_tv_, NULL); std::string _str_alloc_; \
		fprintf(stderr, LOG_MSG_FORMAT_PREFIX format "\n", LOG_MSG_FORMAT_VALUES, STR_LOG_LEVEL((log_lv_)), ##__VA_ARGS__); \
	}\
} while (0)
#endif

#ifndef GLOG
#define GLOG(log_lv_, err, format, ...)	do{\
	if ((log_lv_) >= logger_level()){\
		timeval err_tv_; gettimeofday(&err_tv_, NULL); std::string _str_alloc_; \
		logger_write(nullptr, (err), LOG_MSG_FORMAT_PREFIX format "\n", LOG_MSG_FORMAT_VALUES, STR_LOG_LEVEL((log_lv_)), ##__VA_ARGS__); \
	}\
} while (0)
#endif

