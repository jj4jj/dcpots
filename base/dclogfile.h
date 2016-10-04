#pragma  once

namespace dcs {
struct logfile_impl_t;
enum logfile_roll_order {
	//asc:.log,  .log.1 .... .log.n  -> newer
	//dsc:.log,  .log.1 .... .log.n  -> older
	LOGFILE_ROLL_ASC = 0,
	LOGFILE_ROLL_DSC = 1,
};
struct logfile_t {
    void    init(const char * file, int max_roll, int max_file_size, logfile_roll_order order = LOGFILE_ROLL_ASC);
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