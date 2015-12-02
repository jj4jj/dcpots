#pragma once
#include <getopt.h>
extern char *optarg;
extern int optind, opterr, optopt;
#include "utility.hpp"

struct cmdline_opt_t {
	int					argc;
	char **				argv;
	std::multimap<string, string>	dict_opts;
	string							usage;
	//pattern:"{cmd:{}"
	cmdline_opt_t(int argc, char ** argv){
		this->argc = argc;
		this->argv = argv;
		usage = "usage: ";
		usage += argv[0];
		usage += " [options]	\n";
		usage += "option should be like as follow:\n";
	}
	//long pattern([a-zA-Z]+(-[a-zA-Z]+)?:nro:[a-zA-Z]?,)
	void			 parse(const char * pattern = "version:n:v:desc,log-path:r::desc,::I:desc"){
		std::vector<std::string>	sopts;
		dcsutil::split(pattern, ",", sopts);
		std::vector<struct option>	longopts;
		std::vector<std::string>	longoptnames;
		longoptnames.reserve(128);
		string short_opt;
		for (auto & sopt : sopts){
			std::vector<std::string>	soptv;
			dcsutil::split(sopt, ":", soptv, false);
			assert("pattern opt must be format of '[<long name>]:[rno]:[a-zA-Z]:[desc]' " && soptv.size() == 4);
			if (soptv[0].empty()){
				if (soptv[2][0]){
					short_opt += soptv[2][0];
					if (soptv[1][0] == 'r' || soptv[1][0] == 'o'){
						short_opt += ":";
					}
				}
			}
			else {
				longoptnames.push_back(soptv[0]);
				struct option opt_;
				opt_.name = longoptnames.back().c_str();
				opt_.flag = NULL;
				opt_.has_arg = no_argument;
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
			}
			//////////////////////////////////////////////////////////
			dcsutil::strrepeat(usage, " ", 4);
			int length = 0;
			if (soptv[2][0]){
				usage += "-";
				usage += soptv[2];
				length += 2;
			}
			if (soptv[0][0]){
				if (soptv[2][0]){
					usage += ", ";
					length += 2;
				}
				usage += "--";
				usage += soptv[0];
				length += soptv[0].length();
				length += 2;
			}
			if (soptv[1][0] == 'r'){
				usage += " <arg>";
				length += 6;
			}
			if (soptv[1][0] == 'o'){
				usage += " [arg]";
				length += 6;
			}
			if (soptv[3][0]){
				if (length < 25){
					dcsutil::strrepeat(usage, " ", 25 - length);
				}
				usage += "\t";
				usage += soptv[3];
			}
			usage += "\n";
			//////////////////////////////////////////////////////////

		}
		struct option end_opt_ { NULL, no_argument, NULL, 0 };
		longopts.push_back(end_opt_);
		///////////////////////////////////////////////////////////////////////////
		int longIndex = 0;
		int opt = 0;
		opt = getopt_long(argc, argv, short_opt.c_str(), &longopts[0], &longIndex);
		while (opt != -1) {
			if (opt == 0){
				//long opt index = longIndex , store
				if (optarg){
					dict_opts.insert(std::make_pair(string(longopts[longIndex].name),string(optarg)));
				}
				else {
					dict_opts.insert(std::make_pair(string(longopts[longIndex].name), string()));
				}
			}
			else if (opt == '?'){
				//usage
				std::cerr << "error option :" << (char)opterr << std::endl;
				pusage();
				exit(-1);
			}
			else {	//short opt
				if (optarg){
					dict_opts.insert(std::make_pair(string((char)opt,1), string(optarg)));
				}
				else {
					dict_opts.insert(std::make_pair(string((char)opt,1), string()));
				}
			}
			opt = getopt_long(argc, argv, short_opt.c_str(), &longopts[0], &longIndex);
		}
	}
	int			getoptnum(const char * opt){
		auto range = dict_opts.equal_range(opt);
		int count = 0;
		while (range.first != range.second){
			++count;
			range.first++;
		}
		return count;
	}
	const char * getoptstr(const char * opt, int idx = 0){
		auto range = dict_opts.equal_range(opt);
		while (range.first != range.second){
			if (idx == 0){
				return range.second->second.c_str();
			}
			--idx;
			range.first++;
		}
		return nullptr;
	}
	int			 getoptint(const char * opt, int idx = 0){
		auto range = dict_opts.equal_range(opt);
		while (range.first != range.second){
			if (idx == 0){
				return std::stoi(range.second->second);
			}
			--idx;
			range.first++;
		}
		return 0;
	}
	const char * pusage(){
		std::cerr << usage << std::endl;
	}
};