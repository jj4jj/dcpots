#pragma once

struct error_msg_t;

error_msg_t *	error_create(int max_msg_buff = 1024);
void			error_destroy(error_msg_t *);
//last msg
const char*		error_msg(error_msg_t *, const char * trace_deli = nullptr);
//last err
int				error_errno(error_msg_t *);
//set last
int				error_write(error_msg_t *, int err, error_msg_t * killer, const char* fmt, ...);

//back trace
#define ERROR_BT_MSG(erm, killer, err_no, ...)		do{\
	error_write((erm), (err_no), killer, __VA_ARGS__);\
}while (0)

#define ERROR_MSG(erm, err_no, ...)		do{\
	error_write((erm), (err_no), nullptr, __VA_ARGS__); \
}while (0)

#define ERROR_ENV_MSG(erm, err_no, fmt, ...)		do{\
	timeval tv; gettimeofday(&tv, NULL);\
	error_write((erm), (err_no), nullptr, "(env>%u.%u|%s:%d)"##fmt, tv.tv_sec, tv.tv_usec, __file__, __line__, __VA_ARGS__); \
}while (0)

//	printf("(env>%ld.%ld|%s:%d) "format, tv.tv_sec, tv.tv_usec, __FILE__, __LINE__, __VA_ARGS__);
#define LOGP(format, ...)		do{\
	timeval tv; gettimeofday(&tv, NULL); \
	printf("(env>%ld.%ld|%s:%d) " format "\n", tv.tv_sec, tv.tv_usec, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}while (0)
