
#include "dcutils.hpp"
#include "dclogfile.h"

NS_BEGIN(dcs)

struct logfile_roll_impl_t {
    FILE	*	pf{ nullptr };
    string		logfile;
	int			max_roll {10};
	int			max_file_size {1024*1024*20};
	logfile_roll_order order {LOGFILE_ROLL_ASC};
    logfile_type       ftype;
	int			next_rollid { 0 };
};

struct logfile_net_impl_t {
    string      netaddr;
    int         fd {-1};
};

struct logfile_impl_t {
    logfile_type            type;
    logfile_roll_impl_t     roll;
    logfile_net_impl_t      net;
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
int logfile_t::init(const char * file, int max_roll, int max_file_size,
                     logfile_roll_order order, logfile_type type){
    impl->type = type;
    if(type == LOGFILE_TYPE_ROLL){
        if (file && *file) {
            impl->roll.logfile = file;
            impl->roll.max_roll = max_roll;
            impl->roll.max_file_size = max_file_size;
            impl->roll.order = order;
        }    
        else {
            return -1;
        }
    }
    else if(type == LOGFILE_TYPE_NET){
        impl->net.netaddr = file;
        impl->net.fd = -1;
    }
    else {
        impl->type = LOGFILE_TYPE_ROLL;
        return -2;
    }
}
static inline void _init_open_logfile(logfile_roll_impl_t * impl) {
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
        return;
	}
}
static inline void _shift_logfile(logfile_roll_impl_t * impl) {
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
    if(impl->type == LOGFILE_TYPE_ROLL){    
        if (impl->roll.logfile.empty()) {
            fputs("log file config file is empty!\n", stderr);
            return -1;
        }
        if (!impl->roll.pf) { //open file
            impl->roll.pf = fopen(impl->roll.logfile.c_str(), "a");
        }
        if (impl->roll.pf == nullptr) { // read file
            fprintf(stderr, "open log file :%s error %s!\n",
                    impl->roll.logfile.c_str(), strerror(errno));
            return -2;
        }
        _init_open_logfile(&impl->roll);    
    }
    else if(impl->type == LOGFILE_TYPE_NET){
        impl->net.fd = dcs::openfd(impl->net.netaddr, "w", 800);
        if(impl->net.fd < 0){
            fprintf(stderr, "open netaddr:%s error:%d !", impl->net.netaddr.c_str(), impl->net.fd);
            return -3;
        }
    }
    else {
        fprintf(stderr, "erro file type:%d!\n", impl->type);    
        return -1;
    }
    return 0;
}
void logfile_t::close(){
    if(!impl){
        return;
    }
    if(impl->type == LOGFILE_TYPE_ROLL){
        if (impl->roll.pf) {
            fflush(impl->roll.pf);
            fclose(impl->roll.pf);
            impl->roll.pf = nullptr;
        } 
    }
    else if(impl->type == LOGFILE_TYPE_NET){
        if(impl->net.fd >= 0){
            dcs::closefd(impl->net.fd);
            impl->net.fd = -1;       
        }
    }
    else {
        return;
    }
}
int logfile_t::write(const char * logmsg){
    if (!impl) { return -1; }
    if(impl->type == LOGFILE_TYPE_ROLL){
        if(!impl->roll.pf){
            return -1;
        }
        fputs(logmsg, impl->roll.pf);
        fflush(impl->roll.pf);
        if (ftell(impl->roll.pf) >= impl->roll.max_file_size) {
            close();//current , open next
            _shift_logfile(&impl->roll);
            return open();
        }
        return 0;
    }
    else if (impl->type == LOGFILE_TYPE_NET){
        int ret = dcs::writefd(impl->net.fd, logmsg, 0, "end", 0);
        if(ret <= 0){
            return -2;
        }
    }
    else {
        return -1;
    }
}

NS_END()

