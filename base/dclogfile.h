#pragma  once

namespace dcs {
struct logfile_impl_t;
struct logfile_t {
    void    init(const char * file);
    int     open();
    void    close();
    int     write(const char * logmsg, int max_roll, int max_file_size);
public:
    logfile_t();
    ~logfile_t();
private:
    logfile_impl_t * impl;
};

}