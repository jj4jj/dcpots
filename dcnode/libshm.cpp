#include "libshm.h"

bool	_shm_exists(key_t k ,int ioflag = 0){
	int id = shmget(k, 0, IPC_CREAT | IPC_EXCL | ioflag);
	if (id < 0 && errno == EEXIST) {
		return true;
	}
	if (id >= 0){
		shmctl(id, IPC_RMID, NULL);
	}
	return false;
}
int		 sshm_create(const sshm_config_t & conf, void ** p, bool & attached){
	key_t key = ftok(conf.shm_path.c_str(), 1);
	if (key < 0){
		return -1;
	}
	int ioflag = 0666;
	bool exist = _shm_exists(key, ioflag);
	if (exist){
		attached = true;
	}
	else {
		if (!conf.attach){
			//not exist
			return -2;
		}
		attached = false;
	}
	int flags = IPC_CREAT | ioflag;
	int id = shmget(key, conf.shm_size, flags);
	if (id < 0){ //create error
		return -3;
	}
	void *ap = shmat(id, NULL, ioflag);
	if (ap == (void*)(-1)){
		shmctl(id, IPC_RMID, NULL);
		return -4;
	}
	*p = ap;
	return 0;
}
void	 sshm_destroy(void * p){
	if (p){
		shmdt(p);
	}
}
