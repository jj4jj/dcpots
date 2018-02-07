#pragma  once
#include "stdinc.h"
NS_BEGIN(dcs)
    //-----------------lock-------------------------------
    template<bool threadsafe>
    struct lock_mixin;
    //----------misc------------------------------------------------------------
	uint64_t			time_unixtime_ns();
	uint64_t			time_unixtime_us();
	inline	uint64_t	time_unixtime_ms(){ return time_unixtime_us() / 1000L;}
	inline	time_t		time_unixtime_s() { return time(NULL); }

    const char*			strftime(std::string & str, time_t unixtime = 0, const char * format = "%FT%X%z");
    time_t  	        strptime(const std::string & str, const char * format = "%FT%X%z");
    const char*			strftime(std::string & str, struct tm & rtm, const char * format = "%FT%X%z");

    time_t				stdstrtime(const char * strtime = "1970-01-01T08:08:08+0800");
    bool                time_same_hour(time_t t1, time_t t2);
    bool                time_same_ten_minutes(time_t t1, time_t t2);
    bool                time_same_day(time_t t1, time_t t2);
    bool                time_same_month(time_t t1, time_t t2);
    bool                time_same_week(time_t t1, time_t t2);
    bool                time_same_year(time_t t1, time_t t2);
    int	                getminutes(time_t unixtime);

	//file releataed
    //if sz = 0 , test file exist
    int					readfile(const std::string & file, std::string & content);
	int					readfile(const std::string & file, char * buffer , size_t sz);
    int                 touch_file(const std::string & file_path);
    int					writefile(const std::string & file, const char * buffer, size_t sz = 0);
	bool				file_exists(const std::string & file);
	size_t              file_size(const std::string & file);
	time_t				file_access_time(const std::string & file);
	time_t				file_modify_time(const std::string & file);
    int                 file_md5sum(std::string & md5sum, const std::string & file);
	const char *		path_base(const char * path);
    std::string         path_dir(const char * path);
    typedef void(*path_walker_t)(const char * dirname, const char * basename, bool is_dir, void * ud);
    int                 path_walk(const char * path, path_walker_t walker, void * ud, int depth = 0);
    enum  path_entry_filter {
        PATH_IS_FILE    = 0x01,
        PATH_IS_DIR     = 0x02,
        PATH_IS_BLK     = 0x04,
        PATH_IS_FIFO    = 0x08,
        PATH_IS_SOCKET  = 0x10,
        PATH_IS_LINK    = 0X20,
        PATH_IS_CHAR    = 0x40,
    };
    int                 path_list(std::vector<std::string> & file_list, const char * path, int filter = PATH_IS_FILE);
    //file://<path>
    //tcp://<ip:port>
    //udp://<ip:port>
    //?
	//mode:r:listen/w:connect,others -> file
    int                 openfd(const std::string & uri, const char * mode = "w", int timeout_ms = 30000);
	//mode: size, end, msg:sz32/16/8, token:\r\n\r\n , return > 0 read size, = 0 end, < 0 error
	int                 readfd(int fd, char * buffer, size_t sz, const char * mode = "end", int timeout_ms = 10000);
	//mode: size, end, msg:sz32/16/8, token:\r\n\r\n , return > 0 write size, = 0 end, < 0 error
	int                 writefd(int fd, const char * buffer, size_t sz = 0, const char * mode = "end", int timeout_ms = 10000);
	//tcp return fd, udp return 0 , error < 0
	//int				readfromfd(int fd, struct sockaddr_in & addr);
    int                 closefd(int fd);
    int                 nonblockfd(int fd, bool nonblock = true);
    bool                isnonblockfd(int fd);
    //return 0: readable , other error occurs
    int                 waitfd_readable(int fd, int timeout_ms);
    int                 waitfd_writable(int fd, int timeout_ms);
    int                 ipfromhostname(OUT uint32_t * ip, INOUT int & ipnum, const std::string & hostname);
    int                 socknetaddr(struct sockaddr_in & addr, const std::string & saddr);
    int                 netaddr(struct sockaddr_in & addr, bool stream, const std::string & saddr);

    uint32_t            host_getipv4(const char * nic="eth0");
    string              stripfromu32v4(uint32_t ip);
    uint32_t            u32fromstripv4(const string & ip);
    string              host_getmac(const char * nic="eth0");

    ///////////process////////////////////////////////////////////////////////////////////////
	int					daemonlize(int closestdio = 1, int chrootdir = 0, const char * pidfile = nullptr);
	//he signals SIGKILL and SIGSTOP cannot be caught or ignored
	int					signalh_ignore(int sig);
	int					signalh_default(int sig);
	typedef void(*sah_handler)(int sig, siginfo_t * sig_info, void * ucontex);
	int					signalh_push(int sig, sah_handler sah, int sah_flags = 0);
	sah_handler			signalh_pop(int sig);
	void				signalh_clear(int sig);
    int                 signalh_set(int sig, sah_handler sah, int sah_flags = 0);
	//-1:open file error , getpid():lock ok , 0:lock error but not known peer, >0: the locker pid.
    int					lockpidfile(const std::string & pidfile, int kill_other_sig = 0, bool nb = true, int * plockfd = nullptr, bool notify=false);
    //return fd
    int                 lockfile(const std::string & file, bool nb = true);
    int                 unlockfile(int fd);

	///////////str////////////////////////////////////////////////////////////////////////////////
    enum str_hash_strategy {
        hash_str_dbj2,
        hash_str_sdbm,
    };
    size_t              strhash(const std::string & buff, str_hash_strategy st = hash_str_sdbm);
	int					strsplit(const std::string & str, const string & sep, std::vector<std::string> & vs, bool ignore_empty = true, int maxsplit = 0, int beg = 0, int end = 0);
	size_t				strprintf(std::string & str, const char * format, ...);
	size_t				strnprintf(std::string & str, size_t max_sz, const char * format, ...);
	size_t				vstrprintf(std::string & str, const char* format, va_list va);
	void				strrepeat(std::string & str, const char * rep, int repcount);
    bool                strisint(const std::string & str, int base = 10);
    const char *		strrandom(std::string & randoms, int length = 8, char charbeg = 0x21, char charend = 0x7E);
    const char *		strcharsetrandom(std::string & randoms, int length = 8, const char * charset = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIGKLMNOPQRSTUVWXYZ_!@#$+-");
    string &            strreplace(string & str, const string & sub, const string & repl, bool global = false);
    string &            strrereplace(string & str, const string & repattern, const string & repl);
    bool                strrefind(string & str, const string & repattern, std::match_results<string::const_iterator>& m);
    std::string &       strtrim(std::string & str, const char * charset=" \t");
    std::string &       strltrim(std::string & str, const char * charset = " \t");
    std::string &       strrtrim(std::string & str, const char * charset = " \t");

    template <typename StrItable>
    const char         *strjoin(std::string & val, const std::string & sep, StrItable it);
    const char         *strjoin(std::string & val, const std::string & sep, const char ** it);
    const char         *strspack(std::string & str, const std::string & sep, const std::string & ks, ...);
    int                 strsunpack(const std::string & str, const std::string & sep, const std::string & k, ...);
    //todo variadic  ?

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    int                 b64_encode(std::string & b64, const char * buff, int ibuff);
    int                 b64_decode(std::string & buff, const std::string & b64);
    int                 hex2bin(std::string & bin, const char * hex);
    int                 bin2hex(std::string & hex, const char * buff, int ibuff);


    //////////////////////////////////////////////////////////////////////////////////////
    void    *           dllopen(const char * file, int flag = 0);
    void    *           dllsym(void * dll, const char * sym);
    int                 dllclose(void * dll);
    const char *        dllerror();

    ////////////////////////////////////////////////////////////////////////////




    //implementation
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    template<>
    struct lock_mixin<false>{
        void lock(){}
        void unlock(){}
    };

    template<>
    struct lock_mixin<true>{
        void lock(){ lock_.lock(); }
        void unlock(){ lock_.unlock(); }
    private:
        std::mutex  lock_;
    };
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename StrItable>
    const char         *strjoin(std::string & val, const std::string & sep, StrItable it){
        size_t i = 0;
        val.clear();
        for (auto & v : it){
            if (i != 0){ val.append(sep); }
            val.append(v);
            ++i;
        }
        return val.c_str();
    }

NS_END()
