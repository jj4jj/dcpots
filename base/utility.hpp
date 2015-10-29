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

namespace util {
	//time 
	uint64_t	time_unixtime_ms();
	uint64_t	time_unixtime_us();
};
