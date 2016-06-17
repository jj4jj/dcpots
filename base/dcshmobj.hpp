#pragma  once

#include <vector>
#include <string>
#include "dcshm.h"
//any pod struct , can allocate a memory with shm
//auto init , auto recover .

#pragma pack(1)
struct dcshmobj_pool_mm_fmt {
    enum {
        MAX_OBJECT_TYPE_NAME_LEN = 32,
        MAX_OBJECT_TYPE_COUNT = 128,
        BODY_CHUNK_BLOCK_SIZE = 8,
    };
    struct _head_t {
        uint16_t        objects_count;
        struct {
            uint32_t    block_offset;
            uint32_t    block_count;
            char        name[32];
        } objects[MAX_OBJECT_TYPE_COUNT];
    } head;
    struct _body_t {
        uint8_t         blocks[1][BODY_CHUNK_BLOCK_SIZE];
    } body;
};
#pragma pack()

struct dcshmobj_user_t {
    virtual const char * name() const {
        return "shmobj_user";
    }
    virtual void on_created(void * data) {
    }
    virtual int  on_attached(){
        return 0;
    }
    virtual size_t  size(){
        return 0;
    }
};


template<class U>
struct dcshmobj_t : public dcshmobj_user_t {
    U             * data;
    dcshmobj_t():data(nullptr){
    }
    virtual const char * name() const {
        return typeid(U).name();
    }
    virtual size_t size() const {
        return sizeof(U);
    }
    virtual void on_created(void * pv) {
        assert(!this->data);
        this->data = new(pv)U();
    }
};

struct dcshmobj_pool {
    dcshmobj_pool_mm_fmt *            shm{ nullptr };    
    std::vector<dcshmobj_user_t*>     shm_users;
    //list[magic][size_t*8][next][chunk][magic]
    int     regis(dcshmobj_user_t * user){
        std::string strname;
        strname = user->name();
        for (dcshmobj_user_t * u : this->shm_users){
            if (strname == u->name()){
                return -1;
            }
        }
        shm_users.push_back(user);
        return 0;
    }
    static size_t  alien_size(size_t sz){
        return (sz + dcshmobj_pool_mm_fmt::BODY_CHUNK_BLOCK_SIZE - 1) / dcshmobj_pool_mm_fmt::BODY_CHUNK_BLOCK_SIZE * dcshmobj_pool_mm_fmt::BODY_CHUNK_BLOCK_SIZE;
    }
    size_t  total_size(){
        size_t sz = alien_size(sizeof(dcshmobj_pool_mm_fmt));
        for (auto u : shm_users){
            sz += alien_size(u->size());
        }
        return sz;
    }
    int     check_valid(){
        return 0;
    }
    dcshmobj_user_t * find_user(const char * name){
        return nullptr;
    }
    int     start(const char * keypath){
        size_t total_size = total_size();
        dcshm_config_t & shconf;
        shconf.shm_path = keypath;
        shconf.shm_size = total_size;
        shconf.attach = false;
        void * p = NULL;
        bool attached = false;
        int	ret = dcshm_create(shconf, &shm, attached);
        if (ret){
            GLOG_SER("shm create error alloc size:%zu ret:%d", total_size, ret);
            return -1;
        }
        assert(shm);
        if (attached){
            //check states , verify data
            ret = check_valid();
            if (ret){
                GLOG_ERR("check shm valid error :%d", ret);
                return -2;
            }
            for (size_t i = 0; i < shm->head.objects_count; ++i){
                const char * objname = shm->head.objects[i].name;
                dcshmobj_user_t * user = find_user(objname);
                assert(user);
                user->on_created(&shm->body.blocks[shm->head.objects[i].block_offset][0]);
                user->on_attached();
            }
        }
        else {
            //create
            for (int i = 0; i < this->shm_users.size(); ++i){
                for (size_t i = 0; i < shm->head.objects_count; ++i){
                    const char * objname = shm->head.objects[i].name;
                    dcshmobj_user_t * user = find_user(objname);
                    assert(user);
                    user->on_created(&shm->body.blocks[shm->head.objects[i].block_offset][0]);
                }
            }
            //check recover shm
            //if there exists a backup data , recover them

        }

        //key 1 : list block (flat space)
        //key 2 : dump compress  (narrow space)
        //if key 1 space ok attach
        //else using key 2 //else fail
    }
    int     stop(){
        //for all , dumps to key 2
        //

    }
};
