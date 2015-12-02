#pragma once

struct cmdline_opt_t {
	void			parse(const char * pattern = "version:n:v:desc;log-path:r::desc;:o:I:desc");
	int				getoptnum(const char * opt);
	const char *	getoptstr(const char * opt, int idx = 0);
	int				getoptint(const char * opt, int idx = 0);
	void			pusage();
	const char *	usage();
public:
	cmdline_opt_t(int argc, char ** argv);
	~cmdline_opt_t();
private:
	void * handle;
};