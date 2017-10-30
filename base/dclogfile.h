#pragma  once

namespace dcs {
struct logfile_impl_t;
enum logfile_roll_order {
	//asc:.log,  .log.1 .... .log.n  -> newer [loop <.1.2..3.4.1.2.3.4.1>]
	//dsc:.log,  .log.1 .... .log.n  -> older [not loop <.1.2.3.4>]
	LOGFILE_ROLL_ASC = 0,
	LOGFILE_ROLL_DSC = 1,
};
enum logfile_type {
    LOGFILE_TYPE_ROLL,
    LOGFILE_TYPE_NET,
};
struct logfile_t {
    int     init(const char * file, int max_roll, int max_file_size, 
                 logfile_roll_order order = LOGFILE_ROLL_ASC,
                 logfile_type type = LOGFILE_TYPE_ROLL);
    int     open();
    void    close();
    int     write(const char * logmsg);
public:
    logfile_t();
    ~logfile_t();
private:
    logfile_impl_t * impl;
};

}