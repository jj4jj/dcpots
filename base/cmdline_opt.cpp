#include <getopt.h>
extern char *optarg;
extern int optind, opterr, optopt;
#include "utility.hpp"
#include "cmdline_opt.h"

struct cmdline_opt_impl_t {
	int								argc;
	char **							argv;
	std::multimap<string, string>	dict_opts;
	string							usage;
};

#define _THIS_HANDLE ((cmdline_opt_impl_t*)handle)

cmdline_opt_t::cmdline_opt_t(int argc, char ** argv){
	handle = new cmdline_opt_impl_t();
	_THIS_HANDLE->argc = argc;
	_THIS_HANDLE->argv = argv;
	_THIS_HANDLE->usage = "usage: ";
	_THIS_HANDLE->usage += argv[0];
	_THIS_HANDLE->usage += " [options]	\n";
	_THIS_HANDLE->usage += "option should be like as follow:\n";
}
cmdline_opt_t::~cmdline_opt_t(){
	if (handle){
		delete _THIS_HANDLE;
	}
}
// = "version:n:v:desc;log-path:r::desc;:o:I:desc"
void			 
cmdline_opt_t::parse(const char * pattern){
	std::vector<std::string>	sopts;
	dcsutil::split(pattern, ";", sopts);
	std::vector<struct option>	longopts;
	std::vector<std::string>	longoptnames;
	longoptnames.reserve(128);
	string short_opt;
	for (auto & sopt : sopts){
		std::vector<std::string>	soptv;
		dcsutil::split(sopt, ":", soptv, false);
		if (soptv.size() != 4){
			std::cerr << "error format option:" << sopt << " size:" << soptv.size() << " but not 4" << std::endl;
			std::cerr << "pattern opt must be format of '[<long name>]:[rno]:[<short name>]:[desc];' " << std::endl;
			exit(-2);
		}
		if (soptv[2][0]){
			short_opt += soptv[2][0];
			if (soptv[1][0] == 'r'){
				short_opt += ":";
			}
			else if (soptv[1][0] == 'o'){
				short_opt += "::";
			}
		}
		if (soptv[0][0]){
			longoptnames.push_back(soptv[0]);
			struct option opt_;
			opt_.name = longoptnames.back().c_str();
			opt_.flag = NULL;
			opt_.has_arg = no_argument;
			opt_.val = 0;
			if (soptv[1][0] == 'o'){
				opt_.has_arg = optional_argument;
			}
			else if (soptv[1][0] == 'r'){
				opt_.has_arg = required_argument;
			}
			if (soptv[2][0]){
				opt_.val = soptv[2][0];
			}
			longopts.push_back(opt_);
			//std::cout << "add dbg:" << opt_.name << ":val:" << opt_.val << std::endl;
		}
		//////////////////////////////////////////////////////////
		dcsutil::strrepeat(_THIS_HANDLE->usage, " ", 4);
		int length = 0;
		if (soptv[2][0]){
			_THIS_HANDLE->usage += "-";
			_THIS_HANDLE->usage += soptv[2];
			length += 2;
		}
		if (soptv[0][0]){
			if (soptv[2][0]){
				_THIS_HANDLE->usage += ", ";
				length += 2;
			}
			_THIS_HANDLE->usage += "--";
			_THIS_HANDLE->usage += soptv[0];
			length += soptv[0].length();
			length += 2;
		}
		if (soptv[1][0] == 'r'){
			_THIS_HANDLE->usage += " <arg>";
			length += 6;
		}
		if (soptv[1][0] == 'o'){
			_THIS_HANDLE->usage += " [arg]";
			length += 6;
		}
		if (soptv[3][0]){
			if (length < 20){
				dcsutil::strrepeat(_THIS_HANDLE->usage, " ", 20 - length);
			}
			_THIS_HANDLE->usage += "\t";
			_THIS_HANDLE->usage += soptv[3];
		}
		_THIS_HANDLE->usage += "\n";
		//////////////////////////////////////////////////////////
	}
	struct option end_opt_ { NULL, no_argument, NULL, 0 };
	longopts.push_back(end_opt_);
	///////////////////////////////////////////////////////////////////////////
	int longIndex = 0;
	int opt = 0;
	//std::cout << "dbg:" << short_opt << std::endl;
	opt = getopt_long(_THIS_HANDLE->argc, _THIS_HANDLE->argv, short_opt.c_str(), &longopts[0], &longIndex);
	while (opt != -1) {
		if (opt == 0){
			string opt_name = longopts[longIndex].name;
			string opt_value = (optarg ? optarg : "");
			//std::cout << "dbg long:" << opt_name << "=" << opt_value << ":length:" << opt_value.length() << std::endl;
			_THIS_HANDLE->dict_opts.insert(std::make_pair(opt_name, opt_value));
			if (longopts[longIndex].val > 0){
				opt_name = string((char*)&(longopts[longIndex].val), 1);
				//std::cout << "dbg short:" << opt_name << "=" << opt_value << ":length:" << opt_value.length() << std::endl;
				_THIS_HANDLE->dict_opts.insert(std::make_pair(opt_name, opt_value));
			}
		}
		else if (opt == '?'){
			//usage
			pusage();
			exit(-1);
		}
		else {	//short opt
			string opt_name = string((char*)&opt, 1);
			string opt_value = (optarg ? optarg : "");
			_THIS_HANDLE->dict_opts.insert(std::make_pair(opt_name, opt_value));
			//std::cout << "dbg short:" << opt_name << "=" << opt_value << ":length:" << opt_value.length() << std::endl;
		}
		opt = getopt_long(_THIS_HANDLE->argc, _THIS_HANDLE->argv, short_opt.c_str(), &longopts[0], &longIndex);
	}
}
int			
cmdline_opt_t::getoptnum(const char * opt){
	auto range = _THIS_HANDLE->dict_opts.equal_range(opt);
	int count = 0;
	while (range.first != range.second){
		++count;
		range.first++;
	}
	return count;
}
const char * 
cmdline_opt_t::getoptstr(const char * opt, int idx){
	auto range = _THIS_HANDLE->dict_opts.equal_range(opt);
	while (range.first != range.second){
		if (idx == 0){
			return range.first->second.c_str();
		}
		--idx;
		range.first++;
	}
	return nullptr;
}
int			 
cmdline_opt_t::getoptint(const char * opt, int idx){
	auto range = _THIS_HANDLE->dict_opts.equal_range(string(opt));
	while (range.first != range.second){
		if (idx == 0){
			return std::stoi(range.first->second);
		}
		--idx;
		range.first++;
	}
	return 0;
}
void
cmdline_opt_t::pusage(){
	std::cerr << _THIS_HANDLE->usage << std::endl;
}
const char *	
cmdline_opt_t::usage(){
	return _THIS_HANDLE->usage.c_str();
}

