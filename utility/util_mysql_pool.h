#include "util_mysql.h"
#include "base/msg_buffer.hpp"

NS_BEGIN(dcsutil)

struct mysqlclient_pool_t {
    struct command_t {
        std::string     sql;
        std::string     table_name;
        msg_buffer_t    cbdata;
        bool            need_result;
        command_t() :need_result(0){}
    };
    struct result_t {
        int             status;
        int             err_no;
        std::string     error;
        int             affects;
        //----------------------------------------------------
        size_t			                            row_total;
        size_t			                            row_offset;
        typedef std::pair<string, msg_buffer_t>     row_field_t;
        typedef std::vector< row_field_t >          row_t;
        typedef std::vector< row_t>                 results_t;
        results_t                                   fetched_results;
    };
    typedef void (*cb_func_t)(void *ud, const result_t & result, const command_t & cmd);
    //========================================================================
    typedef typename mysqlclient_t::cnnx_conf_t     mysqlconnx_config_t;
    int			    init(const mysqlconnx_config_t & conf, int threadnum = 0);
    int             poll(int timeout_ms = 10, int maxproc = 100);
    int			    execute(const command_t & cmd, cb_func_t cb, void * ud);
    void            stop();
    int             running();
    //========================================================================
    ~mysqlclient_pool_t();
    void            cleanup(int timeout_ms = 1000);
};



NS_END()