//extern "C" {
#include "libbson-1.0/bson.h"
#include "libbson-1.0/bcon.h"
#include "libmongoc-1.0/mongoc.h"
//}
#include "utility_mongo.h"
#include "blocking_queue.hpp"
#include "logger.h"

NS_BEGIN(dcsutil)

///////////////////////////////////////////////////////////////
#define	MAX_MOGNO_THREAD_POOL_SIZE	(128)
#define MAX_QUEUE_REQUEST_SIZE		(4096)

struct mongo_request_t {
	mongo_client_t::commnd_t			cmd;
	mongo_client_t::on_response_t		cb;
	void							   *cb_ud;
};
struct mongo_response_t {
	mongo_client_t::on_response_t		 cb;
	void								*cb_ud;
	mongo_client_t::result_t			 result;
};
struct mongo_client_impl_t {
	mongo_client_config_t	conf;
	mongoc_client_pool_t	*pool;
	bool									stop;
	blocking_queue<mongo_request_t>			command_queue;
	blocking_queue<mongo_response_t>		result_queue;
	std::atomic<int>						running;
	std::thread								workers[MAX_MOGNO_THREAD_POOL_SIZE];
	mongo_client_impl_t() {
		pool = NULL;
		stop = false;
		running = 0;
	}
};

#define _THIS_HANDLE	((mongo_client_impl_t*)(handle))
//#define LOG_S(format, ...)	LOGSTR(_THIS_HANDLE->error_msg, "mysql", " [%d (%s)]" format,mysql_errno(_THIS_HANDLE->mysql_conn),mysql_error(_THIS_HANDLE->mysql_conn), ##__VA_ARGS__)


mongo_client_t::mongo_client_t(){
	handle = new mongo_client_impl_t();
}

mongo_client_t::~mongo_client_t(){
	if (_THIS_HANDLE){
		if (_THIS_HANDLE->pool){
			mongoc_client_pool_destroy(_THIS_HANDLE->pool);
			mongoc_cleanup();
		}
		delete _THIS_HANDLE;
	}
}
static void 
_real_excute_command(mongoc_client_t * client, mongo_response_t & rsp, const mongo_request_t & req){
	rsp.cb = req.cb;
	rsp.cb_ud = req.cb_ud;
	//////////////////////////////////////////
	bson_error_t error;
	bson_t reply;
	char *str;
	bson_t *command = bson_new_from_json((const uint8_t *)req.cmd.cmd.c_str(), req.cmd.cmd.length(), &error);// BCON_NEW(BCON_UTF8(req.cmd.cmd.c_str()));
	if (!command){
		rsp.result.err_msg = "bson_new_from_json error! :";
		rsp.result.err_msg += error.message;
		return;
	}
	bool ret = false;
	if (!req.cmd.coll.empty()){
		mongoc_collection_t * collection = mongoc_client_get_collection(client, req.cmd.db.c_str(), req.cmd.coll.c_str());
		if (!collection){
			rsp.result.err_msg = "not found collection !";
		}
		else {
			ret = mongoc_collection_command_simple(collection, command, NULL, &reply, &error);
			mongoc_collection_destroy(collection);
		}
	}
	else {
		mongoc_database_t * database = mongoc_client_get_database(client, req.cmd.db.c_str());
		if (!database){
			rsp.result.err_msg = "not found database !";
		}
		else {
			ret = mongoc_database_command_simple(database, command, NULL, &reply, &error);
			mongoc_database_destroy(database);
		}
	}
	if (ret) {
		str = bson_as_json(&reply, NULL);
		rsp.result.rst = str;
		bson_free(str);
	}
	else {
		if (error.code){
			rsp.result.err_no = error.code;
			rsp.result.err_msg = error.message;
		}
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
		if (mci->command_queue.pop(req, 5000)){
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

static void *	
_start_pool(void * data){
	mongo_client_impl_t * mci = (mongo_client_impl_t *)(data);	
	pthread_t				threads[MAX_MOGNO_THREAD_POOL_SIZE];
	for (int i = 0; i < mci->conf.multi_thread; i++) {
		pthread_create(&threads[i], NULL, _worker, mci);
	}
	for (int i = 0; i < mci->conf.multi_thread; i++) {
		pthread_join(threads[i], NULL);
	}
	return NULL;
}
int				
mongo_client_t::init(const mongo_client_config_t & conf){
	if (conf.multi_thread > MAX_MOGNO_THREAD_POOL_SIZE){
		//error thread num
		return -1;
	}
	mongoc_init();
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
	_THIS_HANDLE->conf = conf;
#if 0
	int ret = pthread_create(&_THIS_HANDLE->main_thread, NULL, _start_pool, _THIS_HANDLE);
	if (ret){
		LOGP("thread create error !");
		return -4;
	}
#else
	for (int i = 0; i < conf.multi_thread; i++) {
		_THIS_HANDLE->workers[i] = std::thread(_worker, handle);
		_THIS_HANDLE->workers[i].detach();
	}
#endif
	_THIS_HANDLE->running.fetch_add(1);
	return 0;
}

int				
mongo_client_t::excute(const commnd_t & cmd, on_response_t cb, void * ud){
	if (_THIS_HANDLE->command_queue.size() > MAX_QUEUE_REQUEST_SIZE){
		return -1;
	}
	//push a reaues
	mongo_request_t	req;
	req.cmd = cmd;
	req.cb = cb;
	req.cb_ud = ud;
	_THIS_HANDLE->command_queue.push(req);
	return 0;
}

void			
mongo_client_t::stop(){ //set stop
	_THIS_HANDLE->running.fetch_sub(1);
	_THIS_HANDLE->stop = true;
}
bool			
mongo_client_t::running(){ //is running ?
	return 	_THIS_HANDLE->running > 0;
}
int				
mongo_client_t::poll(int max_proc){//same thread cb call back
	
	//get a response
	if (_THIS_HANDLE->result_queue.empty()){
		return 0;
	}
	int nproc;
	//a cb
	mongo_response_t	rsp;
	for (nproc = 0; nproc < max_proc && !_THIS_HANDLE->result_queue.empty(); ++nproc){
		if (!_THIS_HANDLE->result_queue.pop(rsp, 1)){
			rsp.cb(rsp.cb_ud, rsp.result);
		}
	}
	return nproc;
}













NS_END()