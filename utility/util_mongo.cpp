//extern "C" {
#include "libbson-1.0/bson.h"
#include "libbson-1.0/bcon.h"
#include "libmongoc-1.0/mongoc.h"
//}
#include "util_mongo.h"
#include "base/blocking_queue.hpp"
#include "base/logger.h"

NS_BEGIN(dcsutil)

///////////////////////////////////////////////////////////////
#define	MAX_MOGNO_THREAD_POOL_SIZE	(128)
#define MAX_QUEUE_REQUEST_SIZE		(4096)
#define MAX_MONGO_CMD_SIZE			(512*1024)		//512K

struct mongo_request_t {
    mongo_client_t::command_t			cmd;
	mongo_client_t::on_result_cb_t		cb;
	void							   *cb_ud;
};
struct mongo_response_t {
    size_t                               reqid;
	mongo_client_t::result_t			 result;
};
struct mongo_client_impl_t {
	mongo_client_config_t	conf;
	bool					stop;
	mongoc_client_t			*client;
	//////////////////////////////////////////////////////
	mongoc_client_pool_t	*pool;
	blocking_queue<size_t>			        command_queue;
    blocking_queue<size_t>		            result_queue;
	std::atomic<int>						running;
	std::thread								workers[MAX_MOGNO_THREAD_POOL_SIZE];
    object_pool<mongo_request_t>            request_pool;
    object_pool<mongo_response_t>           response_pool;

	mongo_client_impl_t() {
		pool = NULL;
		stop = false;
		running = 0;
		client = nullptr;
	}
};

#define _THIS_HANDLE	((mongo_client_impl_t*)(handle))

#define LOG_S_E(str, format, ...)	LOGRSTR((str), "mongo", "[%d@%d(%s)]", error.code, error.domain, error.message);GLOG_ERR("[%d@%d(%s)]" format, error.code, error.domain, error.message, ##__VA_ARGS__)
#define LOG_S(str, format, ...)		LOGRSTR((str), "mongo", format, ##__VA_ARGS__);GLOG_ERR(format, ##__VA_ARGS__)

mongo_client_t::mongo_client_t(){
	handle = new mongo_client_impl_t();
}

mongo_client_t::~mongo_client_t(){
	if (_THIS_HANDLE){
		if (_THIS_HANDLE->pool){
			mongoc_client_pool_destroy(_THIS_HANDLE->pool);
			mongoc_cleanup();
		}
		if (_THIS_HANDLE->client){
			mongoc_client_destroy(_THIS_HANDLE->client);
			mongoc_cleanup();
		}
		delete _THIS_HANDLE;
	}
}

enum MongoOptFlag {
    MONGO_OPT_CMD = 0,
    MONGO_OPT_FIND = 1
};
#define MONGO_OPT_PARAM_SEP  ("\r\n")
static void 
_real_excute_command(mongoc_client_t * client, mongo_client_t::result_t & result, const mongo_client_t::command_t & command){
	bson_error_t error;
	result.err_no = 0;
	result.err_msg[0] = 0;
	result.rst[0] = 0;   
    if (command.op == MONGO_OPT_CMD){
        bson_t reply;
        bson_t *bscommand = bson_new_from_json((const uint8_t *)command.cmd_data.data(), command.cmd_length, &error);// BCON_NEW(BCON_UTF8(command.cmd.c_str()));
        if (!bscommand){
            result.err_no = error.code;
            LOG_S_E(result.err_msg, "bson_new_from_json error!");
            return;
        }
        bool ret = false;
        if (!command.coll.empty()){
            mongoc_collection_t * collection = mongoc_client_get_collection(client, command.db.c_str(), command.coll.c_str());
            ret = mongoc_collection_command_simple(collection, bscommand, NULL, &reply, &error);
            mongoc_collection_destroy(collection);
        }
        else {
            mongoc_database_t * database = mongoc_client_get_database(client, command.db.c_str());
            ret = mongoc_database_command_simple(database, bscommand, NULL, &reply, &error);
            mongoc_database_destroy(database);
        }
        if (ret) {
            char *str = bson_as_json(&reply, NULL);
            result.rst = str;
            bson_free(str);
        }
        else {
            result.err_no = error.code;
            LOG_S_E(result.err_msg, "excute command:%s error !", command.cmd_data.c_str());
        }
        bson_destroy(bscommand);
        bson_destroy(&reply);
    }
    else if (command.op == MONGO_OPT_FIND){
        //query, order, fields, skip, limit
        string query, order, fields, skip, limit;
        strsunpack(command.cmd_data.data(), MONGO_OPT_PARAM_SEP, "query,order,fields,skip,limit",
            &query, &order, &fields, &skip, &limit);
        string jsoncmd = "{ \"$query\":" + query;
        if (!order.empty()){
            jsoncmd += ", \"$orderby\":" + order;
        }
        jsoncmd += "}";
        int iskip = std::stoi(skip);
        int ilimit = std::stoi(limit);

        bson_t *bsquery = bson_new_from_json((const uint8_t *)jsoncmd.data(), jsoncmd.length(), &error);
        if (!bsquery){
            result.err_no = error.code;
            LOG_S_E(result.err_msg, "query bson_new_from_json error!");
            return;
        }
        bson_t * bsfieldsquery = NULL;
        if (!fields.empty()){
            bsfieldsquery = bson_new_from_json((const uint8_t *)fields.data(), fields.length(), &error);
            if (!bsfieldsquery){
                result.err_no = error.code;
                LOG_S_E(result.err_msg, "fields bson_new_from_json error!");
                bson_destroy(bsquery);
                return;
            }
        }

        //-----------------------------------------------------------------------------
        const bson_t *doc;
        mongoc_collection_t * collection = mongoc_client_get_collection(client, command.db.c_str(), command.coll.c_str());
        if (!collection){
            result.err_no = -1;
            LOG_S(result.err_msg, "get collection error db:%s coll:%s", command.db.c_str(), command.coll.c_str());
            return;
        }
        mongoc_cursor_t * cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, iskip, ilimit, 0, bsquery, bsfieldsquery, NULL);
        result.rst = "{\"values\": [";
        int count = 0;
        while (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &doc)) {
            char *str = bson_as_json(doc, NULL);
            if (count != 0){
                result.rst.append(",");
            }
            result.rst.append(str);
            /////////////////////////////////
            ++count;
            GLOG_TRA("get result %s\n", str);
            bson_free(str);
        }
        result.rst.append("]}");
        if (mongoc_cursor_error(cursor, &error)) {
			result.err_no = error.code;
			LOG_S_E(result.err_msg, "cursor error occurred!");
        }
        mongoc_cursor_destroy(cursor);
        bson_destroy(bsquery);
        if (bsfieldsquery){
            bson_destroy(bsfieldsquery);
        }
    }
}
static inline void 
process_one(mongoc_client_t *client, mongo_client_impl_t *mci, size_t reqid){
    size_t rspid = mci->response_pool.alloc();
    mongo_response_t & rsp = *(mci->response_pool.ptr(rspid));
    mongo_request_t & req = *(mci->request_pool.ptr(reqid));
    rsp.reqid = reqid;
    _real_excute_command(client, rsp.result, req.cmd);
    mci->result_queue.push(rspid);
}
static void * 
_worker(void * data){
	mongo_client_impl_t * mci = (mongo_client_impl_t *)(data);
	//get client
	mongoc_client_t *client = mongoc_client_pool_pop(mci->pool);
	if (!client){
		return NULL;
	}
	mci->running.fetch_add(1);
    size_t reqid = 0;
    do {
        //take a request
        if (mci->command_queue.pop(reqid, 500)){
			continue; //timeout , 5s
		}
		else {
			//real command excute
            process_one(client, mci, reqid);
		}
		//push result
	} while (!mci->stop);
	//push client
	mongoc_client_pool_push(mci->pool, client);
	mci->running.fetch_sub(1);
	return NULL;
}
static inline int
init_multithread(void * handle, const mongo_client_config_t & conf){
	mongoc_uri_t * uri = mongoc_uri_new(conf.mongo_uri.c_str());
	if (!uri){
		return -2;
	}
	mongoc_client_pool_t * pool = mongoc_client_pool_new(uri);
	mongoc_uri_destroy(uri);
	if (!pool){
		return -3;
	}
	//set poll size:no need
	//mongoc_client_pool_max_size(mongoc_client_pool_t *pool, conf.multi_thread);
	_THIS_HANDLE->pool = pool;
	for (int i = 0; i < conf.multi_thread; i++) {
		_THIS_HANDLE->workers[i] = std::thread(_worker, handle);
		_THIS_HANDLE->workers[i].detach();
	}
	return 0;
}
static inline int
init_singlethread(void * handle, const mongo_client_config_t & conf){
	mongoc_client_t * client = mongoc_client_new(conf.mongo_uri.c_str());
	if (!client){
		std::cerr << "mongo client create error !" << std::endl;
		return -1;
	}
	_THIS_HANDLE->client = client;
	return 0;
}
int				
mongo_client_t::init(const mongo_client_config_t & conf){
	if (conf.multi_thread > MAX_MOGNO_THREAD_POOL_SIZE){
		//error thread num
		std::cerr << "thread number is too much !" << std::endl;
		return -1;
	}
	_THIS_HANDLE->conf = conf;
	////////////////////////////////////////
	mongoc_init();
	if (_THIS_HANDLE->conf.multi_thread < 0){
		_THIS_HANDLE->conf.multi_thread = std::thread::hardware_concurrency();
	}
	int ret = 0;
	if (_THIS_HANDLE->conf.multi_thread > 0){
		ret = init_multithread(handle, _THIS_HANDLE->conf);
	}
	else {
		ret = init_singlethread(handle, _THIS_HANDLE->conf);
	}
	if (ret){
		return ret;
	}
	_THIS_HANDLE->running.fetch_add(1);
	return 0;
}

int		
mongo_client_t::command(const string & db, const string & coll,
					on_result_cb_t cb, void * ud, int op, const char * cmd_fmt, ...){
	if (_THIS_HANDLE->command_queue.size() > MAX_QUEUE_REQUEST_SIZE){
		return -1;
	}
	size_t	reqid = _THIS_HANDLE->request_pool.alloc();
    mongo_request_t & req = *_THIS_HANDLE->request_pool.ptr(reqid);
	req.cb = cb;
	req.cb_ud = ud;
    req.cmd.op = op;
    req.cmd.db = db;
    req.cmd.coll = coll;
    /////////////////////////////////////////////////////////////////////////////////////////
    va_list ap;
	va_start(ap, cmd_fmt);
    req.cmd.cmd_length = vstrprintf(req.cmd.cmd_data, cmd_fmt, ap);
	va_end(ap);
    GLOG_TRA("mongo client excute command:(%s) op:%d", req.cmd.cmd_data.c_str(), req.cmd.op);
	/////////////////////////////////////////////////////////////////////////////////////////
	if (_THIS_HANDLE->client){
        process_one(_THIS_HANDLE->client, _THIS_HANDLE, reqid);
    }
	else {
		_THIS_HANDLE->command_queue.push(reqid);
	}
	return 0;
}

int				
mongo_client_t::insert(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud){
	return command(db, coll, cb, ud, 0, "{\"insert\": \"%s\",\"documents\": [%s]}",
		coll.c_str(), jsonmsg.c_str());
}
int				
mongo_client_t::update(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud){
	return command(db, coll, cb, ud, 0,
	"{\"update\": \"%s\",\"updates\": [%s]}",
	coll.c_str(), jsonmsg.c_str());
}
int				
mongo_client_t::remove(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud){
	return command(db, coll, cb, ud, 0,
		"{\"delete\": \"%s\",\"deletes\": [%s]}",
		coll.c_str(), jsonmsg.c_str());
}
int
mongo_client_t::find(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud,
    const char * projection, const char * sort, int skip , int limit ){
    string findcmd;
    string sskip = std::to_string(skip), slimit = std::to_string(limit);
    strspack(findcmd, MONGO_OPT_PARAM_SEP, "query,fields,skip,limit,order",
        &jsonmsg, &projection, &sskip, &slimit, &sort);
    return command(db, coll, cb, ud, MONGO_OPT_FIND, findcmd.c_str());
}
int				
mongo_client_t::count(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud){
	return command(db, coll, cb, ud, 0,
		"{\"count\": \"%s\", \"query\": %s}",
		coll.c_str(), jsonmsg.c_str());
}

void			
mongo_client_t::stop(){ //set stop
	if (!_THIS_HANDLE->stop){
		_THIS_HANDLE->running.fetch_sub(1);
		_THIS_HANDLE->stop = true;
	}
}
bool			
mongo_client_t::running(){ //is running ?
	return 	_THIS_HANDLE->running > 0;
}
int				
mongo_client_t::poll(int max_proc, int timeout_ms){//same thread cb call back	
	//get a response
	if (_THIS_HANDLE->result_queue.empty()){
		return 0;
	}
    int nproc = 0;
	//a cb
    size_t rspid = 0;
	for (nproc = 0; nproc < max_proc && !_THIS_HANDLE->result_queue.empty(); ++nproc){
		if (!_THIS_HANDLE->result_queue.pop(rspid, timeout_ms)){           
            mongo_response_t & rsp = *_THIS_HANDLE->response_pool.ptr(rspid);
            mongo_request_t & req = *_THIS_HANDLE->request_pool.ptr(rsp.reqid);
            req.cb(req.cb_ud, rsp.result, req.cmd);
            //////////////////////////////////////////////////////////////////////
            _THIS_HANDLE->request_pool.free(rsp.reqid);
            _THIS_HANDLE->response_pool.free(rspid);
		}
	}
	return nproc;
}

NS_END()