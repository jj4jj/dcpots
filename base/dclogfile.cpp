
#include "dcutils.hpp"
#include "dclogfile.h"

NS_BEGIN(dcs)

struct logfile_impl_t {
    FILE	*	pf{ nullptr };
    string		logfile;
	int			max_roll {10};
	int			max_file_size {1024*1024*20};
	logfile_roll_order order {LOGFILE_ROLL_ASC};
	int			next_rollid { 0 };
};
logfile_t::logfile_t(){
    impl = new logfile_impl_t();
}
logfile_t::~logfile_t(){
    close();
    if (impl){
        delete impl;
    }
}
void logfile_t::init(const char * file, int max_roll, int max_file_size, logfile_roll_order order){
    if (file && *file){
		impl->logfile = file;
		impl->max_roll = max_roll;
		impl->max_file_size = max_file_size;
		impl->order = order;
	}
}
static inline void _init_open_logfile(logfile_impl_t * impl) {
	if(impl->order == LOGFILE_ROLL_ASC){
		if (impl->next_rollid == 0) { //init , get the next roll id
			uint32_t last_modify_time = 0, mt = 0;
			impl->next_rollid = 1;
			std::string file_name;
			for (int i = 1; i <= impl->max_roll; ++i) {
				file_name = impl->logfile + "." + std::to_string(i);
				mt = dcs::file_modify_time(file_name);
				if (mt > last_modify_time) {
					last_modify_time = mt;
					impl->next_rollid = (i%impl->max_roll) + 1;
				}
			}
		}
		else {
			fseek(impl->pf, 0, SEEK_SET);
			ftruncate(fileno(impl->pf), 0);
		}
	}
	else {
		//nothing need do in this way
	}
}
static inline void _shift_logfile(logfile_impl_t * impl) {
	if(impl->order == LOGFILE_ROLL_ASC){
		string nextfile = impl->logfile + "." + std::to_string(impl->next_rollid);
		rename(impl->logfile.c_str(), nextfile.c_str());
		impl->next_rollid = impl->next_rollid % impl->max_roll + 1;
	}
	else {
		string src_file_name, dst_file_name;
		for (int i = impl->max_roll; i > 1; --i) {
			src_file_name = impl->logfile + "." + std::to_string(i - 1);
			dst_file_name = impl->logfile + "." + std::to_string(i);
			rename(src_file_name.c_str(), dst_file_name.c_str());
		}
		rename(impl->logfile.c_str(), src_file_name.c_str());
	}
}

int logfile_t::open(){
    if (impl->logfile.empty()){
        fputs("log file config file is empty!\n", stderr);
        return -1;
    }
    if (!impl->pf){ //open file
        impl->pf = fopen(impl->logfile.c_str(), "a");
    }
    if (impl->pf == nullptr){ // read file
        fprintf(stderr, "open log file :%s error %s!\n", 
			impl->logfile.c_str(), strerror(errno));
        return -2;
    }
	_init_open_logfile(impl);
    return 0;
}
void logfile_t::close(){
    if (impl && impl->pf){
        fflush(impl->pf);
        fclose(impl->pf);
        impl->pf = nullptr;
    }
}
int logfile_t::write(const char * logmsg){
    if (!impl || !impl->pf) return -1;
    fputs(logmsg, impl->pf);
    fflush(impl->pf);
    if (ftell(impl->pf) >= impl->max_file_size){
        close();//current , open next
		_shift_logfile(impl);
        return open();
    }
    return 0;
}

NS_END()

