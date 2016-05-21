#pragma once

struct cmdline_opt_t {
	//opt:<long name>:[nro]:[short name]:[desc]:[default value][;<opt>]* , n:no arguments, r:required, o:optinal
	void			parse(const char * pattern = "version:n:v:desc:default;log-path:r::desc;:o:I:desc:default", const char * version = nullptr);
	int				getoptnum(const char * opt);
	const char *	getoptstr(const char * opt, int idx = 0);
	int				getoptint(const char * opt, int idx = 0);
	bool			hasopt(const char * opt, int idx = 0){return getoptstr(opt, idx) ? true : false;}
	void			pusage();
	const char *	usage();
public:
	cmdline_opt_t(int argc,const char ** argv);
	~cmdline_opt_t();
private:
	void * handle;
};