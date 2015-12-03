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
	struct command_t {
		string  db;
		string	coll;
		string	cmd;
		size_t	cmd_length;//if 0:strlen()
		int		flag;
		command_t() :cmd_length(0),flag(0){}
	};
	struct result_t {
		enum { RESULT_MAX_ERR_MSG_SZ = 128 };
		string	rst;
		string	err_msg;
		int		err_no;
		result_t() :err_no(0){
			err_msg.reserve(RESULT_MAX_ERR_MSG_SZ);
		}
		result_t(const result_t & rhs){
			this->operator = (rhs);
		}
		result_t & operator = (const result_t & rhs){
			if (this != &rhs){
				rst.swap(const_cast<string&>(rhs.rst));
				err_no = rhs.err_no;
				err_msg.assign(rhs.err_msg.data(), rhs.err_msg.capacity());
			}
			return *this;
		}
	};
private:
	void	*		handle;
public:
	mongo_client_t();
	~mongo_client_t();
public:
	int				init(const mongo_client_config_t & conf);
	typedef	 void(*on_result_cb_t)(void * ud,const mongo_client_t::result_t & result);
	int				excute(const command_t & cmd, on_result_cb_t cb, void * ud);

	int				insert(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);
	int				update(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);
	int				remove(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);
	int				find(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);
	int				count(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);

	int				poll(int max_proc = 100, int timeout_ms = 2);//same thread cb call back
	void			stop(); //set stop
	bool			running(); //is running ?
};


















//////////////////////////////////////////////////////////////////////
NS_END()