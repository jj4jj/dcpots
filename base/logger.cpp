#include "dclogfile.h"
#include "logger.h"
#include "dcdebug.h"

#define  THREAD_SAFE

struct logger_t {
	logger_config_t	conf;
#ifdef  THREAD_SAFE
    dcsutil::lock_mixin<true>   lock;
#else
    dcsutil::lock_mixin<false>  lock;
#endif //  THREAD_SAFE
	int			last_err;
	string		last_msg;
	
    dcsutil::logfile_t   logfile;
    dcsutil::logfile_t   errfile;

    logger_t() :last_err(0){
	}
};
#undef THREAD_SAFE

static	logger_t * G_LOGGER = nullptr;
logger_t *		default_logger(){
    if (!G_LOGGER){
        default_logger_init(logger_config_t());
    }
	return G_LOGGER;
}
int				default_logger_init(const logger_config_t & conf){
	if (G_LOGGER){
		default_logger_destroy();
		G_LOGGER = nullptr;
	}
	G_LOGGER = logger_create(conf);
	return G_LOGGER ? 0 : -1;
}
void			default_logger_destroy(){
	if (!G_LOGGER){
		return;
	}
	logger_destroy(G_LOGGER);
	G_LOGGER = nullptr;
}

logger_t *	logger_create(const logger_config_t & conf){
	logger_t * em = new logger_t();
	if (!em) return nullptr;
	em->last_msg.reserve(conf.max_msg_size);
	em->conf = conf;
	string filepath = conf.dir;
	string error_filepath = conf.dir;
	if (!conf.pattern.empty()){
		//debug log
		filepath += "/" + conf.pattern + ".all.log";
		em->logfile.init(filepath.c_str());
		em->logfile.open();
		//error log
		error_filepath += "/" + conf.pattern + ".err.log";
		em->errfile.init(error_filepath.c_str());
		em->errfile.open();
	}
	return em;
}
void            logger_lock(logger_t * logger){
    if (!logger){
        return default_logger()->lock.lock();
    }
    return logger->lock.lock();
}
void            logger_unlock(logger_t * logger){
    if (!logger){
        return default_logger()->lock.unlock();
    }
    return logger->lock.unlock();
}

void			logger_destroy(logger_t * logger){
	if (logger) {
		delete logger; 
	}
}
void			logger_set_level(logger_t * logger, log_msg_level_type level){
	if (logger == nullptr){
		logger = default_logger();
	}
	logger->conf.lv = level;
}
int				logger_level(logger_t * logger){
	if (logger == nullptr){
		logger = default_logger();
	}
	if (logger)
		return logger->conf.lv;
	return LOG_LVL_INVALID;
}

//last msg
const char*		logger_msg(logger_t * logger){
	if (logger == nullptr){
		logger = default_logger();
	}
	return logger->last_msg.c_str();
}
//last err
int				logger_errno(logger_t * logger){
	if (logger == nullptr){
		logger = default_logger();
	}
	return logger->last_err;
}

//set last
int				logger_write(logger_t * logger, int loglv, int  sys_err_, const char* fmt, ...){
	if (logger == nullptr){
		logger = default_logger();
	}
	if (loglv < logger->conf.lv){
		return 0;
	}
    logger_lock(logger);
	va_list ap;
	va_start(ap, fmt);
	int n = 0;
	logger->last_err = errno;
	char * msg_start = (char*)logger->last_msg.data();
	n = vsnprintf(msg_start, logger->last_msg.capacity() - 1, fmt, ap);
	va_end(ap);
	int available_size = logger->last_msg.capacity() - (n + 2);
	char errorno_msg_buff[128];
    if (sys_err_ != 0 && available_size > 64 && errno != 0){
		if (msg_start[n-1] == '\n'){
			--n;
		}
		n += snprintf(&msg_start[n], available_size, " [system errno:%d(%s)]\n", errno,
			strerror_r(errno, errorno_msg_buff, sizeof(errorno_msg_buff)-1));
		available_size = logger->last_msg.capacity() - (n + 2);
	}
	if (loglv == LOG_LVL_FATAL && available_size > 128){
		//append stack frame info
		string strstack;
		snprintf(&msg_start[n], available_size, "%s\n",
			dcsutil::stacktrace(strstack, 2));
	}

	if (logger->logfile.write(msg_start, logger->conf.max_roll, logger->conf.max_file_size)){
        fputs(msg_start, stderr);
    }
    if (loglv == LOG_LVL_ERROR || loglv == LOG_LVL_FATAL){
		logger->errfile.write(msg_start, logger->conf.max_roll, logger->conf.max_file_size);
    }
    logger_unlock(logger);
	return n;
}
