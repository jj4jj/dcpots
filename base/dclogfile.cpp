
#include "dcutils.hpp"
#include "dclogfile.h"

NS_BEGIN(dcs)

struct logfile_roll_impl_t {
    FILE	*	pf{ nullptr };
    string		logfile;
	int			max_roll {10};
	int			max_file_size {1024*1024*20};
	logfile_roll_order order {LOGFILE_ROLL_ASC};
	int			next_rollid { 0 };
};

struct logfile_net_impl_t {
    string          netaddr;
    int             fd {-1};
    std::string     prefix;
    std::string     logmsg;
};

struct logfile_impl_t {
    bool                    is_net_log;
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
int logfile_t::init(const char * path, int max_roll, int max_file_size,
                     logfile_roll_order order){
    impl->is_net_log = (strstr(path, "tcp://") == path ||
                        strstr(path, "udp://") == path );
    if(!impl->is_net_log){
        if (path && *path) {
            impl->roll.logfile = path;
            impl->roll.max_roll = max_roll;
            impl->roll.max_file_size = max_file_size;
            impl->roll.order = order;
        }    
        else {
            return -1;
        }
    }
    else {
        impl->net.netaddr = path;
        impl->net.fd = -1;
        std::string::size_type sep = impl->net.netaddr.find("?");
        if(sep != std::string::npos){
            impl->net.prefix = impl->net.netaddr.substr(sep+1);
            impl->net.netaddr.erase(sep);
        }
    }
    return open();
}
static inline void _init_open_logfile(logfile_roll_impl_t * impl) {
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
    if(!impl->is_net_log){    
        if(impl->roll.pf){
            return 0;//ok already
        }
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
    else {
        if (impl->net.fd != -1) {
            return 0;//ok already
        }
        impl->net.fd = dcs::openfd(impl->net.netaddr, "w", 800);
        if(impl->net.fd < 0){
            fprintf(stderr, "open netaddr:%s error:%d !", impl->net.netaddr.c_str(), impl->net.fd);
            return -3;
        }
    }
    return 0;
}
void logfile_t::close(){
    if(!impl){
        return;
    }
    if(!impl->is_net_log){
        if (impl->roll.pf) {
            fflush(impl->roll.pf);
            fclose(impl->roll.pf);
            impl->roll.pf = nullptr;
        } 
    }
    else {
        if(impl->net.fd >= 0){
            dcs::closefd(impl->net.fd);
            impl->net.fd = -1;       
        }
    }
}
int logfile_t::write(const char * logmsg){
    if (!impl) { return -1; }
    if(!impl->is_net_log){
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
    else {
        impl->net.logmsg = impl->net.prefix + "|" + logmsg;
        dcs::strreplace(impl->net.logmsg, "\n", "`", true);
        if('`' == impl->net.logmsg.back()){
            impl->net.logmsg.back() = '\n';
        }
        int ret = dcs::writefd(impl->net.fd, impl->net.logmsg.c_str(), impl->net.logmsg.length(), "end", 0);
        if(ret <= 0){
            return -2;
        }
        return 0;
    }
}

NS_END()

