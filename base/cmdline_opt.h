#pragma once
#include <map>
#include <string>
struct cmdline_opt_impl_t;
struct cmdline_opt_t {
	//opt:<long name>:[nro]:[short name]:[desc]:[default value][;<opt>]* , n:no arguments, r:required, o:optinal
    int             init(int argc, const char * argv[], const char * init_pattern = nullptr);
    const char *    program() const;
    void			parse(const char * pattern =  nullptr, const char * version = nullptr);
    int				getoptnum(const char * opt) const;
	const char *	getoptstr(const char * opt, int idx = 0) const;
	int				getoptint(const char * opt, int idx = 0) const;
    bool			hasopt(const char * opt, int idx = 0) const;
	void			pusage() const;
	const char *	usage() const;
    const ::std::multimap<std::string, std::string>  & options() const;

public:
	cmdline_opt_t(int argc = 0,const char ** argv = nullptr);
	~cmdline_opt_t();
    cmdline_opt_impl_t * impl_;
};