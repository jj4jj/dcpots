
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

ipsync_t *   ipsync_lock(int key, bool wait, bool attach){
    int   semid = -1;
    int flags = IPC_CREAT | 0666;
    if(attach){flags |= IPC_EXCL;}
    semid = semget(key, 1, flags);
    if(semid < 0){
        GLOG_SER("semget error attach:%d !", attach);
        return nullptr;
    }
    struct sembuf sbop;
    memset(&sbop, 0 , sizeof(sbop));
    sbop.sem_op = -1;
    sbop.sem_flg = SEM_UNDO;
    if(!wait){
        sbop.sem_flg |= IPC_NOWAIT;    
    }

    if(semop(semid, &sbop, 1) < 0){
        GLOG_SER("semop error flags:%d", sbop.sem_flg);
        return nullptr;
    }

    ipsync_t * ips = new ipsync_t ();
    ips->semid = semid;

    return ips;
} 
void         ipsync_unlock(ipsync_t * is){
    if(is){
        if(is->semid != -1){
            semctl(is->semid, 0, IPC_RMID);
        }
        delete is;    
    }
}



};