#include "utility.hpp"
#include "logger.h"

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


	int			readfile(const char * file, char * buffer, size_t sz){
		FILE * fp = fopen(file, "r");
		if (!fp){
			LOGP("open file£º%s error:%d", file, errno);
			return -1;
		}		
		int n;
		size_t tsz = 0;
		while ((n = fread(buffer + tsz, 1, sz - tsz, fp))){
			if (n > 0){
				tsz += n;
			}
			else if(errno != EINTR &&
				errno != EAGAIN) {
				LOGP("read file:%s ret:%d error :%d total sz:%zu", file, n, errno, tsz);
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
	int			writefile(const char * file, const char * buffer, size_t sz){
		FILE * fp = fopen(file, "w");
		if (!fp){
			LOGP("open file£º%s error:%d", file, errno);
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
				errno != EAGAIN) {
				LOGP("write file:%s ret:%d error :%d writed sz:%zu total:%zu", file, n, errno, tsz, sz);
				break;
			}
		}
		fclose(fp);
		if (tsz == sz){
			return tsz;
		}
		else {
			LOGP("write file:%s writed:%zu error :%d total sz:%zu", file, tsz, errno, sz);
			return -2;
		}
	}
}



