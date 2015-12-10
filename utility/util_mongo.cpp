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


struct mongo_command_t {
    string  db;
    string	coll;
    string	cmd;
    size_t  length;
    int		flag;
    mongo_command_t() :length(0), flag(0){}
    mongo_command_t(const mongo_command_t & rhs){
        this->operator=(rhs);
    }
    mongo_command_t & operator = (const mongo_command_t & rhs){
        if (this != &rhs){
            this->db.swap(const_cast<string&>(rhs.db));
            this->coll.swap(const_cast<string&>(rhs.coll));
            this->cmd.assign(rhs.cmd.data(), rhs.cmd.length());
            //this->cmd.swap(const_cast<string&>(rhs.cmd));
            this->flag = rhs.flag;
        }
    }
};

struct mongo_request_t {
	mongo_command_t						cmd;
	mongo_client_t::on_result_cb_t		cb;
	void							   *cb_ud;
};
struct mongo_response_t {
	mongo_client_t::on_result_cb_t		 cb;
	void								*cb_ud;
	mongo_client_t::result_t			 result;
};
struct mongo_client_impl_t {
	mongo_client_config_t	conf;
	bool					stop;
	mongoc_client_t			*client;
	//////////////////////////////////////////////////////
	mongoc_client_pool_t	*pool;
	blocking_queue<mongo_request_t>			command_queue;
	blocking_queue<mongo_response_t>		result_queue;
	std::atomic<int>						running;
	std::thread								workers[MAX_MOGNO_THREAD_POOL_SIZE];
	mongo_client_impl_t() {
		pool = NULL;
		stop = false;
		running = 0;
		client = nullptr;
	}
};

#define _THIS_HANDLE	((mongo_client_impl_t*)(handle))
#define LOG_S_E(str, format, ...)	LOGRSTR((str), "mongo", " [%d@%d (%s)] " format, error.code, error.domain, error.message, ##__VA_ARGS__)
#define LOG_S(str, format, ...)		LOGRSTR((str), "mongo", format, ##__VA_ARGS__)

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
static void 
_real_excute_command(mongoc_client_t * client, mongo_response_t & rsp, const mongo_request_t & req){
	rsp.cb = req.cb;
	rsp.cb_ud = req.cb_ud;
	rsp.result.db = req.cmd.db;
	rsp.result.coll = req.cmd.coll;
	//////////////////////////////////////////
	bson_error_t error;
	bson_t reply;
	rsp.result.err_no = 0;
	rsp.result.err_msg[0] = 0;
	rsp.result.rst[0] = 0;
	bson_t *command = bson_new_from_json((const uint8_t *)req.cmd.cmd.data(), req.cmd.length, &error);// BCON_NEW(BCON_UTF8(req.cmd.cmd.c_str()));
	if (!command){
		rsp.result.err_no = error.code;
		LOG_S_E(rsp.result.err_msg, "bson_new_from_json error!");
		return;
	}
	bool ret = false;
	if (!req.cmd.coll.empty()){
		mongoc_collection_t * collection = mongoc_client_get_collection(client, req.cmd.db.c_str(), req.cmd.coll.c_str());
		ret = mongoc_collection_command_simple(collection, command, NULL, &reply, &error);
		mongoc_collection_destroy(collection);
	}
	else {
		mongoc_database_t * database = mongoc_client_get_database(client, req.cmd.db.c_str());
		ret = mongoc_database_command_simple(database, command, NULL, &reply, &error);
		mongoc_database_destroy(database);
	}
	if (ret) {
		char *str = bson_as_json(&reply, NULL);
		rsp.result.rst = str;
        GLOG_TRA("rsp result:%s", str);
		bson_free(str);
	}
	else {
		rsp.result.err_no = error.code;
		LOG_S_E(rsp.result.err_msg, "excute command:%s error !", req.cmd.cmd.c_str());
	}
	bson_destroy(command);
	bson_destroy(&reply);
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
	mongo_request_t req;
	mongo_response_t rsp;
	do {
		//take a request
		if (mci->command_queue.pop(req, 500)){
			continue; //timeout , 5s
		}
		else {
			//real command excute
			_real_excute_command(client, rsp, req);
            mci->result_queue.push(rsp);
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
					on_result_cb_t cb, void * ud, int flag, const char * cmd_fmt, ...){
	if (_THIS_HANDLE->command_queue.size() > MAX_QUEUE_REQUEST_SIZE){
		return -1;
	}
	mongo_request_t	req;
	req.cb = cb;
	req.cb_ud = ud;

	mongo_command_t & command = req.cmd;
	command.flag = flag;
	command.db = db;
	command.coll = coll;
	command.cmd.reserve(MAX_MONGO_CMD_SIZE);

	va_list ap;
	va_start(ap, cmd_fmt);
	command.length = vstrprintf(command.cmd, cmd_fmt, ap);
	va_end(ap);
	GLOG_TRA("excute command json:%s", command.cmd.c_str());
	////////////////////////////////////////////////////////
	if (_THIS_HANDLE->client){
		mongo_response_t rsp;
		_real_excute_command(_THIS_HANDLE->client, rsp, req);
	}
	else {
		_THIS_HANDLE->command_queue.push(req);
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
mongo_client_t::find(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud){
	return command(db, coll, cb, ud, 0,
		"{\"findAndModify\": \"%s\",\"query\": %s, \"update\": false}",
		coll.c_str(), jsonmsg.c_str());
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
	int nproc;
	//a cb
	mongo_response_t	rsp;
	for (nproc = 0; nproc < max_proc && !_THIS_HANDLE->result_queue.empty(); ++nproc){
		if (!_THIS_HANDLE->result_queue.pop(rsp, timeout_ms)){
            rsp.cb(rsp.cb_ud, rsp.result);
		}
	}
	return nproc;
}













NS_END()