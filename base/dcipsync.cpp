
#include "logger.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

namespace dcs {


struct ipsync_t {
    int     semid {-1};
};
    
int     path_to_key(const char * path, unsigned char proj_id){
    key_t key = ftok(path, proj_id);
    if (key < 0) {
        GLOG_SER("ftoken error path = %s proj id:%d", path, proj_id);
        return -1;
    }
    return key;
}

ipsync_t *   ipsync_init(int key, bool attach){
    int   semid = -1;
    int flags = IPC_CREAT|0666|IPC_EXCL;
    bool attached = false;
    semid = semget(key, 1, flags);
    if(semid < 0){
        if(errno == EEXIST){
            //exist already
            flags = IPC_CREAT|0666;
            semid = semget(key, 1, flags);            
        }
        if(semid < 0) {
            GLOG_SER("semget error attach:%d !", attach);
            return nullptr;
        }
    }
    else {
        //no exist , and create one
        if(attach){
            semctl(semid, 0, IPC_RMID);
            GLOG_ERR("attach sem but not exist create one now rm it !");
            return nullptr;
        }
        //first create
        if(semctl(semid, 0, SETVAL, 1)){
            GLOG_SER("init sem error !");
            semctl(semid, 0, IPC_RMID);
            return nullptr;
        }
    }

    ipsync_t * ips = new ipsync_t ();
    ips->semid = semid;

    return ips;
}

int        ipsync_lock(ipsync_t * is, bool wait){
    if(is){
        struct sembuf sbop;
        memset(&sbop, 0, sizeof(sbop));
        sbop.sem_num = 0;
        sbop.sem_op = -1;
        sbop.sem_flg = SEM_UNDO;
        if (!wait) {
            sbop.sem_flg |= IPC_NOWAIT;
        }

        if (semop(is->semid, &sbop, 1) < 0) {
            GLOG_SER("semop error flags:%d", sbop.sem_flg);
            return -1;
        }
        return 0;
    }
    else {
        GLOG_ERR("lock sync is null !");
        return -1;
    }
}


void         ipsync_unlock(ipsync_t * is){
    if(is){
        struct sembuf sbop;
        memset(&sbop, 0, sizeof(sbop));
        sbop.sem_num = 0;
        sbop.sem_op = +1;
        sbop.sem_flg = IPC_NOWAIT;
        if (semop(is->semid, &sbop, 1) < 0) {
            GLOG_SER("semop error flags:%d", sbop.sem_flg);
        }
    }
    else {
        GLOG_ERR("lock sync is null !");
    }
}

void        ipsync_free(ipsync_t * is){
    if(is){
        if (is->semid != -1) {
            semctl(is->semid, 0, IPC_RMID);
        }
        delete is;    
    }
    else {
        GLOG_ERR("lock sync is null !");
    }
}



};