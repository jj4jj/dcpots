#pragma  once

#include <vector>
#include <string>
#include <type_traits>


struct dcshmobj_user_t {
    virtual const char *    name() const;
    virtual size_t          size() const;
    virtual int             on_alloced(void * udata, bool attached);
    ////////////////////////////////////////////////////////////////////////////////////
    virtual size_t          pack_size(void * udata) const;
    virtual bool            pack(void * buff, size_t buff_sz, const void * udata) const;
    virtual bool            unpack(void * udata, const void * buff, size_t buff_sz);
};

template<class U>
struct dcshmobj_t : public dcshmobj_user_t {
    U             * data;
    dcshmobj_t():data(nullptr){
        static_assert(std::is_pod<U>::value, "dcobject data must be a pod struct !");
    }
    virtual const char * name() const {
        return typeid(U).name();
    }
    virtual size_t size() const {
        return sizeof(U);
    }
    virtual int on_rebuild(){
        return 0;
    }
    virtual int on_alloced(void * pv, bool attached) {
        assert(!this->data);
        if (attached){
            this->data = pv;
        }
        else {
            this->data = new(pv)U();
            return on_rebuild();
        }
        return 0;
    }
    virtual size_t          pack_size(void * udata) const {
        return static_cast<U*>(udata)->dumps_size();
    }
    virtual bool            pack(void * buff, size_t buff_sz, const void * udata) const {
        assert(udata == data);
        int ret = static_cast<U*>(udata)->dumps(buff, buff_sz);
        if (ret){
            return false;
        }
        return true;
    }
    virtual bool            unpack(void * udata, const void * buff, size_t buff_sz) {
        int ret = static_cast<U*>(udata)->loads(buff, buff_sz);
        if (ret){
            return false;
        }
        return true;
    }


};

struct dcshmobj_pool_impl;
struct dcshmobj_pool {
    int               regis(dcshmobj_user_t * user);
    int               start(const char * keypath);
    int               stop();
    /////////////////////////////////////////////////
public:
    dcshmobj_pool();
    ~dcshmobj_pool();
private:
    dcshmobj_pool_impl * impl_{ nullptr };
};
