#pragma once
#include "stdinc.h"

struct sshm_config_t {
	string	shm_path;
	int		shm_size;
	bool	attach; //just attach , no create
};


int		 sshm_create(const sshm_config_t & conf , void ** p, bool & attached);
void	 sshm_destroy(void *);







