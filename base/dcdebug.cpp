#include "dcdebug.h"
#include <cxxabi.h>
#include <execinfo.h>
#include "dcutils.hpp"
NS_BEGIN(dcsutil)

stackframe_info_t::stackframe_info_t(){
    this->addr = nullptr;
    this->offset = 0;
    this->module = "";
    this->func = "";
    this->info = "";
}
const char * stackframe_info_t::str(string & s){
    if (!this->func.empty()){
        strnprintf(s, 256, "%-18s%-44s%p", this->module.c_str(), this->func.c_str(), this->addr);
    }
    else {
        s = this->info;
    }
    return s.c_str();
}

//must link with -rdynamic
int           stacktrace(std::vector<stackframe_info_t> & vsi, int startlevel, int maxlevels){
    vsi.clear();
    static char mod_buff[256];
    char mangled_func_buff[128];
    int offset;
    int  ret;
    int nptrs;
    void * addr;
    void ** buffer = nullptr;
    char ** strings = nullptr;
    char demangled_func_buff[128];
    startlevel += 1;//skip local
    /////////////////////////////////////////////////////////////////////
    if (maxlevels <= 0 || startlevel < 0){
        goto FUNC_END;
    }
    buffer = (void **)malloc((maxlevels+startlevel) * sizeof(char *));
    if (!buffer){
        goto FUNC_END;
    }
    nptrs = backtrace(buffer, maxlevels + startlevel);
    if (nptrs <= 0){
        goto FUNC_END;
    }
    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        goto FUNC_END;
    }
    for (int j = startlevel; j < nptrs; j++){
        //demangle :<mod>(_ZN7dcsutil10stacktraceERSsii+0x33) [0x4f3cf2]
        vsi.push_back(stackframe_info_t());
        stackframe_info_t & si = vsi.back();
        ret = sscanf(strings[j], "%[^(](%[^+]+%i) [%li]", mod_buff, mangled_func_buff, &offset, (uint64_t*)&addr);
        if (ret == 4){ //full converted
            size_t demangled_len = sizeof(demangled_func_buff);
            char * demanged_name = abi::__cxa_demangle(mangled_func_buff, demangled_func_buff, &demangled_len, &ret);
            if (demanged_name && 0 == ret){ //ok
                si.addr = addr;
                si.offset = offset;
                si.func = demanged_name;
                si.module = basename(mod_buff);
                continue;
            }
        }
        si.info = strings[j];
    }
FUNC_END:
    if (strings){
        free(strings);
    }
    if (buffer){
        free(buffer);
    }
    return vsi.size();

}
const char *  stacktrace(string & str, int startlevel, int maxlevel){
    std::vector<stackframe_info_t> vsi;
    int n = stacktrace(vsi, startlevel + 1, maxlevel);
    string  allocator_s;
    assert(n == (int)vsi.size());
    for (int i = 0; i < n; ++i){
        if (i > 0){
            str.append("\n");
        }
        str.append(vsi[i].str(allocator_s));
    }
    return str.c_str();
}

NS_END()




