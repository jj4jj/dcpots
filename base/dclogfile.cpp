
#include "dcutils.hpp"
#include "dchannel.h"
#include "dclogfile.h"
#include "logger.h"

NS_BEGIN(dcs)

struct logfile_roll_impl_t {
    FILE	*	pf{ nullptr };
    string		logfile;
	int			max_roll {10};
	int			max_file_size {1024*1024*20};
	logfile_roll_order order {LOGFILE_ROLL_ASC};
	int			next_rollid { 0 };
};

struct logfile_async_impl_t {
    string          addr;
    ChannelPoint *  point;
    std::string     prefix;
    uint32_t        buffsz {1024*1024*8};
    std::string     logmsg;
};

struct logfile_impl_t {
    bool                    is_async;
    logfile_roll_impl_t     roll;
    logfile_async_impl_t    async;
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
    impl->is_async = (strstr(path, "://")!=nullptr);
    if(!impl->is_async){
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
        //xxx://<key>?path=v&size=v
        impl->async.addr = path;
        impl->async.point = nullptr;
        std::string::size_type sep = impl->async.addr.find("?");
        if(sep != std::string::npos){
            std::string params = impl->async.addr.substr(sep + 1);
            impl->async.addr.erase(sep);
            std::vector<std::string>    vs;
            dcs::strsplit(params, "&", vs);
            std::string spath = params;
            uint32_t def_buffsz = 1024*1024*8;
            for(auto & kv : vs){
                std::vector<std::string>  vskv;
                dcs::strsplit(kv,"=", vskv);
                if(vskv.size() != 2){
                    continue;
                }
                if(vskv[0] == "path"){
                    spath = vskv[1];
                }
                else if(vskv[0] == "buffsz"){
                    def_buffsz = atoi(vskv[1].c_str());
                }
            }
            impl->async.prefix = spath;
            impl->async.buffsz = def_buffsz;
        }
        else {
            fprintf(stderr, "error addr key:%s (format:xxx://key?path=v&buffsz=v)", path);
            return -2;
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
			mt = file_modify_time(file_name);
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
    if(!impl->is_async){    
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
        if (impl->async.point) {
            return 0;//ok already
        }
        impl->async.point = AsyncChannel::instance().Open(impl->async.addr.c_str(), impl->async.buffsz);
        if(!impl->async.point){
            fprintf(stderr, "open async addr:%s !", impl->async.addr.c_str());
            return -3;
        }
    }
    return 0;
}
void logfile_t::close(){
    if(!impl){
        return;
    }
    if(!impl->is_async){
        if (impl->roll.pf) {
            fflush(impl->roll.pf);
            fclose(impl->roll.pf);
            impl->roll.pf = nullptr;
        } 
    }
    else {
        if(impl->async.point){
            AsyncChannel::instance().Close(impl->async.point);
            impl->async.point = nullptr;
        }
    }
}
int logfile_t::write(const char * logmsg, int loglv){
    if (!impl) { return -1; }
    if(!impl->is_async){
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
        if(loglv == LOG_LVL_ERROR || loglv == LOG_LVL_FATAL){
            impl->async.logmsg = impl->async.prefix + "!|" + logmsg;
        }
        else {
            impl->async.logmsg = impl->async.prefix + "|" + logmsg;
        }
        return AsyncChannel::instance().Send(impl->async.point, impl->async.logmsg.data(), impl->async.logmsg.length());
    }
}

NS_END()

