
#include "stdinc.h"
#include "dclogfile.h"


NS_BEGIN(dcsutil)

struct logfile_impl_t {
    FILE	*	pf{ nullptr };
    int			next_rollid{ 0 };
    string		logfile;
};
logfile_t::logfile_t(){
    impl = new logfile_impl_t();
}
logfile_t::~logfile_t(){
    close();
    if (impl)
        delete impl;
}
void logfile_t::init(const char * file){
    if (file && *file) impl->logfile = file;
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
    if (impl->next_rollid == 0){ //init , get the next roll id
        char nextfileid[32] = { 0 };
        FILE* rfp = fopen(impl->logfile.c_str(), "r");
        if (rfp){ //reading
            size_t sz = sizeof(nextfileid)-1;
            char * pnxf = (char*)nextfileid;
            if (getline(&pnxf, &sz, rfp) > 0){
                char * converted = nullptr;
                impl->next_rollid = strtoul(nextfileid, &converted, 10);
                if (nextfileid == converted){
                    impl->next_rollid = 0;
                }
            }
            fclose(rfp);
        }
        //read done
        if (impl->next_rollid == 0){ //init write 
            fseek(impl->pf, 0, SEEK_SET);
            fprintf(impl->pf, "1\n");
        }
        impl->next_rollid = impl->next_rollid > 0 ? impl->next_rollid : 1;
    }
    else {
		fseek(impl->pf, 0, SEEK_SET);
		ftruncate(fileno(impl->pf), 0);
		fprintf(impl->pf, "%d\n", impl->next_rollid);
    }
    return 0;
}
void logfile_t::close(){
    if (impl && impl->pf){
        fflush(impl->pf);
        fclose(impl->pf);
        impl->pf = nullptr;
    }
}
int logfile_t::write(const char * logmsg, int max_roll, int max_file_size){
    if (!impl || !impl->pf) return -1;
    fputs(logmsg, impl->pf);
    if (ftell(impl->pf) >= max_file_size){
        close();//current , open next
        string nextfile = impl->logfile + "." + std::to_string(impl->next_rollid);
        rename(impl->logfile.c_str(), nextfile.c_str());
        impl->next_rollid = (impl->next_rollid + 1) % max_roll;
        if (impl->next_rollid == 0) { impl->next_rollid = max_roll; };
        return open();
    }
    return 0;
}

NS_END()

