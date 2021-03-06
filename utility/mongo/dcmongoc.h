#pragma once
#include "../../base/stdinc.h"
NS_BEGIN(dcs)
//////////////////////////////////////////////////////////////////////
struct mongo_client_config_t {
	string	mongo_uri;//mongodb://localhost:27017
	int		multi_thread;//multi-thread: default is 1 //single thread	
	mongo_client_config_t() :multi_thread(1){}
};

struct mongo_client_t {
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
    struct command_t {
        enum { MAX_COMMAND_LENGTH = 1024 * 1024};
        string  db;
        string	coll;
        string	cmd_data;
        size_t  cmd_length;
        int		op;
        string  cb_data;
        int     cb_size;
        command_t() :cmd_length(0), op(0), cb_size(0){
            cmd_data.reserve(MAX_COMMAND_LENGTH);
        }
        command_t(const command_t & rhs){
            this->operator=(rhs);
        }
        command_t & operator = (const command_t & rhs){
            if (this != &rhs){
                this->db.swap(const_cast<string&>(rhs.db));
                this->coll.swap(const_cast<string&>(rhs.coll));
                this->op = rhs.op;
                this->cmd_length = rhs.cmd_length;
                this->cmd_data.assign(rhs.cmd_data.data(), rhs.cmd_data.capacity()); //max capa
                this->cb_size = rhs.cb_size;
                this->cb_data.swap(const_cast<string&>(rhs.cb_data));
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
    typedef	 void(*on_result_cb_t)(void * ud, const mongo_client_t::result_t & result, const mongo_client_t::command_t & command);
	int				command(const string & db, const string & coll,
							on_result_cb_t cb, void * ud, int op, const char * cmd_fmt, ...);
	int				insert(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);
	int				update(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);
	int				remove(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);
    int				find(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud,
        const char * projection = nullptr, const char * sort = nullptr, int skip = 0, int limit = 0);
	int				count(const string & db, const string & coll, const string & jsonmsg, on_result_cb_t cb, void * ud);

	int				poll(int max_proc = 100, int timeout_ms = 2);//same thread cb call back
	void			stop(); //set stop
	bool			running(); //is running ?
};


















//////////////////////////////////////////////////////////////////////
NS_END()
