#pragma once
#include "logger.h"
#include "utility.hpp"

struct		profile_t {
	const char *  file;
	const char *  funcname;
	const int  line;
	static std::string	 level_s;
	uint64_t			 start_time;
	profile_t(const char * file_, const char * funcname_, int line_) :
		file(file_), funcname(funcname_), line(line_){
		start_time = dcsutil::time_unixtime_us();
		LOGR(LOG_LVL_PROF,"%s%s:%d BEGIN",level_s.c_str(), funcname, line);
		level_s.push_back('\t');
	}
	~profile_t(){
		uint64_t lcost_time = dcsutil::time_unixtime_us() - start_time;
		level_s.pop_back();
		LOGR(LOG_LVL_PROF, "%s%s:%d END | COST:%lu us", level_s.c_str(), funcname, line, lcost_time);
	}
};

#define PROFILE_ON
#ifdef PROFILE_ON
#define PROFILE_FUNC() profile_t _func_prof_(__FILE__,__FUNCTION__,__LINE__)
#else
#define PROFILE_FUNC() 
#endif