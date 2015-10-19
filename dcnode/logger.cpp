#include "logger.h"
#include "stdinc.h"
#include "utility.hpp"

struct logger_t {
	logger_config_t	conf;
	//lock info todo ?
	int			last_err;
	string		last_msg;
	FILE	*	pf;
	int			next_rollid;
	bool		inited;
	string		logfile;//current
	log_msg_level_type	 level;
	logger_t() :last_err(0){
		pf = nullptr;
		next_rollid = 1;
		inited = false;
	}
};

static	logger_t * G_LOGGER = nullptr;
int				global_logger_init(const logger_config_t & conf){
	if (G_LOGGER){
		return -1;
	}
	G_LOGGER = logger_create(conf);
	return G_LOGGER ? 0 : -1;
}
void			global_logger_destroy(){
	if (!G_LOGGER){
		return;
	}
	logger_destroy(G_LOGGER);
	G_LOGGER = nullptr;
}

logger_t *	logger_create(const logger_config_t & conf){
	FILE * pf = nullptr;
	int	 nextrollid = 1;
	string filepath = conf.path + "/" + conf.pattern;
	if (filepath.length() > 1){
		pf = fopen(filepath.c_str(), "a+");
		if (pf == nullptr){
			LOGE(LOG_LVL_FATAL ,errno,"open file :%s error!",filepath.c_str());
			return nullptr;
		}
		char nextfileid[32];
		FILE* rfp = fopen(filepath.c_str(), "r");
		if (rfp){
			size_t sz = sizeof(nextfileid)-1;
			char * pbuf = nextfileid;
			if (getline(&pbuf, &sz, rfp) > 0){
				nextrollid = strtoul(nextfileid, nullptr, 10);
			}
			fclose(rfp);
		}
		if (nextrollid <= 0){
			//write 1 begin
			nextrollid = 1;
			fputs("1", pf);
		}
	}
	logger_t * em = new logger_t();
	if (!em) return nullptr;
	em->last_msg.reserve(conf.max_msg_size);
	em->conf = conf;
	em->inited = true;
	em->next_rollid = nextrollid;
	em->logfile = filepath;
	em->pf = pf;

	return em;
}
void			logger_destroy(logger_t * logger){
	if (logger) {
		if (logger->pf){
			fclose(logger->pf);
		}
		delete logger; 
	}
}
void			logger_set_level(logger_t * logger, log_msg_level_type level){
	if (logger == nullptr){
		logger = G_LOGGER;
	}
	logger->level = level;
}
int				logger_level(logger_t * logger){
	if (logger == nullptr){
		logger = G_LOGGER;
	}
	return logger->level;
}

//last msg
const char*		logger_msg(logger_t * logger){
	if (logger == nullptr){
		logger = G_LOGGER;
	}
	return logger->last_msg.c_str();
}
//last err
int				logger_errno(logger_t * logger){
	if (logger == nullptr){
		logger = G_LOGGER;
	}
	return logger->last_err;
}

//set last
int				logger_write(logger_t * logger, int err, const char* fmt, ...)
{
	if (logger == nullptr){
		logger = G_LOGGER;
	}
	va_list ap;
	va_start(ap, fmt);
	int n = 0;
	logger->last_err = err;
	n = vsnprintf(&(*(logger->last_msg.begin())), logger->last_msg.capacity() - 1, fmt, ap);
	va_end(ap);
	if (logger->pf){
		fputs(logger->last_msg.c_str(), logger->pf);
		if (ftell(logger->pf) >= logger->conf.max_file_size){
			//shift
			fflush(logger->pf);
			logger->pf = nullptr;
			string nextfile = logger->logfile + "." + std::to_string(logger->next_rollid);
			logger->next_rollid = (logger->next_rollid + 1) % logger->conf.max_roll + 1;
			rename(logger->logfile.c_str(), nextfile.c_str());
			rewind(logger->pf);
			fputs(std::to_string(logger->next_rollid).c_str(), logger->pf);
		}
	}
	else{
		fputs(logger->last_msg.c_str(), stderr);
	}
	return n;
}
