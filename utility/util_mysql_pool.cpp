#include "util_mysql_pool.h"
#include "base/logger.h"
#include "base/blocking_queue.hpp"

NS_BEGIN(dcsutil)


#define MAX_MYSQL_CLIENT_THREAD_NUM     (128)
#define MAX_QUEUE_REQUEST_SIZE          (10240)

struct mysqlclient_request_t {
    mysqlclient_pool_t::command_t       cmd;
    mysqlclient_pool_t::cb_func_t       cb;
    void                               *cb_ud;
};

struct mysqlclient_response_t {
    size_t      reqid;
    mysqlclient_pool_t::result_t    result;
};


static struct {
    mysqlclient_t::cnnx_conf_t          mconf;
    int                                 threadsnum;
    std::thread                         threads[MAX_MYSQL_CLIENT_THREAD_NUM];
    mysqlclient_t                       clients[MAX_MYSQL_CLIENT_THREAD_NUM];
    //----------------------------------------------------------------------
    blocking_queue<size_t>              qrequests;
    blocking_queue<size_t>              qresponse;
    ////////////////////////////////////////////////
    object_pool<mysqlclient_request_t>  request_pool;
    object_pool<mysqlclient_response_t> response_pool;
    std::mutex                          lock;
    std::atomic<int>                    running;
    bool                                stop;
    /////////////////////////////////////////////////

} g_ctx;


void   mysqlclient_pool_t::result_t::free_mysql_row(mysqlclient_t::table_row_t & tbrow)  const {
    if (tbrow.fields_name){
        free(tbrow.fields_name);
    }
    if (tbrow.row_data){
        free(tbrow.row_data);
    }
    if (tbrow.row_length){
        free(tbrow.row_length);
    }
    memset(&tbrow, 0, sizeof(tbrow));
}
void    mysqlclient_pool_t::result_t::alloc_mysql_row_converted(mysqlclient_t::table_row_t & tbrow, int idx) const {
    auto & vr = fetched_results.at(idx);
    memset(&tbrow, 0, sizeof(tbrow));
    tbrow.fields_count = vr.size();
    tbrow.fields_name = (const char**)malloc(sizeof(char*) * tbrow.fields_count);
    tbrow.row_data = (const char**)malloc(sizeof(char*)* tbrow.fields_count);
    tbrow.row_length = (size_t*)malloc(sizeof(size_t)* tbrow.fields_count);
    for (size_t i = 0; i < tbrow.fields_count; ++i){
        tbrow.fields_name[i] = vr.at(i).first.data();
        tbrow.row_data[i] = vr.at(i).second.buffer;
        tbrow.row_length[i] = vr.at(i).second.valid_size;
    }
}


static void	
result_cb_func(void* ud, OUT bool & need_more, const mysqlclient_t::table_row_t & tbrow){
    auto results = (mysqlclient_pool_t::result_t::results_t *)ud;   
    const char * *	fields_name;
    const char * *  row_data;
    size_t	*		row_length;
    results->push_back(mysqlclient_pool_t::result_t::row_t());
    auto & mrow = results->back();
    for (int i = 0; i < tbrow.fields_count; ++i){
        mrow.push_back(mysqlclient_pool_t::result_t::row_field_t());
        auto & mfield = mrow.back();
        mfield.first = tbrow.fields_name[i];
        mfield.second.copy(tbrow.row_data[i], tbrow.row_length[i]);
    }
}
static void inline 
_process_one(mysqlclient_t & client, size_t reqid){
     size_t rspid =  g_ctx.response_pool.alloc(g_ctx.lock);
     mysqlclient_response_t & rsp = *g_ctx.response_pool.ptr(rspid);
     mysqlclient_request_t  & req = *g_ctx.request_pool.ptr(reqid);
     rsp.reqid = reqid;
     rsp.result.status = client.execute(req.cmd.sql);
     if (rsp.result.status){
         //error
         rsp.result.error = client.err_msg();
         rsp.result.err_no = client.err_no();
     }
     else {
         rsp.result.affects = client.affects();
         if (req.cmd.need_result){
             client.result(&rsp.result.fetched_results, result_cb_func);
         }
     }

}
static void *
_worker(void * data){
    int idx = *(int*)data;
    mysqlclient_t & client = g_ctx.clients[idx];
    g_ctx.running.fetch_add(1);
    size_t reqid = 0;
    do {
        //take a request
        if (g_ctx.qrequests.pop(reqid, 500)){
            continue; //timeout , 5s
        }
        else {
            //real command excute
            _process_one(client, reqid);
        }
        client.ping();
        //push result
    } while (!g_ctx.stop);
    //push client
    g_ctx.running.fetch_sub(1);
    return NULL;
}

int			    
mysqlclient_pool_t::init(const mysqlclient_t::cnnx_conf_t & conf, int threadsnum){
    if (threadsnum == 0){
        threadsnum = std::thread::hardware_concurrency()*2;
        GLOG_WAR("threadsnum config is 0 get hardware concurrency :%d", threadsnum);
    }
    if (threadsnum < 0){
        threadsnum = 1;
        GLOG_WAR("threadsnum config is less than 0 using thread num :%d", threadsnum);
    }

    g_ctx.stop = false;
    for (int i = 0; i < threadsnum; ++i){
        if (g_ctx.clients[i].init(conf)){
            GLOG_ERR("mysql client init error ! %d:%s",
                g_ctx.clients[i].err_no(), g_ctx.clients->err_msg());
            return -1;
        }
    }
    for (int i = 0; i < threadsnum; ++i){
        g_ctx.threads[i] = std::thread(_worker, &i);
        g_ctx.threads[i].detach();
    }

}
int
mysqlclient_pool_t::poll(int timeout_ms, int maxproc){
    //get a response
    if (g_ctx.qresponse.empty()){
        return 0;
    }
    int nproc = 0;
    //a cb
    size_t rspid = 0;
    for (nproc = 0; nproc < maxproc && !g_ctx.qresponse.empty(); ++nproc){
        if (!g_ctx.qresponse.pop(rspid, timeout_ms)){
            mysqlclient_response_t & rsp = *g_ctx.response_pool.ptr(rspid);
            mysqlclient_request_t & req = *g_ctx.request_pool.ptr(rsp.reqid);
            req.cb(req.cb_ud, rsp.result, req.cmd);
            //////////////////////////////////////////////////////////////////////
            g_ctx.request_pool.free(rsp.reqid);
            g_ctx.response_pool.free(rspid);
        }
    }
    return nproc;
}

void *          
mysqlclient_pool_t::mysqlhandle(){
    if (g_ctx.threadsnum > 0){
        return g_ctx.clients[0].mysql_handle();
    }
    return nullptr;
}

int
mysqlclient_pool_t::execute(const command_t & cmd, mysqlclient_pool_t::cb_func_t cb, void * ud){
    if (g_ctx.qrequests.size() > MAX_QUEUE_REQUEST_SIZE){
        return -1;
    }
    size_t	reqid = g_ctx.request_pool.alloc();
    mysqlclient_request_t & req = *g_ctx.request_pool.ptr(reqid);
    req.cmd = cmd;
    req.cb = cb;
    req.cb_ud = ud;
    g_ctx.qrequests.push(reqid);
    return 0;
}

void            
mysqlclient_pool_t::stop(){
    g_ctx.stop = true;
}

int
mysqlclient_pool_t::running(){
    return g_ctx.running;
}
//========================================================================
mysqlclient_pool_t::~mysqlclient_pool_t(){
    stop();
    int timeout_ms = 1000;
    while (running() > 0 && timeout_ms > 0){
        usleep(10);
        timeout_ms - 10;
    }
}


NS_END()
