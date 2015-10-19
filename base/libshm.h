#pragma once
#include "stdinc.h"

struct sshm_config_t {
	string	shm_path;
	int		shm_size;
	bool	attach; //just attach , no create
};
struct sshm_t;
enum shm_error_type {
	SHM_OK = 0,
	SHM_EXIST = 1,
	SHM_NOT_EXIST = 2,
	SHM_SIZE_NOT_MATCH = 3,
	SHM_ERR_PERM = 4,
	SHM_REF_ERRNO = 0x7FFF, //+errno
};

int			sshm_create(const sshm_config_t & conf, void ** p, bool & attached);
void		sshm_destroy(void *);







