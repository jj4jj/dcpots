#pragma  once
#include "../../base/msg_buffer.hpp"
#include "dcmysqlc.h"

NS_BEGIN(dcsutil)

struct mysqlclient_pool_t {
    struct command_t {
        std::string     sql;
        std::string     full_msg_type_name;;
        msg_buffer_t    cbdata;
        bool            need_result;
        int64_t         opaque;
        string          src;
        bool            flatmode;
		/////////////////////////for select
		int				offset;
		int				limit;
        command_t() :need_result(0), opaque(0), flatmode(false), offset(0), limit(0){}
	};
    struct result_t {
        int             status;
        int             err_no;
        std::string     error;
        int             affects;
        //----------------------------------------------------
        typedef std::pair<string, msg_buffer_t>     row_field_t;
        typedef std::vector< row_field_t >          row_t;
        typedef std::vector< row_t>                 results_t;
        results_t                                   fetched_results;
        void    alloc_mysql_row_converted(mysqlclient_t::table_row_t & tbrow, int idx) const;
        void    free_mysql_row(mysqlclient_t::table_row_t & tbrow) const;
		result_t() { init();}
		void	init(){
			status = 0;
			affects = 0;
			err_no = 0;
			fetched_results.clear();
		}
    };
    typedef void (*cb_func_t)(void *ud, const result_t & result, const command_t & cmd);
    //========================================================================
    typedef typename mysqlclient_t::cnnx_conf_t     mysqlconnx_config_t;
    int			    init(const mysqlconnx_config_t & conf, int threadnum = 0);
    int             poll(int timeout_ms = 10, int maxproc = 100);
    int			    execute(const command_t & cmd, cb_func_t cb, void * ud);
    void            stop();
    int             running();
	mysqlclient_t*	mysql(int idx=0);
    //========================================================================
    ~mysqlclient_pool_t();
};



NS_END()