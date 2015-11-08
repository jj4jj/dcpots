#pragma  once
#include "stdinc.h"

class noncopyable
{
protected:
	noncopyable() {}
	~noncopyable() {}
private: // emphasize the following members are private  
	noncopyable(const noncopyable&);
	const noncopyable& operator=(const noncopyable&);
};

namespace dcsutil {
	//time 
	uint64_t	time_unixtime_ms();
	uint64_t	time_unixtime_us();
	int			daemonlize(int closestd = 1, int chrootdir = 0);
	int			readfile(const char * file, char * buffer, size_t sz);
	int			writefile(const char * file, const char * buffer, size_t sz = 0);
};
