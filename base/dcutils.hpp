#pragma  once
#include "stdinc.h"

NS_BEGIN(dcsutil)

	class noncopyable
	{
	protected:
		noncopyable() {}
		~noncopyable() {}
	private: // emphasize the following members are private  
		noncopyable(const noncopyable&);
		const noncopyable& operator=(const noncopyable&);
	};

	//time 
	uint64_t			time_unixtime_us();
	inline	time_t		time_unixtime_s(){ return time_unixtime_us() / 1000000L; }
	inline	uint64_t	time_unixtime_ms(){ return time_unixtime_us() / 1000L;}
	const char*			strftime(std::string & str, time_t unixtime = 0, const char * format = "%Y-%m-%dT%H:%M:%S");
	time_t				from_strtime(const char * strtime = "1970-01-01T08:08:08");

	//file
	int					readfile(const char * file, char * buffer, size_t sz);
	int					writefile(const char * file, const char * buffer, size_t sz = 0);

	///////////process/////////////////////////////////////////////////////
	int					daemonlize(int closestd = 1, int chrootdir = 0);
	//-1:open file error , getpid():lock ok , 0:lock error but not known peer, >0: the locker pid.
	int					lockpidfile(const char * pidfile, int kill_other_sig = 0, bool nb = true);

	///////////str////////////////////////////////////////////////////////////////////////////////
	int					strsplit(const std::string & str, const string & sep, std::vector<std::string> & vs, bool ignore_empty = true, int maxsplit = 0);
	size_t				strprintf(std::string & str, const char * format, ...);
	size_t				strnprintf(std::string & str, size_t max_sz, const char * format, ...);
	size_t				vstrprintf(std::string & str, const char* format, va_list va);
	void				strrepeat(std::string & str, const char * rep, int repcount);
	const char*			strrandom(std::string & randoms, int length = 8, char charbeg = 0x21, char charend = 0x7E);

	///////////uuid////////////////////////////////////////////////////////////////////////////////////////////////



NS_END()