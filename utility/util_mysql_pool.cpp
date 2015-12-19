#include "util_mysql_pool.h"
#include "base/logger.h"
#include "base/blocking_queue.hpp"

NS_BEGIN(dcsutil)


#define MAX_MYSQL_CLIENT_THREAD_NUM     (128)
#define MAX_QUEUE_REQUEST_SIZE          (10240)

struct mysql_transaction_t {
    mysqlclient_pool_t::command_t       cmd;
    mysqlclient_pool_t::cb_func_t       cb;
    void                               *cb_ud;
    mysqlclient_pool_t::result_t        result;
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
    object_pool<mysql_transaction_t>    transactions_pool;
    std::atomic<int>                    running;
    bool                                stop;
    /////////////////////////////////////////////////
    std::mutex                          lock;
    std::condition_variable             cond;

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
    UNUSED(ud);
    UNUSED(need_more);
    auto results = (mysqlclient_pool_t::result_t::results_t *)ud;   
    results->push_back(mysqlclient_pool_t::result_t::row_t());
    auto & mrow = results->back();
    for (size_t i = 0; i < tbrow.fields_count; ++i){
        mrow.push_back(mysqlclient_pool_t::result_t::row_field_t());
        auto & mfield = mrow.back();
        mfield.first = tbrow.fields_name[i];
        mfield.second.copy(tbrow.row_data[i], tbrow.row_length[i]);
    }
}
static void inline 
_process_one(mysqlclient_t & client, size_t transid){
     mysql_transaction_t & trans = *g_ctx.transactions_pool.ptr(transid);
	 trans.result.init();
     trans.result.status = client.execute(trans.cmd.sql);
	 if (trans.result.status){
         //error
         trans.result.error = client.err_msg();
         trans.result.err_no = client.err_no();
     }
     else {
         trans.result.affects = client.affects();
		 if (trans.cmd.need_result){
             client.result(&trans.result.fetched_results, result_cb_func);
         }
     }
}
static void *
_worker(void * data){
    int idx = *(int*)data;
    mysqlclient_t & client = g_ctx.clients[idx];
    //g_ctx.cond.notify_one();
    g_ctx.running.fetch_add(1);
    ////////////////////////////////////////
    size_t transid = 0;
    do {
        //take a request
        if (g_ctx.qrequests.pop(transid, 500)){
            continue; //timeout , 5s
        }
        else {
            //real command excute
            _process_one(client, transid);
            g_ctx.qresponse.push(transid);
        }
        client.ping();
        //push result
    } while (!g_ctx.stop);
    //push client
    g_ctx.running.fetch_sub(1);
    return nullptr;
}

int			    
mysqlclient_pool_t::init(const mysqlclient_t::cnnx_conf_t & conf, int threadsnum){
    if (threadsnum == 0){
        threadsnum = std::thread::hardware_concurrency() * 2;
        GLOG_WAR("threadsnum config is 0 get hardware concurrency :%d", threadsnum);
    }
    if (threadsnum < 0){
        threadsnum = 1;
        GLOG_WAR("threadsnum config is less than 0 using thread num :%d", threadsnum);
    }
    g_ctx.stop = false;
    g_ctx.mconf = conf;
    g_ctx.mconf.multithread = true;
    g_ctx.threadsnum = threadsnum;
    for (int idx = 0; idx < threadsnum; ++idx){
        mysqlclient_t & client = g_ctx.clients[idx];
        if (client.init(g_ctx.mconf)){
            GLOG_ERR("mysql client(idx:%d) init error ! error (%d:%s) config ip:%s config port:%d",
                idx, client.err_no(), client.err_msg(),
                g_ctx.mconf.ip.c_str(), g_ctx.mconf.port);
            return -1;
        }
        GLOG_IFO("mysql client idx:%d create ok ...", idx);
        ////////////////////////////////////////////////////
        g_ctx.threads[idx] = std::thread(_worker, &idx);
        g_ctx.threads[idx].detach();
    }
	return 0;
}
int
mysqlclient_pool_t::poll(int timeout_ms, int maxproc){
    //get a response
    if (g_ctx.qresponse.empty()){
        return 0;
    }
    int nproc = 0;
    //a cb
    size_t transid = 0;
    for (nproc = 0; nproc < maxproc && !g_ctx.qresponse.empty(); ++nproc){
        if (!g_ctx.qresponse.pop(transid, timeout_ms)){
            mysql_transaction_t & trans = *g_ctx.transactions_pool.ptr(transid);
            trans.cb(trans.cb_ud, trans.result, trans.cmd);
            g_ctx.transactions_pool.free(transid);
        }
    }
    return nproc;
}

mysqlclient_t*
mysqlclient_pool_t::mysql(int i){
	if (g_ctx.threadsnum > 0 && i < g_ctx.threadsnum){
		return &g_ctx.clients[i];
	}
	return nullptr;
}

int
mysqlclient_pool_t::execute(const command_t & cmd, mysqlclient_pool_t::cb_func_t cb, void * ud){
    if (g_ctx.qrequests.size() > MAX_QUEUE_REQUEST_SIZE){
        return -1;
    }
    size_t transid = g_ctx.transactions_pool.alloc();
    mysql_transaction_t & trans = *g_ctx.transactions_pool.ptr(transid);
    trans.cb = cb;
    trans.cb_ud = ud;
    trans.cmd = cmd;
    g_ctx.qrequests.push(transid);
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
        timeout_ms -= 10;
    }
}


NS_END()
