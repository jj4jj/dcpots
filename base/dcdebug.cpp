#include <cxxabi.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <ucontext.h>  


#include "dcdebug.h"

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
        strnprintf(s, 384, "%s|%s|%p", this->module.c_str(), this->func.c_str(), this->addr);
    }
    else {
        s = this->info;
    }
    return s.c_str();
}
//////////////////////////////////////////////////////////////////////////////
#if (defined __x86_64__)  
#define REGFORMAT   "%016lx"      
#elif (defined __i386__)  
#define REGFORMAT   "%08x"  
#elif (defined __arm__)  
#define REGFORMAT   "%lx"  
#endif  

static inline int _ucontext_stacktrace(ucontext_t *uc, std::vector<stackframe_info_t> & vsi, int startlevel, int maxlevels){
    void **frame_pointer = (void **)NULL;
    void *return_address = NULL;
    Dl_info dl_info;
    memset(&dl_info, 0, sizeof(Dl_info));
#if (defined __i386__)  
    frame_pointer = (void **)uc->uc_mcontext.gregs[REG_EBP];
    return_address = (void *)uc->uc_mcontext.gregs[REG_EIP];
#elif (defined __x86_64__)  
    frame_pointer = (void **)uc->uc_mcontext.gregs[REG_RBP];
    return_address = (void *)uc->uc_mcontext.gregs[REG_RIP];
#elif (defined __arm__)  
    frame_pointer = (void **)uc->uc_mcontext.arm_fp;
    return_address = (void *)uc->uc_mcontext.arm_pc;
#endif
    int stack_layer = 0;
    int status = 0;
    char demangled_func_buff[128];
    size_t demangled_len = sizeof(demangled_func_buff);
    while (frame_pointer && return_address && stack_layer < maxlevels) {
        if (!dladdr(return_address, &dl_info)){
            break;
        }
        const char *sym_name = dl_info.dli_sname;
        if (stack_layer >= startlevel && sym_name){
            vsi.push_back(stackframe_info_t());
            stackframe_info_t & si = vsi.back();        
            /* No: return address <sym-name + offset> (filename) */
            //fprintf(stderr, "%02d: %p <%s + %lu> (%s)", stack_layer, return_address, sym_name,
            //    (unsigned long)return_address - (unsigned long)dl_info.dli_saddr,
            //    dl_info.dli_fname);
            si.offset = (unsigned long)return_address - (unsigned long)dl_info.dli_saddr;
            si.addr = dl_info.dli_saddr;
            si.module = basename(dl_info.dli_fname);
            si.func = sym_name;
            demangled_len = sizeof(demangled_func_buff);
            char * demanged_name = abi::__cxa_demangle(sym_name, demangled_func_buff, &demangled_len, &status);
            if (demanged_name && 0 == status){ //ok
                si.func = demanged_name;
            }
            si.info = sym_name;
            if (dl_info.dli_sname && !strcmp(dl_info.dli_sname, "main")) {
                break;
            }
        }
        ++stack_layer;
#if (defined __x86_64__) || (defined __i386__)  
        return_address = frame_pointer[1];
        frame_pointer = (void **)(frame_pointer[0]);
#elif (defined __arm__)
        return_address = frame_pointer[-1];
        frame_pointer = (void **)frame_pointer[-3];
#endif  
    }
    return vsi.size();
}

static inline int   _normal_stacktrace(std::vector<stackframe_info_t> & vsi, int startlevel, int maxlevels){
    static char mod_buff[512];
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
    buffer = (void **)malloc((maxlevels + startlevel) * sizeof(char *));
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

//must link with -rdynamic
int           stacktrace(std::vector<stackframe_info_t> & vsi, int startlevel, int maxlevels, void * uc){
    vsi.clear();
    if (!uc){
        return _normal_stacktrace(vsi, startlevel, maxlevels);
    }
    else {
        return _ucontext_stacktrace((ucontext_t*)uc, vsi, startlevel, maxlevels);
    }
}
const char *  stacktrace(string & str, int startlevel, int maxlevel, void * uc){
    std::vector<stackframe_info_t> vsi;
    int n = stacktrace(vsi, startlevel + 1, maxlevel, uc);
    string  allocator_s;
    assert(n == (int)vsi.size());
	char sibuff[16];
    for (int i = 0; i < n; ++i){
        if (i > 0){
            str.append("\n");
        }
		snprintf(sibuff, sizeof(sibuff)-1, "#%02d#", startlevel + i);
		str.append(sibuff);
        str.append(vsi[i].str(allocator_s));
    }
    return str.c_str();
}

NS_END()




