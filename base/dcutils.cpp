#include "dcutils.hpp"
#include "logger.h"
////////////////////////
#include<sys/ioctl.h>
#include<sys/socket.h>
#include<net/if.h>
#include<arpa/inet.h>
#include<netinet/in.h>
//#include <libgen.h>

namespace dcsutil {
    uint64_t	time_unixtime_us(){
        timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000 + tv.tv_usec;
    }

    int			daemonlize(int closestd, int chrootdir){
#if _BSD_SOURCE || (_XOPEN_SOURCE && _XOPEN_SOURCE < 500)
        return daemon(!chrootdir, !closestd);
#else
        assert("not implement in this platform , using nohup & launch it ?")
            return -404;//todo 
#endif
    }
	typedef  std::unordered_map<int, std::stack<sah_handler> >	signalh_stacks_t;
	static signalh_stacks_t	s_sigh_stacks;
	int					signalh_ignore(int sig){
		return signalh_push(sig, (sah_handler)SIG_DFL);
	}
	int					signalh_default(int sig){
		return signalh_push(sig, (sah_handler)SIG_DFL);
	}
	void				signalh_clear(int sig){
		if (s_sigh_stacks.find(sig) == s_sigh_stacks.end()){
			return;
		}
		while (!s_sigh_stacks[sig].empty()){
			s_sigh_stacks[sig].pop();
		}
	}
	////////////////////////////////////////////////////////////////////////
	//typedef void(*sah_handler)(int sig, siginfo_t * sig_info, void * ucontex);
	int					signalh_push(int sig, sah_handler sah, int sah_flags){
		if (s_sigh_stacks.find(sig) == s_sigh_stacks.end()){
			s_sigh_stacks[sig] = std::stack<sah_handler>();
		}
		s_sigh_stacks[sig].push((sah_handler)SIG_IGN);
		struct sigaction act;
		memset(&act,0,sizeof(act));
		act.sa_flags = sah_flags;
		act.sa_sigaction = sah;
		//act.sa_mask = sah_mask;
		act.sa_flags = sah_flags | SA_SIGINFO;
		return sigaction(sig, &act, NULL);
	}
	sah_handler			signalh_pop(int sig){
		if (s_sigh_stacks.find(sig) == s_sigh_stacks.end() ||
			s_sigh_stacks[sig].empty()){
			return (sah_handler)SIG_DFL;
			//s_sigh_stacks[sig] = std::stack<sah_handler>();
		}
		sah_handler sah = s_sigh_stacks[sig].top();
		s_sigh_stacks[sig].pop();
		return sah;
	}

    int			readfile(const std::string & file, char * buffer, size_t sz){
        FILE * fp = fopen(file.c_str(), "r");
        if (!fp){
            GLOG_ERR("open file£º%s error:%d", file.c_str(), errno);
            return -1;
        }
        if (sz == 0 || !buffer){
            return 0; //file exist
            fclose(fp);
        }
        int n;
        size_t tsz = 0;
        while ((n = fread(buffer + tsz, 1, sz - tsz, fp))){
            if (n > 0){
                tsz += n;
            }
            else if (errno != EINTR &&
                errno != EAGAIN && errno != EWOULDBLOCK) {
                GLOG_ERR("read file:%s ret:%d error :%d total sz:%zu", file.c_str(), n, errno, tsz);
                break;
            }
        }
        fclose(fp);
        if (n >= 0){
            if (tsz < sz){
                buffer[tsz] = 0;
            }
            return tsz;
        }
        else {
            return -2;
        }
    }
    size_t      filesize(const std::string & file){
        FILE * fp = fopen(file.c_str(), "r");
        if (!fp){
            return 0;//not exist 
        }
        fseek(fp, 0L, SEEK_END);
        size_t sz = ftell(fp);
        fclose(fp);
        return sz;
    }
	const char *		path_base(const char * path){
		//#define MAX_PATH_LENGTH	 256
		//char path_buff[MAX_PATH_LENGTH] = { 0 };
		//strncpy(path_buff, path.c_str(), MAX_PATH_LENGTH - 1);
		return basename(path);
	}
#if 0
	//include by libgen
	string		path_base(const string & path){
		#define MAX_PATH_LENGTH	 256
		char path_buff[MAX_PATH_LENGTH] = { 0 };
		strncpy(path_buff, path.c_str(), MAX_PATH_LENGTH - 1);
		return basename(path_buff);
	}
	string 				path_dir(const string & path){
		#define MAX_PATH_LENGTH	 256
		char path_buff[MAX_PATH_LENGTH] = { 0 };
		strncpy(path_buff, path.c_str(), MAX_PATH_LENGTH - 1);
		return dirname(path_buff);
	}
#endif
    int			writefile(const std::string & file, const char * buffer, size_t sz){
        FILE * fp = fopen(file.c_str(), "w");
		if (!fp){
            GLOG_ERR("open file£º%s error:%d", file.c_str(), errno);
			return -1;
		}
		if (sz == 0){
			sz = strlen(buffer);
		}
		size_t tsz = 0;
		int n = 0;
		while ((n = fwrite(buffer + tsz, 1, sz - tsz, fp))){
			if (n > 0){
				tsz += n;
			}
			else  if (errno != EINTR &&
				errno != EAGAIN && errno != EWOULDBLOCK) {
                GLOG_ERR("write file:%s ret:%d error :%d writed sz:%zu total:%zu", file.c_str(), n, errno, tsz, sz);
				break;
			}
		}
		fclose(fp);
		if (tsz == sz){
			return tsz;
		}
		else {
            GLOG_ERR("write file:%s writed:%zu error :%d total sz:%zu", file.c_str(), tsz, errno, sz);
			return -2;
		}
	}
    //file://<path>
    //tcp://<ip:port>
    //udp://<ip:port>
    int                 openfd(const std::string & uri, int timeout_ms){
        if (uri.find("file://") == 0){ //+7
            int fd = open(uri.substr(7).c_str(), O_CREAT | O_RDWR);
            if (fd >= 0){
                nonblockfd(fd, true);
            }
            return fd;
        }
        else if (uri.find("tcp://") == 0){//+6
            const char * connaddr = uri.substr(6).c_str();
            sockaddr_in iaddr;
            int ret = socknetaddr(iaddr, connaddr);
            if (ret){
                GLOG_ERR("error tcp address:%s", connaddr);
                return -1;
            }
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0){
                GLOG_ERR("create sock stream error !");
                return -2;
            }
            socklen_t len = sizeof(sockaddr_in);
            nonblockfd(fd, true);
            ret = connect(fd, (struct sockaddr*)&iaddr, len);
            if (ret){
                if (errno == EINPROGRESS){
                    ret = waitfd_writable(fd, timeout_ms);
                }
                if (ret){
                    GLOG_ERR("connect addr:%s error !", connaddr);
                    close(fd);
                    return -3;
                }
            }
            return fd;
        }
        else if (uri.find("udp://") == 0){//+6
            const char * connaddr = uri.substr(6).c_str();
            sockaddr_in iaddr;
            int ret = socknetaddr(iaddr, connaddr);
            if (ret){
                GLOG_ERR("error net udp address:%s", connaddr);
                return -1;
            }
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0){
                GLOG_ERR("create sock stream error !");
                return -2;
            }
            socklen_t len = sizeof(sockaddr_in);
            nonblockfd(fd, true);
            ret = connect(fd, (struct sockaddr*)&iaddr, len);
            if (ret){
                GLOG_ERR("connect addr:%s error !", connaddr);
                close(fd);
                return -3;
            }
            return fd;
        }
        else if (uri.find("http://") == 0){//7
            std::vector<string> vs;
            strsplit(uri.substr(7), "/", vs, true, 2);           
            if (vs.empty()){
                GLOG_ERR("uri path is error :%s", uri.c_str());
                return -1;
            }
            std::vector<string> tcpvs;
            strsplit(vs[0], ":", tcpvs);
            string tcpuri = "tcp://" + vs[0];
            if (tcpvs.size() < 2){
                tcpuri += ":80";//default
            }
            int fd = openfd(tcpuri, timeout_ms);
            if (fd < 0){
                GLOG_ERR("open tcp uri error:%s http uri:%s", tcpuri.c_str(), uri.c_str());
                return -2;
            }
            string httpget = "GET /";
            if (vs.size() == 2){
                httpget += vs[1];
            }
            httpget += " HTTP/1.1\r\n";
            httpget += "Connection: Close\r\n";
            httpget += "Host: ";
            httpget += tcpvs[0];
            httpget += "\r\n\r\n";
            int ret = writefd(fd, httpget.data(), httpget.length(), timeout_ms);
            if (ret != (int)httpget.length()){
                GLOG_ERR("write http get request:%s error ret:%d", httpget.c_str(), ret);
                closefd(fd);
                return -3;
            }
            return fd;
        }
        else {
            GLOG_ERR("not support uri:%s", uri.c_str());
            return -1;
        }
        return -2;
    }
    static inline int _readfd(int fd, char * buffer, size_t sz, int timeout_ms){
        int n = 0;
        size_t tsz = 0;
        while ((sz > tsz) && (n = read(fd, buffer + tsz, sz - tsz))){
            if (n > 0){
                tsz += n;
            }
            else if (errno != EINTR){
                if (errno == EAGAIN || errno == EWOULDBLOCK){//
                    if (waitfd_readable(fd, timeout_ms)){
                        GLOG_ERR("read fd:%d time out:%dms tsz:%zd", fd, timeout_ms, tsz);
                        return -1;
                    }
                }
                else {
                    GLOG_ERR("read fd:%d ret:%d error :%d(%s) total sz:%zd", fd, n, errno, strerror(errno), tsz);
                    return -2;
                }
            }
        }
        return tsz;
    }
    //mode: size, end, msg:sz32/16/8, token:\r\n\r\n , return > 0 read size, = 0 end, < 0 error
    int                 readfd(int fd, char * buffer, size_t sz, const char * mode, int timeout_ms){
        if (sz == 0 || !buffer || fd < 0){
            return 0;
        }
        if (strstr(mode, "size")){
            //just read sz or end
            size_t tsz = _readfd(fd, buffer, sz, timeout_ms);
            if (sz != tsz){
                GLOG_ERR("read fd:%d size sz:%zd tsz:%zd not match !", fd, sz, tsz);
                return -1;
            }
            return tsz;
        }
        else if (strstr(mode, "end")){
            return _readfd(fd, buffer, sz, timeout_ms);
        }
        else if (strstr(mode, "msg:")){
            //read 32
            int    nrsz = 0;
            int szlen = 0;
            if (strstr(mode, ":sz32")){
                szlen = sizeof(uint32_t);
            }
            else if (strstr(mode, ":sz16")){
                szlen = sizeof(uint16_t);
            }
            else if (strstr(mode, ":sz8")){
                szlen = sizeof(uint8_t);
            }
            else {
                GLOG_ERR("read fd:%d error mode:%s",fd, mode);
                return -1;
            }
            if (_readfd(fd, (char *)&nrsz, szlen, timeout_ms)!= szlen){
                GLOG_ERR("read fd:%d size16 head error !", fd);
                return -2;
            }
            if (szlen == 16){
                nrsz = ntohs(nrsz);
            }
            else if (szlen == 32){
                nrsz = ntohl(nrsz);
            }
            if ((int)sz < nrsz){
                GLOG_ERR("read fd:%d buffer size:%d not enough %zd  ", fd, nrsz, sz);
                return -3;
            }
            if (_readfd(fd, buffer, nrsz, timeout_ms) != nrsz){
                GLOG_ERR("read fd:%d buffer data erorr size:%d", fd, nrsz);
                return -4;
            }
            return nrsz;
        }
        else if (strstr(mode, "token:")){
            const char * sep = mode + 6;
            if (!*sep){ //blocksz == 0
                GLOG_ERR("error mode:%s", mode);
                return -1;
            }
            int nrsz = 0;
            int seplen = strlen(sep);
            int blocksz = strlen(sep);
            int rdz = 0;
            int matchnum = 0;
            while (true){
                rdz = _readfd(fd, buffer + nrsz, blocksz, timeout_ms);
                if (rdz < 0){
                    GLOG_ERR("read fd:%d ret:%d error !", fd, rdz);
                    return rdz;
                }
                nrsz += rdz;
                //=============
                if (rdz == 0){
                    return nrsz;
                }
                matchnum = 0; //matched num
                while (matchnum < seplen && *(buffer + nrsz - rdz + matchnum) == *(sep + matchnum)){
                    ++matchnum;
                }
                if (matchnum == seplen){ //matched all
                    return nrsz;
                }
                blocksz = seplen - matchnum;
            }
        }
        else {
            GLOG_ERR("not support mode:%s", mode);
            return -1;
        }
    }
    //write size , return > 0 wirte size, <= 0 error
    int         writefd(int fd, const char * buffer, size_t sz, int timeout_ms){
		if (fd < 0){ return -1; }
        if (sz == 0){
            sz = strlen(buffer);
        }
        size_t tsz = 0;
        int n = 0;
        while (sz > tsz && (n = write(fd, buffer + tsz, sz - tsz))){
            if (n > 0){
                tsz += n;
            }
            else  if (errno != EINTR){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    if (waitfd_writable(fd, timeout_ms)){
                        GLOG_ERR("write fd:%d time out:%dms tsz:%zd", fd, timeout_ms, tsz);
                        return -1;
                    }
                }
				else {
					GLOG_ERR("write fd:%d ret:%d error :%d(%s) total sz:%zd write:%zd", fd, n, errno, strerror(errno), sz, tsz);
					return -2;
				}
            }
        }
        return tsz;
    }
    int        closefd(int fd){
        int ret = close(fd);
        if (ret){
            GLOG_ERR("close fd:%d error !", fd);
        }
        return ret;
    }
    int         nonblockfd(int fd, bool nonblock){
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0){
            return -1;
        }
        if (nonblock){
            flags |= O_NONBLOCK;
        }
        else{
            flags &= ~(O_NONBLOCK);
        }
        if (fcntl(fd, F_SETFL, flags) < 0){
            return -1;
        }
        return 0;
    }
    //return 0: readable , other error occurs
    int         waitfd_readable(int fd, int timeout_ms){
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000 ) * 1000;
        int ret = select(fd + 1, &fdset, NULL, NULL, &tv);
        if (ret > 0){
            assert(FD_ISSET(fd, &fdset));
            return 0;
        }
        return -1;
    }
    int         socknetaddr(sockaddr_in & addr, const std::string & saddr){
        memset(&addr, 0, sizeof(addr));
        addr.sin_addr.s_addr = 0;        
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        std::vector<string> vs;
        strsplit(saddr, ":", vs);
        if (strisint(vs[0])){
            addr.sin_addr.s_addr = stol(vs[0]);
        }
        uint32_t a, b, c, d;
        if (sscanf(vs[0].c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4){
            //net order = big endia order 
            addr.sin_addr.s_addr = a | (b << 8) | (c << 16) | (d << 24);
        }
        else { //must be a domainname
            int num = 1;
            if (ipfromhostname(&addr.sin_addr.s_addr, num, vs[0])){
                GLOG_ERR("get host name error first name:%s hostname:%s", vs[0].c_str(), saddr.c_str());
                return -1;
            }
        }
        if (vs.size() == 2){
            addr.sin_port = htons(stoi(vs[1]));
        }
        return 0;
    }
    int         ipfromhostname(OUT uint32_t * ip, INOUT int & ipnum, const std::string & hostname){
        int nmaxip = ipnum;
        ipnum = 0;
        struct hostent hosts;
        struct hostent * result;
        int h_errnop;
        char buffer[512];
        int ret = gethostbyname_r(hostname.c_str(),
            &hosts, buffer, sizeof(buffer),
            &result, &h_errnop);
        if (ret){
            GLOG_ERR("gethost by name error :%s ", hstrerror(h_errnop));
            return -1;
        }
        while (ipnum < nmaxip && result->h_addr_list[ipnum]){
            *ip = *(uint32_t*)result->h_addr_list[ipnum];
            ++ipnum;
        }
        return 0;
    }
    uint32_t    localhost_getipv4(const char * nic){
        struct ifreq _temp;
        struct sockaddr_in *soaddr;
        int fd = 0;
        int ret = -1;
        strcpy(_temp.ifr_name, nic);
        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            return 0;
        }
        ret = ioctl(fd, SIOCGIFADDR, &_temp);
        close(fd);
        if (ret < 0){
            return 0;
        }
        soaddr = (struct sockaddr_in *)&(_temp.ifr_addr);
        return soaddr->sin_addr.s_addr;
    }
    string      stripfromu32v4(uint32_t ip){
        char tmpbuff[64] = { 0 };
        if (nullptr == inet_ntop(AF_INET, &ip, tmpbuff, sizeof(tmpbuff))){
            return "";
        }
        return string(tmpbuff);
    }
    uint32_t    u32fromstripv4(const string & ip){
        uint32_t uip;
        if (1 == inet_pton(AF_INET, ip.c_str(), &uip)){
            return uip;
        }
        return 0;
    }
    string      host_getmac(const char * nic){
        struct ifreq _temp;
        char mac_addr[64] = { 0 };
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0){
            return "";
        }
        memset(&_temp, 0, sizeof(struct ifreq));
        strncpy(_temp.ifr_name, nic, sizeof(_temp.ifr_name) - 1);
        if ((ioctl(sockfd, SIOCGIFHWADDR, &_temp)) < 0){
            close(sockfd);
            return "";
        }
        close(sockfd);
        snprintf(mac_addr,sizeof(mac_addr)-1, "%02x%02x%02x%02x%02x%02x",
            (unsigned char)_temp.ifr_hwaddr.sa_data[0],
            (unsigned char)_temp.ifr_hwaddr.sa_data[1],
            (unsigned char)_temp.ifr_hwaddr.sa_data[2],
            (unsigned char)_temp.ifr_hwaddr.sa_data[3],
            (unsigned char)_temp.ifr_hwaddr.sa_data[4],
            (unsigned char)_temp.ifr_hwaddr.sa_data[5]);
        return mac_addr;
    }

    int         waitfd_writable(int fd, int timeout_ms){
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int ret = select(fd + 1, NULL, &fdset, NULL, &tv);
        if (ret > 0){
            assert(FD_ISSET(fd, &fdset));
            return 0;
        }
        return -1;
    }
    int			lockpidfile(const std::string & file, int kill_other_sig, bool nb){
        int fd = open(file.c_str(), O_RDWR | O_CREAT, 0644);
		if (fd == -1) {
            GLOG_ERR("open file:%s error ", file.c_str());
			return -1;
		}
		int flags = LOCK_EX;
		if (nb){
			flags |= LOCK_NB;
		}
		char szpid[16] = { 0 };
		int pid = 0;
		while (flock(fd, flags) == -1) {
			if (pid == 0){ //just read once
                int n = readfile(file, szpid, sizeof(szpid));
				if (n > 0){
					pid = strtol(szpid, NULL, 10);
                    GLOG_ERR("lock pidfile:%s fail , the file is held by pid %d", file.c_str(), pid);
				}
				else {
                    GLOG_ERR("lock pidfile:%s fail but read pid from file error !", file.c_str());
				}
			}
			if (pid > 0 && kill_other_sig > 0){
				if (kill(pid, kill_other_sig) && errno == ESRCH){
                    GLOG_WAR("killed the pidfile locker:%d by signal:%d", pid, kill_other_sig);
					break;
				}
			}
			else {
				return pid;
			}
		}
		pid = getpid();
		snprintf(szpid, sizeof(szpid), "%d", pid);
        writefile(file, szpid);

		return pid;
	}
    int			strsplit(const std::string & str, const string & sep, std::vector<std::string> & vs,
                        bool ignore_empty, int maxsplit, int beg_ , int end_ ){
		vs.clear();
        string::size_type beg = beg_;
        string::size_type end = str.length();
        if (end_ > 0){
            end = end_;
        }
		string::size_type pos = 0;
		//if pos not found add the rest then return , else add substr . again
		do {
			pos = str.find(sep, beg);
			if ( pos != string::npos && pos < end ){
				if (pos > beg){
					vs.push_back(str.substr(beg, pos - beg));
				}
				else if (!ignore_empty){
					vs.push_back(""); //empty 
				}
				beg = pos + sep.length();
			}
			if (pos == string::npos || //last one
				(maxsplit > 0 && (int)vs.size() + 1 == maxsplit) ||
                pos > end){
				if (beg < end){
					vs.push_back(str.substr(beg, end - beg));
				}
				else if (!ignore_empty){
					vs.push_back(""); //empty 
				}
				return vs.size();
			}
		} while (true);
		return vs.size();
	}
    bool                time_same_hour(time_t t1, time_t t2){
        return t1 / 3600 == t2 / 3600;
    }
    bool                time_same_week(time_t t1, time_t t2){
        struct tm _sftm;
        localtime_r(&t1, &_sftm);
        int d1 = _sftm.tm_wday;//0-6
        localtime_r(&t2, &_sftm);
        int d2 = _sftm.tm_wday;//0-6
        return d1 == d2 && time_same_year(t1, t2) && abs(t1-t2) < (7*86400);
    }
    bool                time_same_year(time_t t1, time_t t2){
        struct tm _sftm;
        localtime_r(&t1, &_sftm);
        int d1 = _sftm.tm_year;//1-31
        localtime_r(&t2, &_sftm);
        int d2 = _sftm.tm_year;//1-31
        return d1 == d2;
    }
    bool                time_same_day(time_t t1, time_t t2){
        struct tm _sftm;
        localtime_r(&t1, &_sftm);
        int d1 = _sftm.tm_yday;//0-365
        localtime_r(&t2, &_sftm);
        int d2 = _sftm.tm_yday;//0-365
        return d1 == d2 && time_same_year(t1, t2);
    }
    bool                time_same_month(time_t t1, time_t t2){
        struct tm _sftm;
        localtime_r(&t1, &_sftm);
        int m1 = _sftm.tm_mon;//0-11
        localtime_r(&t2, &_sftm);
        int m2 = _sftm.tm_mon;//0-11
        return m1 == m2 && time_same_year(t1, t2);
    }

	const char*		strftime(std::string & str, time_t unixtime, const char * format){
		str.reserve(32);
		if (unixtime == 0U){
			unixtime = time(NULL);
		}
		struct tm _sftm;
		localtime_r(&unixtime, &_sftm);
		strftime((char*)str.c_str(), str.capacity(), format, &_sftm);
		return str.c_str();
	}
    const char*         strptime(time_t & unixtime ,const std::string & str, const char * format){
        struct tm _tmptm;
        unixtime = 0;
        const char * p = strptime(str.c_str(), format, &_tmptm);
        if(!p){
            return nullptr;
        }
        unixtime = mktime(&_tmptm);
        return p;
    }
	time_t			    stdstrtime(const char * strtime){
        time_t _tmt;
        strptime(_tmt, strtime);
        return _tmt;
	}
    bool            strisint(const std::string & str, int base){
        char * endptr;
        if (str.empty()){
            return false;
        }
        auto v = strtoll(str.c_str(), &endptr, base);
        //if *nptr is not '\0' but **endptr 
        if (*endptr){//should be last char \0
            return false;
        }
        if (v == LLONG_MAX || v == LLONG_MIN){
            return false;
        }
        return true;
    }
	void			strrepeat(std::string & str, const char * rep, int repcount){
		while (repcount-- > 0){
			str.append(rep);
		}
	}
	size_t			vstrprintf(std::string & str, const char* format, va_list ap){
		size_t ncvt = vsnprintf((char*)str.data(), str.capacity(), format, ap);
		if (ncvt == str.capacity()){
			str[ncvt - 1] = 0;
			--ncvt;
		}
		return ncvt;
	}
	size_t			strprintf(std::string & str, const char * format, ...){
		va_list	ap;
		va_start(ap, format);
		size_t ncvt = vstrprintf(str, format, ap);
		va_end(ap);
		return ncvt;
	}
	size_t			strnprintf(std::string & str, size_t max_sz, const char * format, ...){
		str.reserve(max_sz);
		va_list	ap;
		va_start(ap, format);
		size_t ncvt = vstrprintf(str, format, ap);
		va_end(ap);
		return ncvt;
	}


	//////////////////////////////////////////////////////////////////////////////////////////
    const char*			strcharsetrandom(std::string & randoms, int length , const char * charset){
        if (!charset || !(*charset)){
            return nullptr;
        }
        int charsetlen = strlen(charset);
        std::random_device	rd;
        for (int i = 0; i < length; ++i){
            randoms.append(1, charset[rd() % charsetlen]);
        }
        return randoms.c_str();
    }
    const char*			strrandom(std::string & randoms, int length, char charbeg, char charend){
		if (charbeg > charend){std::swap(charbeg, charend);}
		std::random_device	rd;
		for (int i = 0; i < length; ++i){
			randoms.append(1, (char)(rd() % (charend - charbeg + 1) + charbeg));
		}
		return randoms.c_str();
	}
    string &            strreplace(string & str, const string & sub, const string & repl, bool global){
        string::size_type found = str.find(sub);
        if (global){
            while (found != string::npos){
                str.replace(found, sub.length(), repl);
                found = str.find(sub);
            }
        }
        else if (found != string::npos){
            str.replace(found, sub.length(), repl);
        }
        return str;
    }
    string &            strrereplace(string & str, const string & repattern, const string & repl){
        str = std::regex_replace(str, std::regex(repattern), repl);
        return str;
    }
    bool                strrefind(string & str, const string & repattern, std::match_results<string::const_iterator>& m){
        return std::regex_search(str, m, std::regex(repattern));
    }

    int                 strsunpack(const std::string & str, const std::string & sep, const std::string & ks, ...){
        //{K=V,}
        if (str[0] != '{' || *str.rbegin() != '}'){
            return -1;//valid format
        }
        //name=v
        std::map<string, string>      kvmap;
        std::vector<string> vs;
		string kvsep;
		strrepeat(kvsep, sep.c_str(), 2);//,
		strsplit(str, kvsep, vs, true, 0, 1, str.length() - 1); //{<---------->}
        for (auto & kv : vs){
            std::vector<string> skv;
            int lkv = strsplit(kv, sep, skv, true, 2);//k=v
            if (lkv == 2){
                kvmap[skv[0]] = skv[1];
            }
        }
		/////////////////////////////////////////
		std::vector<string> vsks;
		strsplit(ks, ",", vsks, true);
		va_list ap;
		va_start(ap, ks);
		for (auto & k : vsks){
			//k=v
			string * v = va_arg(ap, std::string *);
			v->assign(kvmap[k]);
		}
		va_end(ap);
        return vs.size();
    }
    const char         *strspack(std::string & str, const std::string & sep, const std::string & ks, ...){
        //{K=V,}
        str = "{";
        //name=v
        std::vector<string> vs;
        strsplit(ks, ",", vs, true);
        va_list ap;
        va_start(ap, ks);
		for (size_t i = 0; i < vs.size(); ++i){
            if (i > 0){
                strrepeat(str, sep.c_str(), 2);//,
            }
            //k=v
            str += vs[i];//k
            const string * v = va_arg(ap, std::string *);
            strrepeat(str, sep.c_str(), 1);//=
            str += *v;
        }
        va_end(ap);
        str += "}";
        return str.data();
    }











}



