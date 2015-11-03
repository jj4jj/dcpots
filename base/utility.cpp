
#include "utility.hpp"
#include "stdinc.h"

namespace dcsutil {
	uint64_t	time_unixtime_ms(){
		return time_unixtime_us() / 1000;
	}
	uint64_t	time_unixtime_us(){
		timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec * 1000000 + tv.tv_usec ;
	}

	int			daemonlize(int closestd , int chrootdir){
#if _BSD_SOURCE || (_XOPEN_SOURCE && _XOPEN_SOURCE < 500)
		return daemon(!chrootdir, !closestd);
#else
		return -404;//todo 
#endif
	}






}

