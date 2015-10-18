#pragma once
#include "stdinc.h"
struct logger_t;

enum log_msg_level_type {
	ERROR_LVL_TRACE = 1,
	ERROR_LVL_DEBUG = 2,
	ERROR_LVL_INFO = 3,
	ERROR_LVL_WARNING = 4,
	ERROR_LVL_ERROR = 5,
	ERROR_LVL_FATAL = 6,
};
struct logger_config_t{
	string	path;
	string	pattern;
	int		max_roll;
	int		max_msg_size;
	logger_config_t() :max_roll(20), max_msg_size(1024 * 1024){}
};

int				global_logger_init(const logger_config_t & conf);
void			global_logger_destroy();

logger_t *		logger_create(const logger_config_t & conf);
void			logger_destroy(logger_t *);
void			logger_set_level(logger_t *, int level);
int				logger_level(logger_t *);
//last msg
const char*		logger_msg(logger_t *);
//last err
int				logger_errno(logger_t *);
//set last
int				logger_write(logger_t *, int err, const char* fmt, ...);


#define LOG_MSG_FORMAT_PREFIX	"%lu.%lu:%d|%s:%d|"
#define LOG_MSG_FORMAT_VALUES	err_tv_.tv_sec,err_tv_.tv_usec,getpid(),__FUNCTION__,__LINE__
#ifndef LOG_MSG
#define LOG_MSG(lv, erm, killer, err_no, format, ...)	\
	do{\
		if ((lv) >= error_level((erm)))\
		{\
			timeval err_tv_; gettimeofday(&err_tv_, NULL); \
			fprintf(stderr, LOG_MSG_FORMAT_PREFIX format "\n", LOG_MSG_FORMAT_VALUES, ##__VA_ARGS__); \
		}\
	} while (0)
#endif
//			error_write((erm), (err_no), (killer), "(%lu.%lu:%d|%s:%d)" fmt "\n", tv.tv_sec, tv.tv_usec, getpid(),__FUNCTION__, __LINE__, ##__VA_ARGS__);

#ifndef LOGP
#define LOGP(format, ...)	\
do{\
	timeval err_tv_; gettimeofday(&err_tv_, NULL); \
	fprintf(stderr, LOG_MSG_FORMAT_PREFIX format "\n", LOG_MSG_FORMAT_VALUES, ##__VA_ARGS__); \
} while (0)
#endif

#ifndef LOGE
#define LOGE(err, format, ...)	do{\
	if ((lv) >= error_level((global_error()))){\
		timeval err_tv_; gettimeofday(&err_tv_, NULL); \
		logger_write(nullptr, (err), LOG_MSG_FORMAT_PREFIX format "\n", LOG_MSG_FORMAT_VALUES, ##__VA_ARGS__);\
		fprintf(stderr, ); \
	}\
} while (0)
#endif



