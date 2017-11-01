#ifndef _DC_THREAD_LOCAL_H_
#define _DC_THREAD_LOCAL_H_

namespace dcs {

template<class Tag, class TVal>
struct thread_local_storage {
    static pthread_key_t    tkey;
    static std::atomic<int> ref;
    /////////////////////////////////////////////////
    TVal * get() {
        return (TVal*)pthread_getspecific(tkey);
    }
    void  set(TVal * v) {
        pthread_setspecific(tkey, v);
    }    
    thread_local_storage(){
        //todo cas
        ++ref;
        if(ref == 1){
            pthread_key_create(&tkey, _tls_desctruct);
        }
    }
    ~thread_local_storage(){
        --ref;
        if(ref == 0){
            pthread_key_delete(tkey);
        }
    }
    static void _tls_desctruct(void * val) {
        TVal * v = (TVal*)val;
    }
};

template<class Tag, class TVal>
pthread_key_t thread_local_storage<Tag, TVal>::tkey;

template<class Tag, class TVal>
std::atomic<int> thread_local_storage<Tag, TVal>::ref(0);

};


#endif