#pragma  once
#include "stdinc.h"

NS_BEGIN(dcsutil)
    //-----------------noncopy----------------------------
	class noncopyable
	{
	protected:
		noncopyable() {}
		~noncopyable() {}
	private: // emphasize the following members are private  
		noncopyable(const noncopyable&);
		const noncopyable& operator=(const noncopyable&);
	};
    //-----------------lock-------------------------------
    template<bool threadsafe>
    struct lock_mixin;

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
    //-------------------------------------------------------------------------
    //----------misc------------------------------------------------------------
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
	int					strsplit(const std::string & str, const string & sep, std::vector<std::string> & vs, bool ignore_empty = true, int maxsplit = 0, int beg = 0, int end = 0);
	size_t				strprintf(std::string & str, const char * format, ...);
	size_t				strnprintf(std::string & str, size_t max_sz, const char * format, ...);
	size_t				vstrprintf(std::string & str, const char* format, va_list va);
	void				strrepeat(std::string & str, const char * rep, int repcount);
	const char*			strrandom(std::string & randoms, int length = 8, char charbeg = 0x21, char charend = 0x7E);
    template <typename StrItable>
    const char         *strjoin(std::string & val, const std::string & sep, StrItable it){
        size_t i = 0;
        val.clear();
        for (auto & v : it){
            if (i != 0){
                v.append(sep);
            }
            v.append(v);
            ++i;
        }
    }
    const char          *strspack(std::string & str, const std::string & sep, const std::string & ks, ...);
    int                 strsunpack(const std::string & str, const std::string & sep, const std::string & k, ...);
    //todo variadic  



    ///////////uuid////////////////////////////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////////////////////////////////
    //max size = 0 , unlimited
    template<class T, size_t MAX_NUM = 0>
    class object_pool {
    public:
        typedef typename std::vector<T>::iterator    pointer;
    private:
        std::vector<T>                      pool;
        std::unordered_set<uint64_t>        free_pool;
    public:
        object_pool(){
            if (MAX_NUM > 0){
                pool.reserve(MAX_NUM);
            }
            else {
                pool.reserve(1024);
            }
        }
        bool      is_null(pointer p){
            return p == pool.end();
        }
        size_t    alloc(){
            if (MAX_NUM > 0 && pool.size() >= MAX_NUM){
                return 0; //error
            }
            return genid();
        }
        void        free(size_t id){
            if (id > 0 && id <= pool.size()){
                free_pool.insert(id);
            }
        }
        size_t      id(pointer p){
            return std::distance(pool.begin(), p) + 1;
        }
        pointer     ptr(size_t id){
            if (id > 0 && free_pool.find(id) == free_pool.end()){
                if (id <= pool.size()){
                    pointer it = pool.begin();
                    std::advance(it, id - 1);
                    return it;
                }
            }
            return pool.end();
        }
        void    clear(){
            free_pool.clear();
            pool.clear();
        }
    private:
        size_t    genid(){
            if (free_pool.empty()){
                pool.push_back(T());                
                return pool.size();
            }
            else {
                size_t id = *free_pool.begin();
                free_pool.erase(free_pool.begin());
                return id;
            }
        }
    };
    //////////////////////////////////////////////////////////////////////////////////////////////////////

NS_END()