
#include "utility.hpp"
#include "stdinc.h"

namespace util {
	uint64_t	time_unixtime_ms(){
		return time_unixtime_us() / 1000;
	}
	uint64_t	time_unixtime_us(){
		timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec * 1000000 + tv.tv_usec ;
	}
}

