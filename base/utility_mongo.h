#pragma once
#include "stdinc.h"
NS_BEGIN(dcsutil)
//////////////////////////////////////////////////////////////////////
struct mongo_client_config_t {
	string	mongo_uri;//mongodb://localhost:27017
	int		multi_thread;//multi-thread: default is 1 //single thread	
	mongo_client_config_t() :multi_thread(1){}
};

struct mongo_client_t {
	struct commnd_t {
		string  db;
		string	coll;
		string	cmd;
		int		flag;
		commnd_t() :flag(0){}
	};
	struct result_t {
		enum { RESULT_MAX_ERR_MSG_SZ = 64 };
		string	rst;
		string	err_msg;
		int		err_no;
		result_t() :err_no(0){
			err_msg.reserve(RESULT_MAX_ERR_MSG_SZ);
		}
	};
private:
	void	*		handle;
public:
	mongo_client_t();
	~mongo_client_t();
public:
	int				init(const mongo_client_config_t & conf);
	typedef	 void(*on_response_t)(void * ud,const mongo_client_t::result_t & result);
	int				excute(const commnd_t & cmd, on_response_t cb, void * ud);
	int				poll(int max_proc = 100);//same thread cb call back
	void			stop(); //set stop
	bool			running(); //is running ?
};


















//////////////////////////////////////////////////////////////////////
NS_END()