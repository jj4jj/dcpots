
#include "logger.h"
#include "dcshm.h"
#include "dcshmobj.hpp"


//////////////////////////////////////////////////////////////////
const char * dcshmobj_user_t::name() const { return "shmobj_user"; }
int          dcshmobj_user_t::on_alloced(void * data, bool attached) { UNUSED(data); UNUSED(attached); return 0; }
size_t       dcshmobj_user_t::size() const { return 0; }
size_t       dcshmobj_user_t::pack_size(void * ) const { return 0; }
bool         dcshmobj_user_t::pack(void * , size_t , const void * ) const { return false; }
bool         dcshmobj_user_t::unpack(void * , const void *, size_t ){ return false; }
//////////////////////////////////////////////////////////////////
#pragma pack(1)
struct dcshmobj_pool_mm_fmt {
    enum {
        MAX_OBJECT_TYPE_NAME_LEN = 32,
        MAX_OBJECT_TYPE_COUNT = 128,
        CHUNK_BLOCK_SIZE = 8,
        MAX_SAFE_MAGIC_LENGTH = 1024,
        MAX_USER_NAME_LEN = 32,
        SAFE_CHECK_MAGIC_NUMBER = 0xAA55AA55,
    };
    struct _head_t {
        uint16_t        version;
        uint16_t        objects_count;
        struct {
            uint32_t    block_offset;
            uint32_t    block_count;
            uint32_t    real_size;
            char        name[MAX_USER_NAME_LEN];
        } objects[MAX_OBJECT_TYPE_COUNT];
    } head;
    uint8_t padding[CHUNK_BLOCK_SIZE - (sizeof(_head_t) % CHUNK_BLOCK_SIZE)];
    struct _body_t {
        uint8_t         blocks[1][CHUNK_BLOCK_SIZE];
    } body;
    struct _safe_magic_t {
        char    magic[MAX_SAFE_MAGIC_LENGTH];
        char    name[MAX_USER_NAME_LEN];
        size_t  offset;
        size_t  count;
        void    fill(const char * nam, size_t ofs, size_t cnt){
            for (size_t i = 0; i < sizeof(magic) / sizeof(int); i += sizeof(int)){
                *(int*)(magic + i) = SAFE_CHECK_MAGIC_NUMBER;
            }
            strncpy(name, nam, sizeof(name)-1);
            offset = ofs;
            count = cnt;
        }
        bool    check(){
            for (size_t i = 0; i < sizeof(magic) / sizeof(int); i += sizeof(int)){
                if (*(int*)(magic + i) != (int)SAFE_CHECK_MAGIC_NUMBER){
                    return false;
                }
            }
            return true;
        }
    };
    /////////////////////////////////////////////////////////////////////////////////////
    static size_t  size_block(size_t sz){
        return (sz + CHUNK_BLOCK_SIZE - 1) / CHUNK_BLOCK_SIZE;
    }
    static size_t  align_size(size_t sz){
        return size_block(sz) * CHUNK_BLOCK_SIZE;
    }
    const char *   stat_dump(std::string & str) const {
        str += "objects count:" + std::to_string(head.objects_count);
        str += " version:"+ std::to_string(head.version) + " => list:";
        for (size_t i = 0; i < head.objects_count; ++i){
            str += "{<user name:";
            str += head.objects[i].name;
            str += ">,";
            str += "<block offset:";
            str += std::to_string(head.objects[i].block_offset);
            str += ">,";
            str += "<block count:";
            str += std::to_string(head.objects[i].block_count);
            str += ">,";
            str += "<real size:";
            str += std::to_string(head.objects[i].real_size);
            str += ">}";
            if (i + 1 < head.objects_count){
                str += ", ";
            }
        }
        return str.c_str();
    }
};
#pragma pack()


///////////////////////////////////////////////////////////////////
struct dcshmobj_pool_impl {
    int               regis(dcshmobj_user_t * user);
    size_t            total_size() const;
    int               start(const char * keypath);
    int               stop();
    //////////////////////////////////////////////////
    dcshmobj_pool_mm_fmt *            shm{ nullptr };
    std::vector<dcshmobj_user_t*>     shm_users;
    std::string                       shm_keypath;
private:
    int               check_valid() const;
    dcshmobj_user_t * find_user(const char * name) const;
    int               find_user_shm_idx(dcshmobj_pool_mm_fmt * ushm, const  char * name);
    void            * find_user_shm_addr(dcshmobj_pool_mm_fmt * ushm, const char * name);
    int               start_with_backup_shm(dcshmobj_pool_mm_fmt * shmbackup);
};

int     dcshmobj_pool_impl::regis(dcshmobj_user_t * user){
    if (!user || !(*user->name())){
        return -1;
    }
    std::string strname = user->name();
    for (dcshmobj_user_t * u : this->shm_users){
        if (strname == u->name()){
            return -1;
        }
    }
    shm_users.push_back(user);
    GLOG_IFO("shm pool register user:%s alloc size:%zu", user->name(), user->size());
    return 0;
}
size_t  dcshmobj_pool_impl::total_size() const {
    size_t sz = dcshmobj_pool_mm_fmt::align_size(sizeof(dcshmobj_pool_mm_fmt));
    for (auto u : shm_users){
        sz += dcshmobj_pool_mm_fmt::align_size(u->size());
    }
    return sz;
}
int     dcshmobj_pool_impl::check_valid() const {
    if (shm->head.version == 0){
        GLOG_ERR("dcshmobj pool check valid version error !");
        return -1;
    }
    for (size_t i = 0; i < shm->head.objects_count; ++i){
        dcshmobj_user_t * user = find_user(shm->head.objects[i].name);
        if (!user){
            GLOG_WAR("not found user object in shm name:%s", shm->head.objects[i].name);
            continue;//return -(i + 1);
        }
        if (shm->head.objects[i].real_size != user->size()){
            GLOG_ERR("size not matched when check user name:%s", shm->head.objects[i].name);
            return i+1;
        }
        ////////////////////////////////////////////////////////////////////////
        if (i == 0){
            continue;
        }
        dcshmobj_pool_mm_fmt::_safe_magic_t * safe_block = \
            (dcshmobj_pool_mm_fmt::_safe_magic_t *)\
            shm->body.blocks[shm->head.objects[i-1].block_offset+\
            shm->head.objects[i - 1].block_count];
        if (!safe_block->check()){
            return -i;
        }
    }
    return 0;
}
dcshmobj_user_t * dcshmobj_pool_impl::find_user(const char * name) const {
    for (size_t i = 0; i < shm_users.size(); ++i){
        if (strcmp(shm_users[i]->name(), name) == 0){
            return shm_users[i];
        }
    }
    return nullptr;
}
void  * dcshmobj_pool_impl::find_user_shm_addr(dcshmobj_pool_mm_fmt * ushm, const char * name){
    int idx = find_user_shm_idx(ushm, name);
    if (idx < 0) {
        return nullptr;
    }
    return shm->body.blocks[shm->head.objects[idx].block_offset];
}

int dcshmobj_pool_impl::find_user_shm_idx(dcshmobj_pool_mm_fmt * ushm, const char * name){
    for (size_t i = 0; i < ushm->head.objects_count; ++i){
        if (strcmp(ushm->head.objects[i].name, name) == 0){
            return i;
        }
    }
    return -1;
}
static inline dcshmobj_pool_mm_fmt * _dcshmobj_pool_backup_shm_open(const char * shm_keypath, int sz = 0){
    int key = dcshm_path_key(shm_keypath, 2);
    bool backup_shm_attached = true;
    if (sz > 0){
        backup_shm_attached = false;
    }
    dcshmobj_pool_mm_fmt * shmbackup = (dcshmobj_pool_mm_fmt *)dcshm_open(key, backup_shm_attached, sz);
    return shmbackup;
}
static inline dcshmobj_pool_mm_fmt * _dcshmobj_pool_shm_open(const char * shm_keypath, size_t sz, bool & shm_attached ){
    int key = dcshm_path_key(shm_keypath, 1);
    dcshmobj_pool_mm_fmt * shmp = (dcshmobj_pool_mm_fmt *)dcshm_open(key, shm_attached, sz);
    return shmp;
}

int     dcshmobj_pool_impl::start_with_backup_shm(dcshmobj_pool_mm_fmt * shmbackup){
    if (shm == nullptr){
        bool attach = false;
        shm = _dcshmobj_pool_shm_open(shm_keypath.c_str(), total_size(), attach);
        if (!shm){
            GLOG_ERR("alloate shm size :%zu path:%s error !", total_size(), shm_keypath.c_str());
            return -3;
        }
    }

    //allocate all init
    int ret = 0;
    shm->head.objects_count = 0;
    shm->head.version = 1;
    size_t nb_alloced = 0;
    size_t safe_block_count = dcshmobj_pool_mm_fmt::size_block(sizeof(dcshmobj_pool_mm_fmt::_safe_magic_t));
    for (size_t i = 0; i < this->shm_users.size(); ++i){
        dcshmobj_user_t * user = this->shm_users[i];
        strncpy(shm->head.objects[i].name, user->name(), sizeof(shm->head.objects[i].name) - 1);
        shm->head.objects[i].block_offset = nb_alloced;
        shm->head.objects[i].block_count = dcshmobj_pool_mm_fmt::size_block(user->size());
        shm->head.objects[i].real_size = user->size();
        dcshmobj_pool_mm_fmt::_safe_magic_t * safe_block = (dcshmobj_pool_mm_fmt::_safe_magic_t *)
            shm->body.blocks[nb_alloced + shm->head.objects[i].block_count];
        safe_block->fill(user->name(), nb_alloced, shm->head.objects[i].block_count);
        //fill safe check
        nb_alloced += shm->head.objects[i].block_count + safe_block_count;
        ++shm->head.objects_count;
    }
    //check recover shm
    if (!shmbackup){
        GLOG_IFO("not found backup recover shm , just init allocated shm ...");
        for (size_t i = 0; i < shm->head.objects_count; ++i){
            const char * objname = shm->head.objects[i].name;
            dcshmobj_user_t * user = find_user(objname);
            assert(user);
            GLOG_IFO("allocate shm %s init new fresh ...", user->name());
            ret = user->on_alloced(&shm->body.blocks[shm->head.objects[i].block_offset][0], false);
            if (ret){
                GLOG_ERR("allocated user :%s size:%zu alloc init error ret:%d !", user->name(), user->size(), ret);
                return -4;
            }
        }
    }
    else {
        GLOG_IFO("found backup recover shm object count:%u, init some allocated shm with backup recover ...", shm->head.objects_count);
        for (size_t i = 0; i < shm->head.objects_count; ++i){
            const char * objname = shm->head.objects[i].name;
            dcshmobj_user_t * user = find_user(objname);
            assert(user);
            //////////////////////////////////////////////////////////////////////////////////////
            bool recover_ok = false;
            int  ubakup_shm_idx = find_user_shm_idx(shmbackup, objname);
            if (ubakup_shm_idx < 0){
                recover_ok = false;
            }
            else {
                void * ubackshm_p = &shmbackup->body.blocks[shmbackup->head.objects[ubakup_shm_idx].block_offset][0];
                size_t ubackshm_sz = shmbackup->head.objects[ubakup_shm_idx].real_size;
                void * shm_p = &shm->body.blocks[shm->head.objects[i].block_offset][0];
                size_t shm_sz = shm->head.objects[i].real_size;
                if (!user->unpack(shm_p, ubackshm_p, ubackshm_sz)){
                    GLOG_ERR("name:%s recover shm unpack error shm_sz:%zu ubackshm_sz:%zu !",
                        user->name(), shm_sz, ubackshm_sz);
                    return -5;
                }
                recover_ok = true;
            }
            GLOG_IFO("allocate shm %s recover state(attached):%d", user->name(), recover_ok ? 1 : 0);
            ret = user->on_alloced(&shm->body.blocks[shm->head.objects[i].block_offset][0], recover_ok);
            if (ret){
                GLOG_ERR("allocated user :%s size:%zu alloc init error ret:%d recover state:%d!",
                    user->name(), user->size(), ret, recover_ok ? 1 : 0);
                return -6;
            }
        }
        shm->head.version = shmbackup->head.version + 1;
        dcshm_close(shmbackup, dcshm_path_key(shm_keypath.c_str(), 2));
    }
    return 0;
}

int     dcshmobj_pool_impl::start(const char * keypath){
    if (shm_users.empty()){
        return 0;
    }
    bool attached = true; //just attach first try
    int ret = 0;
    shm_keypath = keypath;
    shm = _dcshmobj_pool_shm_open(keypath, total_size(), attached);
    dcshmobj_pool_mm_fmt * shmbackup = _dcshmobj_pool_backup_shm_open(keypath);
    std::string shm_stat_str;
    if (shm && attached){
        GLOG_IFO("recover shm attached stat:%s", shm->stat_dump(shm_stat_str));
    }
    if (shmbackup){
        GLOG_IFO("backup recover shm attached stat:%s", shm->stat_dump(shm_stat_str));
    }
    if (!shm){
        return start_with_backup_shm(shmbackup);
    }
    assert(attached);
    //must be attach
    ret = check_valid();
    if (ret > 0 && !shmbackup){
        GLOG_ERR("check shm size not match error :%d and not found backup recover shm (maybe should clear shm ? or checking program update error ?)", ret);
        return -2;
    }
    else if (ret != 0){ //not ok , restart shm
        if (shmbackup && shm->head.version != shmbackup->head.version){
            GLOG_ERR("recover shm and backup shm verion (%d,%d) not matched ! ",
                shm->head.version, shmbackup->head.version);
            return -3;
        }
        GLOG_WAR("check shm valid error :%d (>0:size not match;<0:memory broken) recover backup shm :%p , realloc shm trying ...", ret, shmbackup);
        dcshm_close(shm, dcshm_path_key(shm_keypath.c_str(), 1));
        shm = nullptr;
        return start_with_backup_shm(shmbackup);
    }
    assert(ret == 0);
    GLOG_IFO("using attached memory for recover...");
    for (size_t i = 0; i < shm->head.objects_count; ++i){
        const char * objname = shm->head.objects[i].name;
        dcshmobj_user_t * user = find_user(objname);
        assert(user);
        GLOG_IFO("allocate object shm :%s with recover mode (all attached)", user->name());
        ret = user->on_alloced(&shm->body.blocks[shm->head.objects[i].block_offset][0], true);
        if (ret){
            GLOG_ERR("allocated user :%s size:%zu attach init error ret:%d !", user->name(), user->size(), ret);
            return -4;
        }
    }
    if (shmbackup){
        dcshm_close(shmbackup, dcshm_path_key(shm_keypath.c_str(), 2));
    }
    return 0;
}

int     dcshmobj_pool_impl::stop(){
    if (shm_users.empty()){
        return 0;
    }
    //for all , dumps to key 2 ?
    size_t pack_sz = sizeof(dcshmobj_pool_mm_fmt);
    for (size_t i = 0; i < shm_users.size(); ++i){
        dcshmobj_user_t * user = shm_users[i];
        void * ushm_addr = shm->body.blocks[shm->head.objects[i].block_offset];
        size_t upsz = dcshmobj_pool_mm_fmt::align_size(user->pack_size(ushm_addr));
        if (upsz > 0){
            pack_sz += upsz;
        }
    }
    if (pack_sz == sizeof(dcshmobj_pool_mm_fmt)){
        return 0;
    }
    //create backup shm
    dcshmobj_pool_mm_fmt * backup_shm = (dcshmobj_pool_mm_fmt *)_dcshmobj_pool_backup_shm_open(shm_keypath.c_str(), pack_sz);
    if (!backup_shm){
        GLOG_SER("stopping shm create error alloc size:%zu", pack_sz);
        return -1;
    }
    GLOG_WAR("stopping shm create error alloc size:%zu success .", pack_sz);
    backup_shm->head.objects_count = 0;
    backup_shm->head.version = shm->head.version;
    size_t nb_allocated = 0;
    for (size_t i = 0; i < shm_users.size(); ++i){
        dcshmobj_user_t * user = shm_users[i];
        void * ushm_addr = shm->body.blocks[shm->head.objects[i].block_offset];
        size_t upblksz = dcshmobj_pool_mm_fmt::align_size(user->pack_size(ushm_addr));
        if (upblksz == 0){
            continue;
        }
        size_t nsz = upblksz;
        void * ushm = find_user_shm_addr(shm, user->name());
        if (!ushm){
            GLOG_ERR("not found user in shm name:%s", user->name());
            continue;
        }
        if (!user->pack(backup_shm->body.blocks[nb_allocated], nsz, ushm)){
            GLOG_ERR("pack shm error user in shm name:%s", user->name());
            continue;
        }
        strncpy(backup_shm->head.objects[backup_shm->head.objects_count].name, user->name(),
            sizeof(backup_shm->head.objects[0].name)-1);
        backup_shm->head.objects[backup_shm->head.objects_count].block_offset = nb_allocated;
        backup_shm->head.objects[backup_shm->head.objects_count].block_count = dcshmobj_pool_mm_fmt::size_block(upblksz);
        backup_shm->head.objects[backup_shm->head.objects_count].real_size = upblksz;
        ++backup_shm->head.objects_count;
        nb_allocated += dcshmobj_pool_mm_fmt::size_block(upblksz);
    }
    std::string str_backshm;
    GLOG_IFO("stop backup shm create finished stat:%s", backup_shm->stat_dump(str_backshm));
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
dcshmobj_pool::dcshmobj_pool(){
    impl_ = new dcshmobj_pool_impl();
}
dcshmobj_pool::~dcshmobj_pool(){
    if (impl_){
        delete impl_;
        impl_ = nullptr;
    }
}
int               dcshmobj_pool::regis(dcshmobj_user_t * user){
    return   impl_->regis(user);
}
int               dcshmobj_pool::start(const char * keypath){
    return   impl_->start(keypath);
}
int               dcshmobj_pool::stop(){
    return   impl_->stop();
}
