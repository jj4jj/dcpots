#include "dclogfile.h"
#include "logger.h"
#include "dcdebug.h"
#include "dctls.h"

struct logger_t {
	logger_config_t	    conf;
    std::mutex          lock;
    int			        last_err {0};
	string		        last_msg;
    dcs::logfile_t      logfile;
    dcs::logfile_t      errfile;
    bool                errfilt {false};
};

logger_config_t::logger_config_t() :max_roll(20),
max_msg_size(1024 * 1024), max_file_size(1024 * 1024 * 10) {
    lv = LOG_LVL_TRACE;
}
static	logger_t * g_default_logger = nullptr;
struct LoggerWriteTls {};
dcs::thread_local_storage<LoggerWriteTls, int>  thread_is_writing_log;
int    tls_flag_is_writing = 0; //dummy just for address is a flag
int    tls_flag_is_not_writing = 0; //dummy just for address is a flag

logger_t *		default_logger(){
    if (!g_default_logger){
        int ret = default_logger_init(logger_config_t());
        if(ret){
            fprintf(stderr, "default logger init error:%d", ret);
            return nullptr;
        }
    }
	return g_default_logger;
}
int				default_logger_init(const logger_config_t & conf){
	if (g_default_logger){
		default_logger_destroy();
		g_default_logger = nullptr;
	}
	g_default_logger = logger_create(conf);
	return g_default_logger ? 0 : -1;
}
void			default_logger_destroy(){
	if (!g_default_logger){
		return;
	}
	logger_destroy(g_default_logger);
	g_default_logger = nullptr;
}

logger_t *	logger_create(const logger_config_t & conf){
	logger_t * em = new logger_t();
    if (!em) {return nullptr;}
	em->last_msg.reserve(conf.max_msg_size);
	em->conf = conf;
    if(conf.path.empty() || conf.path == "stdout"){
        return em;
    }

    string log_all_filepath =  conf.path;
    if (log_all_filepath.find(".all.log") == string::npos) {
        log_all_filepath += ".all.log";
    }
    string log_err_filepath = log_all_filepath;
    log_err_filepath.replace(log_err_filepath.find(".all.log"), 8, ".err.log");

    //all log
    int ret = em->logfile.init(log_all_filepath.c_str(), conf.max_roll, conf.max_file_size);
    if (ret) {
        fprintf(stderr, "warn: init all log path:%s error:%d", log_all_filepath.c_str(), ret);
    }

    //error log
    if(!log_err_filepath.empty()){
        em->errfilt = true;
        ret = em->errfile.init(log_err_filepath.c_str(), conf.max_roll, conf.max_file_size);
        if (ret) {
            fprintf(stderr, "warn: init error log path:%s error:%d", log_err_filepath.c_str(), ret);
        }    
    }
	return em;
}
bool            logger_try_lock(logger_t * logger){
    if (!logger) {
        return default_logger()->lock.try_lock();
    }
    return logger->lock.try_lock();
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
    if(logger){logger->conf.lv = level;}
}
int				logger_level(logger_t * logger){
	if (logger == nullptr){
		logger = default_logger();
	}
    if (logger) {return logger->conf.lv;}
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
    if(logger){
        return logger->last_err;
    }
    return 0;
}

int				logger_write(logger_t * logger, int loglv, int  sys_err_, const char* fmt, ...){
	if (logger == nullptr){
		logger = default_logger();
	}
    if(!logger){
        fprintf(stderr, "logger is null !");
        return -1;
    }
	if (loglv < logger->conf.lv){
		return 0;
	}
    int * writingp = thread_is_writing_log.get();
    if(writingp && writingp == &tls_flag_is_writing){
        fprintf(stderr, "doing logging but logging recursive logger:%p lv:%d pserr:%d fmt:[%s]!", logger, loglv, sys_err_, fmt);
        return -1;
    }
    thread_is_writing_log.set(&tls_flag_is_writing);//is writing
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
	if (loglv == LOG_LVL_FATAL && available_size > 192){
		//append stack frame info
        if (msg_start[n - 1] == '\n') {
            --n;
        }
		string strstack;
		n += snprintf(&msg_start[n], available_size, "%s\n",
			dcs::stacktrace(strstack, 2, 16, nullptr, " <-- "));
        available_size = logger->last_msg.capacity() - (n + 2);
	}
    msg_start[n] = 0;
    if (logger->logfile.write(msg_start)) {
        fputs(msg_start, stderr);
    }
    if(logger->errfilt){
        if (loglv == LOG_LVL_ERROR || loglv == LOG_LVL_FATAL) {
            logger->errfile.write(msg_start);
        }
    }
    ///////////////////////////////////////////////////////////////////////    
    thread_is_writing_log.set(&tls_flag_is_not_writing);
    logger_unlock(logger);
	return n;
}
