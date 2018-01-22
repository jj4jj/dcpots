#include "stdinc.h"
#include "logger.h"
#include "dcshm.h"
#include "dcipsync.h"

static inline bool _shm_exists(key_t k, size_t sz = 0, int ioflag = 0){
    int id = shmget(k, sz, IPC_CREAT | IPC_EXCL | ioflag);
    if (id == -1 && errno == EEXIST){
        return true;
    }
    if (id != -1){
        shmctl(id, IPC_RMID, NULL);
    }
    return false;
}
static inline void _shm_delete(int key){
    int id = shmget(key, 0, 0);
    if (id == -1){
        GLOG_SER("shm get error from key:%u", key);
    }
    else {
        shmctl(id, IPC_RMID, NULL);
    }
}
int             dcshm_path_key(const char * path, unsigned char proj_id){
    return dcs::path_to_key(path, proj_id);
}
void *			dcshm_open(int key, bool & attach, size_t size){
    if (key < 0){
        GLOG_ERR("open shm with error key:%d size:%d attach:%d", key, size, attach);
        return nullptr;
    }
	int ioflag = 0666;
    if (_shm_exists(key, size, ioflag)){
        attach = true;
    }
	else { //not exist . create one
        if (attach){
            GLOG_WAR("shm attach error [path key:%u shm not exist !]", key);
            return nullptr;
        }
	}
	int flags = IPC_CREAT | ioflag;
    int id = shmget(key, size, flags);
	if (id < 0){ //create error
        GLOG_SER("shm get error ! key=%u size:%zu flags:%d", key, size, flags);
        return nullptr;
	}
	void * ap = shmat(id, NULL, ioflag);
	if (ap == (void*)(-1)){
		//attach error
        GLOG_SER("shm address attach error id:%u!", id);
        return nullptr;
	}
	return ap;
}
void	 dcshm_close(void * p, int key){
	if (p){
		shmdt(p);
	}
    if (key != -1){
        _shm_delete(key);
    }
}
void	dcshm_set_delete(int key){
    _shm_delete(key);
}
