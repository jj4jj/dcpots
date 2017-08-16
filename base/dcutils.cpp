#include "dcutils.hpp"
#include "logger.h"
////////////////////////
#include<sys/ioctl.h>
#include<sys/socket.h>
#include<net/if.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <dlfcn.h>
#include <dirent.h>

#include "dcbitset.hpp"
#define FS_PATH_SEP ('/')
//////////////////////////////////////////////////////////////////////////////////

namespace {
    static const char s_b64_lookup_c2d[] = ""
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x00
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x10
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x3e\x80\x80\x80\x3f" // 0x20
        "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x80\x80\x80\x00\x80\x80" // 0x30
        "\x80\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e" // 0x40
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x80\x80\x80\x80\x80" // 0x50
        "\x80\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28" // 0x60
        "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x80\x80\x80\x80\x80" // 0x70
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x80
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x90
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xa0
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xb0
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xc0
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xd0
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xe0
        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xf0
        "";
    static const char s_b64_lookup_d2c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static inline unsigned char s_hex_lookup_c2d(int c){
        return  (c >= '0' && c <= '9') ? c - '0' : (
                (c >= 'a' && c <= 'f') ? c - 'a' + 10: (
                (c >= 'A' && c <= 'F') ? c - 'A' + 10: 0 ) );
    }
    static inline char s_hex_lookup_d2c(unsigned char d){
        static const char s_hex_lookup[] = "0123456789abcdef";
        return s_hex_lookup[d];
    }

};

namespace dcs {
	uint64_t	time_unixtime_ns(){
		#if 0
		int clock_getres(clockid_t clk_id, struct timespec *res);
		int clock_gettime(clockid_t clk_id, struct timespec *tp);
			CLOCK_REALTIME
			System - wide  clock  that measures real(i.e., wall - clock) time.Setting this clock requires appropriate privileges.This clock is affected by discontinuous jumps in the
			system time(e.g., if the system administrator manually changes the clock), and by the incremental adjustments performed by adjtime(3) and NTP.
			CLOCK_REALTIME_COARSE(since Linux 2.6.32; Linux - specific)
			A faster but less precise version of CLOCK_REALTIME.Use when you need very fast, but not fine - grained timestamps.
			CLOCK_MONOTONIC
			Clock that cannot be set and represents monotonic time since some unspecified starting point.This clock is not affected by discontinuous jumps in the system time
			(e.g., if the system administrator manually changes the clock), but is affected by the incremental adjustments performed by adjtime(3) and NTP.
			CLOCK_MONOTONIC_COARSE(since Linux 2.6.32; Linux - specific)
			A faster but less precise version of CLOCK_MONOTONIC.Use when you need very fast, but not fine - grained timestamps.
			CLOCK_MONOTONIC_RAW(since Linux 2.6.28; Linux - specific)
			Similar  to  CLOCK_MONOTONIC, but  provides access to a raw hardware - based time that is not subject to NTP adjustments or the incremental adjustments performed by
			adjtime(3).
			CLOCK_BOOTTIME(since Linux 2.6.39; Linux - specific)
			Identical to CLOCK_MONOTONIC, except it also includes any time that the system is suspended.This allows applications to get a suspend - aware monotonic clock with©\
			out having to deal with the complications of CLOCK_REALTIME, which may have discontinuities if the time is changed using settimeofday(2).
			CLOCK_PROCESS_CPUTIME_ID(since Linux 2.6.12)
			Per - process CPU - time clock(measures CPU time consumed by all threads in the process).
			CLOCK_THREAD_CPUTIME_ID(since Linux 2.6.12)
			Thread - specific CPU - time clock.
		#endif
		struct timespec tp;
		clock_gettime(CLOCK_REALTIME, &tp);
		return (tp.tv_sec*1000000000 + tp.tv_nsec);
	}
    uint64_t	time_unixtime_us() {
        timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000 + tv.tv_usec;
    }

    int			daemonlize(int closestd, int chrootdir, const char * pidfile) {
        //1.try lock file
        int lockfd = -1;
        if (pidfile) {
            lockfd = lockfile(pidfile);
            if (lockfd == -1) {
                GLOG_ERR("try lock file:%s error ...", pidfile);
                exit(-1);
            }
            else {
                unlockfile(lockfd);
            }
        }
        int ret = 0;
#if _BSD_SOURCE || (_XOPEN_SOURCE && _XOPEN_SOURCE < 500)
        ret = daemon(!chrootdir, !closestd);
#else
        assert("not implement in this platform , using nohup & launch it ?")
            ret = -404;//todo 
#endif
        if (pidfile) {
            int pidget = lockpidfile(pidfile);
            if (pidget != getpid()) {
                GLOG_ERR("error daemonlization when lockpidfile:%s ret=%d ... errno:%d (%s)",
                         pidfile, pidget, errno, strerror(errno));
                exit(-2);
            }
        }
        if (ret) {
            GLOG_ERR("error daemonlization ... errno:%d (%s)", errno, strerror(errno));
            exit(ret);
        }
        return 0;
    }
    typedef  std::unordered_map<int, std::stack<sah_handler> >	signalh_stacks_t;
    static signalh_stacks_t	s_sigh_stacks;
    int					signalh_ignore(int sig) {
        return signalh_set(sig, (sah_handler)SIG_DFL);
    }
    int					signalh_default(int sig) {
        return signalh_set(sig, (sah_handler)SIG_DFL);
    }
    void				signalh_clear(int sig) {
        if (s_sigh_stacks.find(sig) == s_sigh_stacks.end()) {
            return;
        }
        while (!s_sigh_stacks[sig].empty()) {
            s_sigh_stacks[sig].pop();
        }
        signalh_default(sig);
    }
    ////////////////////////////////////////////////////////////////////////
    //typedef void(*sah_handler)(int sig, siginfo_t * sig_info, void * ucontex);
    int                 signalh_set(int sig, sah_handler sah, int sah_flags) {
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_sigaction = sah;
        //act.sa_mask = sah_mask;
        act.sa_flags = sah_flags | SA_SIGINFO;
        return sigaction(sig, &act, NULL);
    }
    int					signalh_push(int sig, sah_handler sah, int sah_flags) {
        if (s_sigh_stacks.find(sig) == s_sigh_stacks.end()) {
            s_sigh_stacks[sig] = std::stack<sah_handler>();
        }        
        int ret = signalh_set(sig, sah, sah_flags);
        if (ret) {
            return ret;
        }
        s_sigh_stacks[sig].push((sah_handler)sah);
        return 0;
    }
    sah_handler			signalh_pop(int sig) {
        if (s_sigh_stacks.find(sig) == s_sigh_stacks.end() ||
            s_sigh_stacks[sig].empty()) {
            return (sah_handler)SIG_DFL;
        }
        sah_handler sah = s_sigh_stacks[sig].top();
        s_sigh_stacks[sig].pop();
        return sah;
    }
    bool		file_exists(const std::string & file) {
        FILE * fp = fopen(file.c_str(), "r");
        fclose(fp);
        return fp != nullptr;
    }
    int			readfile(const std::string & file, std::string & content) {
        char filebuff[512];
        content.clear();
        content.reserve(1024 * 8);
        FILE * fp = fopen(file.c_str(), "r");
        if (!fp) {
            GLOG_SER("open file:%s !", file.c_str());
            return -1;
        }
        while (true) {
            int tsz = fread(filebuff, 1, sizeof(filebuff), fp);
            if (tsz == sizeof(filebuff)) {
                content.append(filebuff, tsz);
            }
            else if (feof(fp)) {
                content.append(filebuff, tsz);
                fclose(fp);
                return content.length();
            }
            else {
                GLOG_SER("read file:%s ret:%zd error :%d ", file.c_str(), tsz, errno);
                fclose(fp);
                return -2;
            }
        }
        return -3;
    }
    int			readfile(const std::string & file, char * buffer, size_t sz) {
        FILE * fp = fopen(file.c_str(), "r");
        if (!fp) {
            GLOG_SER("open file:%s error!", file.c_str());
            return -1;
        }
        if (sz == 0 || !buffer) {
            fclose(fp);
            return 0; //file exists
        }
        size_t tsz = fread(buffer, 1, sz, fp);
        if (feof(fp)) {
            if (tsz < sz) {
                buffer[tsz] = 0;
            }
            fclose(fp);
            return tsz;
        }
        GLOG_SER("read file:%s error :%d total sz:%zu", file.c_str(), errno, tsz);
        fclose(fp);
        return -2;
    }
#if 0
	struct stat {
		dev_t     st_dev;         /* ID of device containing file */
		ino_t     st_ino;         /* inode number */
		mode_t    st_mode;        /* protection */
		nlink_t   st_nlink;       /* number of hard links */
		uid_t     st_uid;         /* user ID of owner */
		gid_t     st_gid;         /* group ID of owner */
		dev_t     st_rdev;        /* device ID (if special file) */
		off_t     st_size;        /* total size, in bytes */
		blksize_t st_blksize;     /* blocksize for filesystem I/O */
		blkcnt_t  st_blocks;      /* number of 512B blocks allocated */

								  /* Since Linux 2.6, the kernel supports nanosecond
								  precision for the following timestamp fields.
								  For the details before Linux 2.6, see NOTES. */

		struct timespec st_atim;  /* time of last access */
		struct timespec st_mtim;  /* time of last modification */
		struct timespec st_ctim;  /* time of last status change */
#endif
	time_t		file_modify_time(const std::string & file) {
		struct stat lfst;
		int ret = stat(file.c_str(), &lfst);
		if (ret) {
			return 0;
		}
		return lfst.st_mtime;//.tv_sec;
	}
	time_t		file_access_time(const std::string & file) {
		struct stat lfst;
		int ret = stat(file.c_str(), &lfst);
		if (ret) {
			return 0;
		}
		return lfst.st_atime;//.tv_sec;
	}

    size_t      file_size(const std::string & file) {
        FILE * fp = fopen(file.c_str(), "r");
        if (!fp) {
            return 0;//not exist 
        }
        fseek(fp, 0L, SEEK_END);
        size_t sz = ftell(fp);
        fclose(fp);
        return sz;
    }
    const char *		path_base(const char * path) {
        if(!path || !*path){
            return ".";
        }
        const char * p = path + strlen(path) - 1;
        while(*p != FS_PATH_SEP && p != path){--p;}
        if(*p == FS_PATH_SEP) ++p;
        if(*p == 0){
            return ".";
        }
        return p;
    }
    string 				path_dir(const char * path){
        std::string strpath = path;
        strpath.erase(strpath.find_last_of(FS_PATH_SEP));
        if(strpath.empty()){
            return ".";
        }
        return strpath;
    }
    int			writefile(const std::string & file, const char * buffer, size_t sz) {
        FILE * fp = fopen(file.c_str(), "w");
        if (!fp) {
            GLOG_SER("open file %s e!", file.c_str());
            return -1;
        }
        if (sz == 0) {
            sz = strlen(buffer);
        }
        size_t tsz = fwrite(buffer, 1, sz, fp);
        fclose(fp);
        if (tsz == sz) {
            return tsz;
        }
        else {
            GLOG_SER("write file:%s writed:%zu error :%d total sz:%zu", file.c_str(), tsz, errno, sz);
            return -2;
        }
    }
    int                 path_list(std::vector<std::string> & file_list, const char * path, int filter){
        file_list.clear();
        DIR *dfd = opendir(path);
        if (!dfd) {
            fprintf(stderr, "open dir = %s error !", path);
            return -1;
        }
#if 0
        struct dirent {
            ino_t          d_ino;       /* inode number */
            off_t          d_off;       /* offset to the next dirent */
            unsigned short d_reclen;    /* length of this record */
            unsigned char  d_type;      /* type of file; not supported
                                            by all file system types */
            char           d_name[256]; /* filename */
        };
        DT_BLK      This is a block device.
        DT_CHR      This is a character device.
        DT_DIR      This is a directory.
        DT_FIFO     This is a named pipe(FIFO).
        DT_LNK      This is a symbolic link.
        DT_REG      This is a regular file.
        DT_SOCK     This is a UNIX domain socket.
        DT_UNKNOWN  The file type is unknown.
#endif
        int dtype_filter = 0;
        if(filter&PATH_IS_BLK){
            dtype_filter |= DT_BLK;
        }
        if (filter&PATH_IS_CHAR) {
            dtype_filter |= DT_CHR;
        }
        if (filter&PATH_IS_DIR) {
            dtype_filter |= DT_DIR;
        }
        if (filter&PATH_IS_FIFO) {
            dtype_filter |= DT_FIFO;
        }
        if (filter&PATH_IS_LINK) {
            dtype_filter |= DT_LNK;
        }
        if (filter&PATH_IS_FILE) {
            dtype_filter |= DT_REG;
        }
        if (filter&PATH_IS_SOCKET) {
            dtype_filter |= DT_SOCK;
        }
        //////////////////////////////////////////////////////////////////////////
        int name_max = pathconf(path, _PC_NAME_MAX);
        if (name_max == -1)         /* Limit not defined, or error */
            name_max = 255;         /* Take a guess */
        int len = offsetof(struct dirent, d_name) + name_max + 1;
        struct dirent * entryp = (struct dirent * )malloc(len);
        struct dirent *dp_itr = NULL;
        int ret = 0;
        while(true){
            ret = readdir_r(dfd, entryp, &dp_itr);
            if(ret || !dp_itr){
                break;
            }
            if (strcmp(dp_itr->d_name, ".") == 0
                || strcmp(dp_itr->d_name, "..") == 0) {
                continue;    /* skip self and parent */
            }
            if((dp_itr->d_type)&dtype_filter){
                file_list.push_back(dp_itr->d_name);
            }
        }
        free(entryp);
        closedir(dfd);
        return ret;
    }

    //write size , return > 0 wirte size, <= 0 error
    static inline	int  _writefd(int fd, const char * buffer, size_t sz, int timeout_ms) {
        if (fd < 0 || sz == 0) { return -1; }
        size_t tsz = 0;
        int n = 0;
        while (sz > tsz && (n = write(fd, buffer + tsz, sz - tsz))) {
            if (n > 0) {
                tsz += n;
            }
            else  if (errno != EINTR) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (waitfd_writable(fd, timeout_ms)) {
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
    //file://<path>
    //tcp://<ip:port>
    //udp://<ip:port>
    //mode:r:listen/w:connect
    int                 openfd(const std::string & uri, const char * mode, int timeout_ms) {
        if (uri.find("file://") == 0) { //+7
            int fd = open(uri.substr(7).c_str(), O_CREAT | O_RDWR);
            if (fd >= 0) {
                nonblockfd(fd, true);
            }
            return fd;
        }
        else if (uri.find("tcp://") == 0) {//+6
            const char * skaddr = uri.substr(6).c_str();
            sockaddr_in iaddr;
            int ret = socknetaddr(iaddr, skaddr);
            if (ret) {
                GLOG_ERR("error tcp address:%s", skaddr);
                return -1;
            }
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) {
                GLOG_SER("create sock stream error !");
                return -2;
            }
            socklen_t len = sizeof(sockaddr_in);
            nonblockfd(fd, true);
            if (strcmp(mode, "w") == 0) {
                ret = connect(fd, (struct sockaddr*)&iaddr, len);
                if (ret) {
                    if (errno == EINPROGRESS) {
                        ret = waitfd_writable(fd, timeout_ms);
                    }
                    if (ret) {
                        GLOG_SER("connect addr:%s error !", skaddr);
                        close(fd);
                        return -3;
                    }
                }
            }
            else {
                ret = bind(fd, (struct sockaddr*)&iaddr, len);
                if (ret) {
                    GLOG_SER("bind addr:%s error !", skaddr);
                    close(fd);
                    return -4;
                }
                ret = listen(fd, 1024);
                if (ret) {
                    GLOG_SER("listen addr:%s error !", skaddr);
                    close(fd);
                    return -5;
                }
            }
            return fd;
        }
        else if (uri.find("udp://") == 0) {//+6
            const char * connaddr = uri.substr(6).c_str();
            sockaddr_in iaddr;
            int ret = socknetaddr(iaddr, connaddr);
            if (ret) {
                GLOG_ERR("error net udp address:%s", connaddr);
                return -1;
            }
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0) {
                GLOG_SER("create sock stream error !");
                return -2;
            }
            socklen_t len = sizeof(sockaddr_in);
            nonblockfd(fd, true);
            if (strcmp(mode, "w") == 0) {
                ret = connect(fd, (struct sockaddr*)&iaddr, len);
                if (ret) {
                    GLOG_SER("udp bind addr:%s error !", connaddr);
                    close(fd);
                    return -3;
                }
            }
            else {
                ret = bind(fd, (struct sockaddr*)&iaddr, len);
                if (ret) {
                    GLOG_SER("udp bind addr:%s error !", connaddr);
                    close(fd);
                    return -4;
                }
            }
            return fd;
        }
        else if (uri.find("http://") == 0) {//7
            std::vector<string> vs;
            strsplit(uri.substr(7), "/", vs, true, 2);
            if (vs.empty()) {
                GLOG_ERR("uri path is error :%s", uri.c_str());
                return -1;
            }
            std::vector<string> tcpvs;
            strsplit(vs[0], ":", tcpvs);
            string tcpuri = "tcp://" + vs[0];
            if (tcpvs.size() < 2) {
                tcpuri += ":80";//default
            }
            int fd = openfd(tcpuri, "w", timeout_ms);
            if (fd < 0) {
                GLOG_ERR("open tcp uri error:%s http uri:%s", tcpuri.c_str(), uri.c_str());
                return -2;
            }
            string httpget = "GET /";
            if (vs.size() == 2) {
                httpget += vs[1];
            }
            httpget += " HTTP/1.1\r\n";
            httpget += "Connection: Close\r\n";
            httpget += "Host: ";
            httpget += tcpvs[0];
            httpget += "\r\n\r\n";
            int ret = _writefd(fd, httpget.data(), httpget.length(), timeout_ms);
            if (ret != (int)httpget.length()) {
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
    static inline int _readfd(int fd, char * buffer, size_t sz, int timeout_ms) {
        int n = 0;
        size_t tsz = 0;
        while ((sz > tsz) && (n = read(fd, buffer + tsz, sz - tsz))) {
            if (n > 0) {
                tsz += n;
            }
            else if (errno != EINTR) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {//
                    if (waitfd_readable(fd, timeout_ms)) {
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
    int                 readfd(int fd, char * buffer, size_t sz, const char * mode, int timeout_ms) {
        if (sz == 0 || !buffer || fd < 0 || !mode) {
            return 0;
        }
        if (strstr(mode, "size") == mode) {
            //just read sz or end
            size_t tsz = _readfd(fd, buffer, sz, timeout_ms);
            if (sz != tsz) {
                GLOG_ERR("read fd:%d size sz:%zd tsz:%zd not match !", fd, sz, tsz);
                return -1;
            }
            return tsz;
        }
        else if (strstr(mode, "end")) {
            return _readfd(fd, buffer, sz, timeout_ms);
        }
        else if (strstr(mode, "msg:")) {
            //read 32
            int    nrsz = 0;
            int szlen = 0;
            if (strstr(mode, ":sz32")) {
                szlen = sizeof(uint32_t);
            }
            else if (strstr(mode, ":sz16")) {
                szlen = sizeof(uint16_t);
            }
            else if (strstr(mode, ":sz8")) {
                szlen = sizeof(uint8_t);
            }
            else {
                GLOG_ERR("read fd:%d error mode:%s", fd, mode);
                return -1;
            }
            if (_readfd(fd, (char *)&nrsz, szlen, timeout_ms) != szlen) {
                GLOG_ERR("read fd:%d size16 head error !", fd);
                return -2;
            }
            if (szlen == 16) {
                nrsz = ntohs(nrsz);
            }
            else if (szlen == 32) {
                nrsz = ntohl(nrsz);
            }
            nrsz -= szlen;
            if ((int)sz < nrsz) {
                GLOG_ERR("read fd:%d buffer size:%d not enough %zd  ", fd, nrsz, sz);
                return -3;
            }
            if (_readfd(fd, buffer, nrsz, timeout_ms) != nrsz) {
                GLOG_ERR("read fd:%d buffer data erorr size:%d", fd, nrsz);
                return -4;
            }
            return nrsz + szlen;
        }
        else if (strstr(mode, "token:") == mode) {
            const char * sep = mode + 6;
            if (!*sep) { //blocksz == 0
                GLOG_ERR("error mode:%s", mode);
                return -1;
            }
            int nrsz = 0;
            int seplen = strlen(sep);
            int blocksz = strlen(sep);
            int rdz = 0;
            int matchnum = 0;
            while (true) {
                rdz = _readfd(fd, buffer + nrsz, blocksz, timeout_ms);
                if (rdz < 0) {
                    GLOG_ERR("read fd:%d ret:%d error !", fd, rdz);
                    return rdz;
                }
                nrsz += rdz;
                //=============
                if (rdz == 0) {
                    return nrsz;
                }
                matchnum = 0; //matched num
                while (matchnum < seplen && *(buffer + nrsz - rdz + matchnum) == *(sep + matchnum)) {
                    ++matchnum;
                }
                if (matchnum == seplen) { //matched all
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

    //mode: msg:sz32/16/8, token:\r\n\r\n , return > 0 write size, = 0 end, < 0 error
    int         writefd(int fd, const char * buffer, size_t sz, const char * mode, int timeout_ms) {
        if (fd < 0) { return -1; }
        if (sz == 0) {
            sz = strlen(buffer);
        }
        if (strstr(mode, "msg:") == mode) {
            //read 32
            size_t nwsz = sz;
            int szlen = 0;
            if (strstr(mode, ":sz32")) {
                szlen = sizeof(uint32_t);
                nwsz = UINT_MAX;
            }
            else if (strstr(mode, ":sz16")) {
                szlen = sizeof(uint16_t);
                nwsz = USHRT_MAX;
            }
            else if (strstr(mode, ":sz8")) {
                szlen = sizeof(uint8_t);
                nwsz = UCHAR_MAX;
            }
            else {
                GLOG_ERR("write fd:%d error mode:%s", fd, mode);
                return -1;
            }
            if (sz > nwsz) {
                GLOG_ERR("write fd:%d buffer size:%d not enough %zd  ", fd, nwsz, sz);
                return -3;
            }

            nwsz = szlen + sz;
            if (szlen == 16) {
                nwsz = htons(nwsz);
            }
            else if (szlen == 32) {
                nwsz = htonl(nwsz);
            }
            int n = _writefd(fd, (const char *)&nwsz, szlen, timeout_ms);
            if (n != szlen) {
                GLOG_ERR("write fd:%d buffer head size:%d write sz error ret:%d",
                         fd, szlen, n);
                return -4;
            }
            n = _writefd(fd, buffer, sz, timeout_ms);
            if ((size_t)n != sz) {
                GLOG_ERR("write fd:%d buffer size:%d write sz error ret:%d",
                         fd, sz, n);
                return -5;
            }
            return szlen + sz;
        }
        else if (strstr(mode, "token:") == mode) {
            const char * sep = mode + 6;
            if (!*sep) { //blocksz == 0
                GLOG_ERR("write fd:%d error mode:%s", fd, mode);
                return -1;
            }
            int n = _writefd(fd, buffer, sz, timeout_ms);
            if ((size_t)n != sz) {
                GLOG_ERR("write fd:%d buffer size:%d write sz error ret:%d",
                         fd, sz, n);
                return -2;
            }
            int seplen = strlen(sep);
            n = _writefd(fd, sep, seplen, timeout_ms);
            if (n != seplen) {
                GLOG_ERR("write fd:%d buffer token size:%d write sz error ret:%d",
                         fd, sz, n);
                return -3;
            }
            return seplen + sz;
        }
        else {
            return _writefd(fd, buffer, sz, timeout_ms);
        }
    }
    int        closefd(int fd) {
        int ret = close(fd);
        if (ret) {
            GLOG_ERR("close fd:%d error !", fd);
        }
        return ret;
    }
    bool        isnonblockfd(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            return false;
        }
        if (flags & O_NONBLOCK) {
            return true;
        }
        return false;
    }
    int         nonblockfd(int fd, bool nonblock) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            return -1;
        }
        if (nonblock) {
            flags |= O_NONBLOCK;
        }
        else {
            flags &= ~(O_NONBLOCK);
        }
        if (fcntl(fd, F_SETFL, flags) < 0) {
            return -1;
        }
        return 0;
    }
    //return 0: readable , other error occurs
    int         waitfd_readable(int fd, int timeout_ms) {
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int ret = select(fd + 1, &fdset, NULL, NULL, &tv);
        if (ret > 0) {
            assert(FD_ISSET(fd, &fdset));
            return 0;
        }
        return -1;
    }
    int         netaddr(struct sockaddr_in & addr, bool stream, const std::string & saddr){
        memset(&addr, 0, sizeof(addr));
        addr.sin_addr.s_addr = 0;
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        //////////////////////////////////////////////////////////////////////////        
        std::vector<string> vs;
        strsplit(saddr, ":", vs);
        if (vs.empty()) {
            GLOG_ERR("error format address:%s", saddr.c_str());
            return -1;
        }
        int port = 0;
        if (vs.size() == 2) {
            port = stoi(vs[1]);
        }
        char _port[6];  /* strlen("65535"); */
        snprintf(_port, 6, "%d", port);
        struct addrinfo hints, *servinfo;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = stream?SOCK_STREAM:SOCK_DGRAM;
        if (getaddrinfo(vs[0].c_str(),_port,&hints,&servinfo) != 0) {
             hints.ai_family = AF_INET6;
             if (getaddrinfo(vs[0].c_str(),_port,&hints,&servinfo) != 0) {
                 GLOG_SER("getaddrinfo error try 6 and 4 host:%s", saddr.c_str());
                 return -1;
            }
        }        
        for (struct addrinfo * p = servinfo; p != NULL; p = p->ai_next) {
            int sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol);
            if (sockfd == -1){
                continue;
            }
            close(sockfd);
            memcpy(&addr, p->ai_addr, p->ai_addrlen);
            freeaddrinfo(servinfo);
            return 0;
        }
        freeaddrinfo(servinfo);
        GLOG_SER("create socket:%s error", saddr.c_str());
        return -1;
    }
    int         socknetaddr(sockaddr_in & addr, const std::string & saddr) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_addr.s_addr = 0;
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        std::vector<string> vs;
        strsplit(saddr, ":", vs);
        if (strisint(vs[0])) {
            addr.sin_addr.s_addr = stol(vs[0]);
        }
        uint32_t a, b, c, d;
        if (sscanf(vs[0].c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            //net order = big endia order 
            addr.sin_addr.s_addr = a | (b << 8) | (c << 16) | (d << 24);
        }
        else { //must be a domainname
            int num = 1;
            if (ipfromhostname(&addr.sin_addr.s_addr, num, vs[0])) {
                GLOG_ERR("get host name error first name:%s hostname:%s", vs[0].c_str(), saddr.c_str());
                return -1;
            }
        }
        if (vs.size() == 2) {
            addr.sin_port = htons(stoi(vs[1]));
        }
        return 0;
    }
    int         ipfromhostname(OUT uint32_t * ip, INOUT int & ipnum, const std::string & hostname) {
        int nmaxip = ipnum;
        ipnum = 0;
        struct hostent hosts;
        struct hostent * result;
        int h_errnop;
        char buffer[512];
        int ret = gethostbyname_r(hostname.c_str(),
                                  &hosts, buffer, sizeof(buffer),
                                  &result, &h_errnop);
        if (ret) {
            GLOG_ERR("gethost by name error :%s ", hstrerror(h_errnop));
            return -1;
        }
        while (ipnum < nmaxip && result->h_addr_list[ipnum]) {
            *ip = *(uint32_t*)result->h_addr_list[ipnum];
            ++ipnum;
        }
        return 0;
    }
    uint32_t    host_getipv4(const char * nic) {
        struct ifreq _temp;
        struct sockaddr_in *soaddr;
        int fd = 0;
        int ret = -1;
        strcpy(_temp.ifr_name, nic);
        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            return 0;
        }
        ret = ioctl(fd, SIOCGIFADDR, &_temp);
        close(fd);
        if (ret < 0) {
            return 0;
        }
        soaddr = (struct sockaddr_in *)&(_temp.ifr_addr);
        return soaddr->sin_addr.s_addr;
    }
    string      stripfromu32v4(uint32_t ip) {
        char tmpbuff[64] = { 0 };
        if (nullptr == inet_ntop(AF_INET, &ip, tmpbuff, sizeof(tmpbuff))) {
            return "";
        }
        return string(tmpbuff);
    }
    uint32_t    u32fromstripv4(const string & ip) {
        uint32_t uip;
        if (1 == inet_pton(AF_INET, ip.c_str(), &uip)) {
            return uip;
        }
        return 0;
    }
    string      host_getmac(const char * nic) {
        struct ifreq _temp;
        char mac_addr[64] = { 0 };
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            return "";
        }
        memset(&_temp, 0, sizeof(struct ifreq));
        strncpy(_temp.ifr_name, nic, sizeof(_temp.ifr_name) - 1);
        if ((ioctl(sockfd, SIOCGIFHWADDR, &_temp)) < 0) {
            close(sockfd);
            return "";
        }
        close(sockfd);
        snprintf(mac_addr, sizeof(mac_addr)-1, "%02x%02x%02x%02x%02x%02x",
                 (unsigned char)_temp.ifr_hwaddr.sa_data[0],
                 (unsigned char)_temp.ifr_hwaddr.sa_data[1],
                 (unsigned char)_temp.ifr_hwaddr.sa_data[2],
                 (unsigned char)_temp.ifr_hwaddr.sa_data[3],
                 (unsigned char)_temp.ifr_hwaddr.sa_data[4],
                 (unsigned char)_temp.ifr_hwaddr.sa_data[5]);
        return mac_addr;
    }

    int         waitfd_writable(int fd, int timeout_ms) {
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int ret = select(fd + 1, NULL, &fdset, NULL, &tv);
        if (ret > 0) {
            assert(FD_ISSET(fd, &fdset));
            return 0;
        }
        return -1;
    }
    int                 lockfile(const std::string & file, bool nb) {
        int fd = open(file.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd == -1) {
            GLOG_ERR("open file:%s error ", file.c_str());
            return -1;
        }
        int flags = LOCK_EX;
        if (nb) {
            flags |= LOCK_NB;
        }
        if (flock(fd, flags)) {
            close(fd);
            return -1;
        }
        return fd;
    }
    int                 unlockfile(int fd) {
        int ret = flock(fd, LOCK_UN);
        close(fd);
        return ret;
    }
    int			lockpidfile(const std::string & file, int kill_other_sig, bool nb, int * pfd, bool notify) {
        int fd = open(file.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd == -1) {
            GLOG_ERR("open file:%s error ", file.c_str());
            return -1;
        }
        int flags = LOCK_EX;
        if (nb) {
            flags |= LOCK_NB;
        }
        char szpid[16] = { 0 };
        int pid = 0;
        while (flock(fd, flags) == -1) {
            if (pid == 0) { //just read once
                int n = readfile(file, szpid, sizeof(szpid));
                if (n > 0) {
                    pid = strtol(szpid, NULL, 10);
                    if (kill_other_sig == 0) {
                        GLOG_ERR("lock pidfile:%s fail , the file is held by pid %d", file.c_str(), pid);
                    }
                }
                else {
                    GLOG_ERR("lock pidfile:%s fail but read pid from file error !", file.c_str());
                }
            }
            if (pid > 0 && kill_other_sig > 0) {
                int ret = kill(pid, kill_other_sig);
                if (ret == 0) {
                    if (notify) {
                        GLOG_WAR("send the pidfile locker:%d by signal:%d", pid, kill_other_sig);
                        return pid;
                    }
                    else {
                        usleep(1000 * 100);//100ms
                    }
                }
                if (ret && errno == ESRCH) { //killed process
                    GLOG_WAR("killed the pidfile locker:%d by signal:%d", pid, kill_other_sig);
                    break;
                }
            }
            else { //pid > 0 && error
                close(fd);
                GLOG_SWR("kill process error !");
                return pid;
            }
        }
        pid = getpid();
        snprintf(szpid, sizeof(szpid), "%d", pid);
        writefile(file, szpid);
        if (pfd) { *pfd = fd; }
        return pid;
    }
    static inline size_t hash_dbj2(const char * str, size_t n) {
        size_t hash = 5381;
        int c;
        if (n > 0) {
            while ((c = *str++) && n--)
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }
        else {
            while ((c = *str++))
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }
        return hash;
    }
    static inline size_t hash_sdbm(const char * str, size_t n) {
        size_t hash = 0;
        int c;
        if (n > 0) {
            while ((c = *str++) && n--)
                hash = c + (hash << 6) + (hash << 16) - hash;
        }
        else {
            while ((c = *str++))
                hash = c + (hash << 6) + (hash << 16) - hash;
        }
        return hash;
    }

    size_t      strhash(const std::string & buff, str_hash_strategy st) {
        switch (st) {
        case hash_str_sdbm:
        return hash_sdbm(buff.data(), buff.length());
        default:
        return hash_dbj2(buff.data(), buff.length());
        }
    }
    int			strsplit(const std::string & str, const string & sep, std::vector<std::string> & vs,
        bool ignore_empty, int maxsplit, int beg_, int end_){
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
            if (pos != string::npos && pos < end){
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
        return d1 == d2 && time_same_year(t1, t2) && abs(t1 - t2) < (7 * 86400);
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
    const char*		strftime(std::string & str, struct tm & rtm, const char * format){
        str.reserve(32);
        strftime((char*)str.c_str(), str.capacity(), format, &rtm);
        return str.c_str();
    }
    time_t         strptime(const std::string & str, const char * format){
        struct tm _tmptm;
        time_t unixtime = 0;
        const char * p = ::strptime(str.c_str(), format, &_tmptm);
        if (!p){
            return time_t(-1);
        }
        unixtime = mktime(&_tmptm);
        return unixtime;
    }
    time_t			    stdstrtime(const char * strtime){
        return strptime(strtime, "%FT%X%z");
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
    const char*			strcharsetrandom(std::string & randoms, int length, const char * charset){
        if (!charset || !(*charset)){
            return nullptr;
        }
        randoms.reserve(length);
        int charsetlen = strlen(charset);
        std::random_device	rd;
        for (int i = 0; i < length; ++i){
            randoms.append(1, charset[rd() % charsetlen]);
        }
        return randoms.c_str();
    }
    const char*			strrandom(std::string & randoms, int length, char charbeg, char charend){
        if (charbeg > charend){ std::swap(charbeg, charend); }
        randoms.reserve(length);
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
#if (defined(_MSC_VER) && _MSC_VER > 1500) || defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 9)
        str = std::regex_replace(str, std::regex(repattern), repl);
#else        
        GLOG_ERR("not found re find in this compiler repattern:%s -> %s", repattern.c_str(), repl.c_str());
#endif
        return str;
    }
    bool                strrefind(string & str, const string & repattern, std::match_results<string::const_iterator>& m){
#if (defined(_MSC_VER) && _MSC_VER > 1500) || (defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 9))
        return std::regex_search(str, m, std::regex(repattern));
#else
        GLOG_ERR("not found re find in this compiler repattern:%s", repattern.c_str());
        return false;
#endif
    }
    std::string &       strtrim(std::string & str, const char * charset){
        if (str.empty() || !charset || !*charset){
            return str;
        }
        strltrim(str, charset);
        return strrtrim(str, charset);
    }
    std::string &       strltrim(std::string & str, const char * charset){
        if (str.empty() || !charset || !*charset){
            return str;
        }
        for (size_t i = 0; i < str.length(); ++i){
            if (!strchr(charset, str[i])){
                if (i > 0){
                    str.erase(0, i);
                }
                return str;
            }
        }
		str.clear();
        return str;
    }
    std::string &       strrtrim(std::string & str, const char * charset){
        if (str.empty() || !charset || !*charset){
            return str;
        }
        for (size_t i = str.length() - 1; (long int)i >= 0; --i){            
            if (!strchr(charset, str[i])){
                if (i+1 < str.length()){
                    str.erase(i+1, str.length() - 1 - i);
                }
                return str;
            }
        }
		str.clear();
        return str;
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
    const char         *strjoin(std::string & val, const std::string & sep, const char ** it){
        size_t i = 0;
        val.clear();
        for (const char * p = it[0]; *p; ++p){
            if (i != 0){ val.append(sep); }
            val.append(p);
            ++i;
        }
        return val.c_str();
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
    int             b64_encode(std::string & b64, const char * buff, int slen){
        b64.clear();
        b64.reserve(slen * 4 / 3 + 3);
		const unsigned char * data = (const unsigned char *)buff;
        for (int i = 0; i < slen; i += 3) {
            uint32_t n = data[i] << 16;
            if (i + 1 < slen){
                n |= data[i + 1] << 8;
            }
            if (i + 2 < slen){
                n |= data[i + 2];
            }
            const unsigned char n0 = (const unsigned char)(n >> 18) & 0x3f;
            const unsigned char n1 = (const unsigned char)(n >> 12) & 0x3f;
            const unsigned char n2 = (const unsigned char)(n >> 6) & 0x3f;
            const unsigned char n3 = (const unsigned char)(n)& 0x3f;
            b64.push_back(s_b64_lookup_d2c[n0]);
            b64.push_back(s_b64_lookup_d2c[n1]);
            if (i + 1 < slen)
            {
                b64.push_back(s_b64_lookup_d2c[n2]);
            }
            if (i + 2 < slen)
            {
                b64.push_back(s_b64_lookup_d2c[n3]);
            }
        }
        for (int k = 0; k < (int)(3 - slen % 3) % 3; k++)
        {
            b64.push_back('=');
        }
        return b64.length();
    }
    int             b64_decode(std::string & buff, const std::string & b64){
        int slen = b64.length();
        const unsigned char * s = (const unsigned char *)b64.data();
        if (slen == 0 || slen % 4){ // empty || !/4
            return -1;
        }
        buff.clear();
        buff.reserve(slen * 3 / 4 + 3);
        for (int i = 0; i < slen; i += 4) {
            unsigned char n0 = s_b64_lookup_c2d[s[i + 0]];
            unsigned char n1 = s_b64_lookup_c2d[s[i + 1]];
            unsigned char n2 = s_b64_lookup_c2d[s[i + 2]];
            unsigned char n3 = s_b64_lookup_c2d[s[i + 3]];
            if (0x80 & (n0 | n1 | n2 | n3)){
                return i;
            }
            uint32_t n = (n0 << 18) | (n1 << 12) | (n2 << 6) | n3;
            buff.push_back((n >> 16) & 0xff);
            if (s[i + 2] != '='){
                buff.push_back((n >> 8) & 0xff);
            }
            if (s[i + 3] != '='){
                buff.push_back((n)& 0xff);
            }
        }
        return 0;
    }
    int                 hex2bin(std::string & bin, const char * hex){        
        bin.clear();
        int nhex = strlen(hex);
        bin.reserve((nhex >> 1) + 1);///2
        if ((nhex & 1) == 1){
            return -1; //error length
        }
        while (hex && *hex){ //'A''B''C' => 16
            unsigned char n = (s_hex_lookup_c2d(*hex) << 4) + s_hex_lookup_c2d(*(hex + 1));
            bin.push_back(n);
            hex += 2;
        }
        return 0;
    }
    int                 bin2hex(std::string & hex, const char * buff, int ibuff){
        hex.clear();
        hex.reserve(ibuff*2+1);
        if (ibuff <= 0){
            return 0;
        }
        while (ibuff--){
            hex.push_back(s_hex_lookup_d2c((buff[ibuff]) >> 4));
            hex.push_back(s_hex_lookup_d2c((buff[ibuff]) & 0xF));
        }
        return 0;
    }
    /*
       void *dlopen(const char *filename, int flag);
       char *dlerror(void);
       void *dlsym(void *handle, const char *symbol);
       int dlclose(void *handle);
    */
    //////////////////////////////////////////////////////////////////////////////
    void    *           dllopen(const char * file, int flag) {
        if (flag == 0) {
            flag = RTLD_LAZY;
        }
        return dlopen(file, flag);
    }
    void    *           dllsym(void * dll, const char * sym) {
        return dlsym(dll, sym);
    }
    int                 dllclose(void * dll) {
        return dlclose(dll);
    }
    const char *        dllerror() {
        return dlerror();
    }

    //////////////////////////////////////////////////////////
    bits::bits(size_t n){
        if (n > 8){
            nvbits.reserve((n + (8 * sizeof(size_t)-1)) / (8 * sizeof(size_t)));
        }
        else {
            nvbits.reserve(8);
        }
    }
    void    bits::set(size_t pos, bool bv){
        size_t idx = pos / (sizeof(size_t)*8);
        size_t xoffset = pos % (sizeof(size_t)*8);
        for (size_t x = nvbits.size(); x <= idx; ++x){
            nvbits.push_back(0ULL);
        }
        if (bv){
            nvbits[idx] |= (1ULL << xoffset);
        }
        else {
            nvbits[idx] &= (~(1ULL << xoffset));
        }
    }
    bool    bits::at(size_t pos){
        size_t idx = pos / (sizeof(size_t)* 8);
        size_t xoffset = pos % (sizeof(size_t)* 8);
        if (idx >= nvbits.size()){
            return false;
        }
        return (nvbits[idx] & ((1ULL) << xoffset)) ? true : false;
    }









}



