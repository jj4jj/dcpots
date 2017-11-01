#pragma  once

namespace dcs {
struct logfile_impl_t;
enum logfile_roll_order {
	//asc:.log,  .log.1 .... .log.n  -> newer [loop <.1.2..3.4.1.2.3.4.1>]
	//dsc:.log,  .log.1 .... .log.n  -> older [not loop <.1.2.3.4>]
	LOGFILE_ROLL_ASC = 0,
	LOGFILE_ROLL_DSC = 1,
};
struct logfile_t {
    int     init(const char * path, int max_roll = 10, int max_file_size = 1024*1024*20, 
                 logfile_roll_order order = LOGFILE_ROLL_DSC);
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