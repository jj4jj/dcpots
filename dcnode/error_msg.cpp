#include "error_msg.h"
#include "stdinc.h"
#include "utility.hpp"

struct error_msg_t : public noncopyable {
	int		last_err;
	error_msg_t * backtrace;
	string	last_msg;
	int			max_buff_sz;
	error_msg_t() :last_err(0),backtrace(nullptr) {}
};

error_msg_t *	error_create(int max_msg_buff){
	error_msg_t * em = new error_msg_t();
	if (!em) return nullptr;
	em->last_msg.reserve(max_msg_buff);
	em->max_buff_sz = max_msg_buff;
	return em;
}
void			error_destroy(error_msg_t * em){
	if (em) {
		delete em; em = nullptr;
	}
}
//last msg
const char*		error_msg(error_msg_t * em, const char * trace_deli){
	error_msg_t * it = em;
	int max_layer = 20;
	while (trace_deli && it->backtrace &&
			em->last_msg.length() < (size_t)em->max_buff_sz &&
			max_layer > 0 ){
		em->last_msg += trace_deli;
		em->last_msg += error_msg(it->backtrace);
		it = it->backtrace;
		max_layer--;
	}
	em->backtrace = nullptr; //for msg more and more
	return em->last_msg.c_str();
}
//last err
int				error_errno(error_msg_t * em){
	return em->last_err;
}

//set last
int			error_write(error_msg_t * em, int err, error_msg_t * killer, const char* fmt, ...)
{
	em->last_err = err;
	em->backtrace = killer;
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(&(*(em->last_msg.begin())),em->last_msg.capacity() - 1 ,fmt, ap);
	va_end(ap);
	return n;
}
