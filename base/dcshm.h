#pragma once

int         dcshm_path_key(const char * path, unsigned char proj_id = 1);
void	 *	dcshm_open(int k, bool & attach, size_t sz = 0);
void		dcshm_close(void * p, int shmkey_set_delete = -1);
void 		dcshm_set_delete(int k);







