#include <getopt.h>
extern char *optarg;
extern int optind, opterr, optopt;
#include "dcutils.hpp"
#include "cmdline_opt.h"

struct cmdline_opt_impl_t {
	int								argc;
	const char *	*				argv;
	std::multimap<string, string>	dict_opts;
	std::map<string, string>		dict_opts_default;
	string							usage;
    string                          init_pattern;
    string                          program;
};

cmdline_opt_t::cmdline_opt_t(int argc, const char ** argv) :impl_(nullptr){
    init(argc, argv, nullptr);
}
static inline void _destroy_cmdlineopt(cmdline_opt_t * cmdopt){
    if (cmdopt->impl_){
        delete cmdopt->impl_;
        cmdopt->impl_ = nullptr;
    }
}
cmdline_opt_t::~cmdline_opt_t(){
    _destroy_cmdlineopt(this);
}
#define MAX_OPT_SHOW_TEXT_WIDTH (32)
int cmdline_opt_t::init(int argc, const char * argv[], const char * init_pattern /* = nullptr */){
    if (argc > 0 && argv != nullptr){
        _destroy_cmdlineopt(this);
        impl_ = new cmdline_opt_impl_t();
        impl_->argc = argc;
        impl_->argv = argv;
        impl_->program = argv[0];
        if (init_pattern){
            impl_->init_pattern = init_pattern;
        }
        return 0;
    }
    else {
        return -1;
    }
}
static inline void pversion(const char * version){
    std::cout << version << std::endl;
    exit(0);
}
void cmdline_opt_t::parse(const char * pattern, const char * version){
    impl_->dict_opts.clear();
    impl_->dict_opts_default.clear();

    impl_->usage = "Usage: ";
    impl_->usage += impl_->program;
    impl_->usage += " [Options]	\n";
    impl_->usage += "Options should be like as follow:\n";

    std::vector<std::string>	sopts;
    string pattern_ex = impl_->init_pattern;
    if (!pattern_ex.empty() && pattern_ex.back() != ';'){
        pattern_ex.append(";");
    }
    pattern_ex += "help:n:h:show help info;";
    if (version){
        pattern_ex += "version:n:V:show version info:";
        pattern_ex += version;
        pattern_ex += ";";
    }
    if (pattern){
        pattern_ex += pattern;
    }
	dcs::strsplit(pattern_ex.c_str(), ";", sopts);
	std::vector<struct option>	longopts;
	std::vector<std::string>	longoptnames;
	longoptnames.reserve(128);
    std::unordered_map<int, string> shortopt_2_longopt;
	string short_opt;
	for (auto & sopt : sopts){
		std::vector<std::string>	soptv;
		dcs::strsplit(sopt, ":", soptv, false, 5);
		if (soptv.size() < 4){
			std::cerr << "error format option:" << sopt << " size:" << soptv.size() << " but < 4" << std::endl;
			std::cerr << "pattern opt must be format of '[<long name>]:[rno]:[<short name>]:[desc]:[default];' " << std::endl;
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
                shortopt_2_longopt[opt_.val] = opt_.name;
			}
			longopts.push_back(opt_);
			//std::cout << "add dbg:" << opt_.name << ":val:" << opt_.val << std::endl;
		}
		//////////////////////////////////////////////////////////
		dcs::strrepeat(impl_->usage, " ", 4);
		int length = 4;
		if (soptv[2][0]){
			impl_->usage += "-";
			impl_->usage += soptv[2];
			length += 2;
		}
		if (soptv[0][0]){
			if (soptv[2][0]){
				impl_->usage += ", ";
				length += 2;
			}
			impl_->usage += "--";
			impl_->usage += soptv[0];
			length += soptv[0].length();
			length += 2;
		}
		if (soptv[1][0] == 'r'){
			impl_->usage += " <arg>";
			length += 6;
		}
		if (soptv[1][0] == 'o'){
			impl_->usage += " [arg]";
			length += 6;
		}
		if (soptv[3][0]){
			if (length < MAX_OPT_SHOW_TEXT_WIDTH){
				dcs::strrepeat(impl_->usage, " ", MAX_OPT_SHOW_TEXT_WIDTH - length);
			}
			impl_->usage += "\t";
			impl_->usage += soptv[3];
		}
		if (soptv.size() > 4 && soptv[4][0]){
			impl_->usage += "  (";
			impl_->usage += soptv[4];
			impl_->usage += ")";
			/////////////////////////////////////
			impl_->dict_opts_default[string(soptv[0])] = soptv[4];
			impl_->dict_opts_default[string(soptv[2])] = soptv[4];
		}
		impl_->usage += "\n";
		//////////////////////////////////////////////////////////
	}
	//end
	struct option end_opt_ = { NULL, no_argument, NULL, 0 };
	longopts.push_back(end_opt_);
	///////////////////////////////////////////////////////////////////////////
	int longIndex = 0;
	int opt = 0;
	//std::cout << "dbg:" << short_opt << std::endl;
    opt = getopt_long(impl_->argc, (char* const *)impl_->argv, short_opt.c_str(), &longopts[0], &longIndex);
	while (opt != -1) {
		if (opt == 0){
			string opt_name = longopts[longIndex].name;
			string opt_value = (optarg ? optarg : "");
			//std::cout << "dbg long:" << opt_name << "=" << opt_value << ":length:" << opt_value.length() << std::endl;
			impl_->dict_opts.insert(std::make_pair(opt_name, opt_value));
			if (longopts[longIndex].val > 0){
				opt_name = string((char*)&(longopts[longIndex].val), 1);
				//std::cout << "dbg short:" << opt_name << "=" << opt_value << ":length:" << opt_value.length() << std::endl;
				impl_->dict_opts.insert(std::make_pair(opt_name, opt_value));
			}
            //std::clog << "opt_name" << "dbg"<< std::endl;
			if (opt_name == "help"){
				pusage();
			}
            else if (version && opt_name == "version"){
                pversion(version);
            }
		}
		else if (opt == '?'){
			//usage
			pusage();
		}
		else {	//short opt
            if(opt == 'h'){
                pusage();
            }
			if (version && opt == 'V'){
				pversion(version);
			}
			string opt_name = string((char*)&opt, 1);
			string opt_value = (optarg ? optarg : "");
			impl_->dict_opts.insert(std::make_pair(opt_name, opt_value));
			//std::cout << "dbg short:" << opt_name << "=" << opt_value << ":length:" << opt_value.length() << std::endl;

            if (shortopt_2_longopt.find(opt) != shortopt_2_longopt.end()){
                opt_name = shortopt_2_longopt[opt];
                //std::cout << "dbg short:" << opt_name << "=" << opt_value << ":length:" << opt_value.length() << std::endl;
                impl_->dict_opts.insert(std::make_pair(opt_name, opt_value));
            }
		}
        opt = getopt_long(impl_->argc, (char* const *)impl_->argv, short_opt.c_str(), &longopts[0], &longIndex);
	}
}
int			
cmdline_opt_t::getoptnum(const char * opt){
	auto range = impl_->dict_opts.equal_range(opt);
	int count = 0;
	while (range.first != range.second){
		++count;
		range.first++;
	}
	return count;
}
bool			
cmdline_opt_t::hasopt(const char * opt, int idx){
    auto range = impl_->dict_opts.equal_range(opt);
    while (range.first != range.second){
        if (idx == 0){
            return range.first->second.c_str();
        }
        --idx;
        range.first++;
    }
    return nullptr;
}
const char * 
cmdline_opt_t::getoptstr(const char * opt, int idx){
	auto range = impl_->dict_opts.equal_range(opt);
	while (range.first != range.second){
		if (idx == 0){
			return range.first->second.c_str();
		}
		--idx;
		range.first++;
	}
	auto it = impl_->dict_opts_default.find(opt);
	if (it != impl_->dict_opts_default.end()){
		return it->second.c_str();
	}
	return nullptr;
}
int			 
cmdline_opt_t::getoptint(const char * opt, int idx){
	const char * value = getoptstr(opt, idx);
	if (value){
		return atoi(value);
	}
	return 0;
}
void
cmdline_opt_t::pusage(){
	std::cerr << impl_->usage << std::endl;
    exit(0);
}
const char *	
cmdline_opt_t::usage(){
	return impl_->usage.c_str();
}
const std::multimap<std::string, std::string>  &
cmdline_opt_t::options() const {
    return impl_->dict_opts;
}

