#pragma  once
#include "blocking_queue.hpp"

template <typename InstType, typename ConfigType,
    typename InputType, typename OutputType>
struct woker_t {
    InstType    inst;
    typedef typename  ConfigType      WokerConfigType;
    typedef typename  InputType       WorkInputType;
    typedef typename  OutputType      WorkOutputType;
    int     init();
    int     proc();
    int     tick();
};

template<typename Worker>
typedef void(*work_complete_notify_t)(void * ud,
            const Worker::WorkOutputType & output, const Worker::WorkInputType & input);

template<typename WorkerType,int MAX_WORKER, int MAX_QUEUE_WORK_SIZE>
struct worker_pool_impl_t {
    struct work_transaction_t {
        typename WorkerType::WorkInputType       input;
        typename WorkerType::WorkOutputType      output;
        work_complete_notify_t<Worker>           cp_cb;
        void *                                   cb_ud;
    };
    typename WorkerType::WorkerConfigType    conf;
    int                 threadsnum;
    std::thread         threads[MAX_WORKER];
    WorkerType          workers[MAX_WORKER];
    //----------------------------------------------------------------------
    blocking_queue_t<size_t>            qrequests;
    blocking_queue_t<size_t>            qresponse;
    ////////////////////////////////////////////////
    typedef object_pool_t<work_transaction_t, MAX_QUEUE_WORK_SIZE>	tranaction_pool_t;
    tranaction_pool_t					transaction_pool;
    std::atomic<int>                    running;
    bool                                stop;
    worker_pool_impl_t(){
        threadsnum = 0;
        stop = true;
        running = 0;
    }
};

template<typename WokerType, typename WorkerConfigType, typename WorkInputType,
         typename WorkOutputType, int MAX_WORKER = 256, int MAX_QUEUE_WORK_SIZE = 1024>
class woker_pool_t {
    typedef     worker_pool_impl_t<WorkerType, WorkerConfigType, WorkInputType,
                            WorkOutputType, MAX_WORKER, MAX_QUEUE_WORK_SIZE>    impl_t;
    impl_t  * impl;
public:
    int         init(const WokerConfigType & conf, int max_worker = 0){
        if (impl->running > 0){
            GLOG_ERR("init worker pool once more !");
            return -1;
        }
        if (max_worker == 0){
            max_worker = std::thread::hardware_concurrency();
            GLOG_WAR("threadsnum config is 0 get hardware concurrency :%d", max_worker);
        }
        if (max_worker < 0){
            max_worker = 1;
            GLOG_WAR("threadsnum config is less than 0 using thread num :%d", max_worker);
        }

        impl->stop = false;
        impl->conf = conf;
        impl->threadsnum = max_worker;
        ////////////////////////////////////////////////////
        static struct worker_work_forwar_param_ {
            impl_t * impl;
            int      idx;
        } thread_param[MAX_WORKER]; //c99, var array
        for (int idx = 0; idx < max_worker; ++idx){
            WokerType & worker = impl->workers[idx];
            if (worker.init(conf)){
                GLOG_ERR("woker(idx:%d) init error !", idx);
                return -2;
            }
            GLOG_IFO("worker idx:%d create ok ...", idx);
            struct worker_work_foward_func_ {
                static void *_woker(void * impl ){
                    worker_work_forwar_param_ * param = (worker_work_forwar_param_*)impl;

                    int idx = param->idx;
                    WorkerType & woker = param->impl.workers[idx];
                    param->impl.running.fetch_add(1);
                    ////////////////////////////////////////
                    size_t transid = 0;
                    do { //take a request
                        if (param->impl.qrequests.pop(transid, 500)){
                            continue; //timeout , 5s
                        }
                        else {
                            auto pio = param->impl.transaction_pool.ptr(transid);
                            woker.proc(pio->output, pio->input);
                            param->impl.qresponse.push(transid);
                        }
                        worker.tick();
                    } while (!param->impl.stop);
                    param->impl.running.fetch_sub(1);
                    return nullptr;
                }
            };
            impl->threads[idx] = std::thread(worker_work_foward_func_::_worker, &thread_param[idx]);
            impl->threads[idx].detach();
        }
        return 0;
    }
    int         poll(int maxproc, int timeout_ms){
        if (impl->qresponse.empty()){
            return 0;
        }
        int nproc = 0;
        size_t transid = 0;
        for (nproc = 0; nproc < maxproc && !impl->qresponse.empty(); ++nproc){
            if (!impl->qresponse.pop(transid, timeout_ms)){
                work_transaction_t & trans = *impl->transaction_pool.ptr(transid);
                trans.cp_cb(trans.cb_ud, trans.output, trans.input);
                impl->transaction_pool.free(transid);
            }
        }
        return nproc;
    }
    int         push_work(const typename WorkerType::WorkInputType & input, work_complete_notify_t<WorkerType> cp_cb, void * cb_ud){
        if (impl->qrequests.size() > MAX_QUEUE_WORK_SIZE){
            GLOG_ERR("worker work queue has been reached max:%d ! please retry later ...",
                MAX_QUEUE_WORK_SIZE);
            return -1;
        }
        size_t transid = impl->transaction_pool.alloc();
        if (transid == 0){
            GLOG_ERR("transaction pool has been reached max:%zd ! please retry later ...",
                impl->transaction_pool.total());
            return -2;
        }
        work_transaction_t & trans = *impl->transaction_pool.ptr(transid);
        trans.cp_cb = cp_cb;
        trans.cb_ud = cb_ud;
        trans.input = input;
        impl->qrequests.push(transid);
        return 0;
    }
    void        stop(){
        impl->stop = true;
    }
    int         running(){
        return impl->running;
    }
public:
    woker_pool_t(){
        impl = new impl_t();
    }
    ~woker_pool_t(){
        if (impl){
            delete impl;
            impl = nullptr;
        }
    }
};






