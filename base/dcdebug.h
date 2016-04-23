#pragma  once

#include "stdinc.h"

NS_BEGIN(dcsutil)
struct stackframe_info_t {
    string      module;
    string      func;
    size_t      offset;
    void        * addr;
    string      info;
    stackframe_info_t();
    const char * str(string & s);
};

const char *  stacktrace(string & str, int startlevel = 0, int maxlevels = 16);
int           stacktrace(std::vector<stackframe_info_t> & vsi, int startlevel = 0, int maxlevels = 16);

        
NS_END()
