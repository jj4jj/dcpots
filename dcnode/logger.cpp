#include "logger.h"
#include "stdinc.h"
#include "utility.hpp"

struct logger_t {
	logger_config_t	conf;
	//lock info todo ?
	int		last_err;
	string	last_msg;
	FILE	*	pf;
	int			next_fid;
	bool		inited;
	logger_t() :last_err(0){
		pf = nullptr;
		next_fid = 1;
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
	logger_t * em = new logger_t();
	if (!em) return nullptr;
	em->last_msg.reserve(conf.max_msg_size);
	em->conf = conf;
	return em;
}
void			logger_destroy(logger_t * em){
	if (em) {
		delete em; em = nullptr;
	}
}
//last msg
const char*		logger_msg(logger_t * em){
	return em->last_msg.c_str();
}
//last err
int				logger_errno(logger_t * em){
	return em->last_err;
}

//set last
int			logger_write(logger_t * em, int err, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int n = 0;
	if (em) {
		em->last_err = err;
		n = vsnprintf(&(*(em->last_msg.begin())), em->last_msg.capacity() - 1, fmt, ap);
	}
	else {
		G_LOGGER->last_err = err;
		n = vsnprintf(&(*(G_LOGGER->last_msg.begin())), G_LOGGER->last_msg.capacity() - 1, fmt, ap);
	}
	va_end(ap);
	return n;
}
