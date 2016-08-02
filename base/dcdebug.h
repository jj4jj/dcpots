#pragma  once

#include "stdinc.h"

NS_BEGIN(dcs)
struct stackframe_info_t {
    string      module;
    string      func;
    size_t      offset{ 0 };
    void        * addr{ nullptr };
    string      info;
    stackframe_info_t();
    const char * str(string & s);
};

const char *  stacktrace(string & str, int startlevel = 0, int maxlevels = 16, void * ucontext = nullptr);
int           stacktrace(std::vector<stackframe_info_t> & vsi, int startlevel = 0, int maxlevels = 16, void * ucontext = nullptr);

        
NS_END()
