
#include <string.h>
#include <sys/time.h>
#include "datetime.h"

#define TIME_RATIO_SECOND_MICRO_SECONDS	(1000000)
#define TIME_RATIO_DAY_SECONDS	(86400)
#define TIME_RATIO_HOUR_SECONDS	(3600)

struct DateTimeImpl {
	struct timeval  update_tv;
	struct timeval  tv;
	struct timezone	local_tz; //minuests of west
	int	set_gmtoff {0}; //minutes of east
	int set_dst {0};
	int local_gmtoff_adjust {0};
	char timezone[8];
	int timeoffset {0};
};
static inline void timezone_str_format(char * str, size_t sz, int gmtoff) {
	int n = gmtoff / 3600;
	n += (gmtoff/60) % 60;
	snprintf(str, sz-1, "%+05d", n);
}
DateTime::DateTime() {
	impl = new DateTimeImpl();
	gettimeofday(&impl->update_tv, &impl->local_tz);
	impl->tv = impl->update_tv;
	impl->set_gmtoff = -60 * impl->local_tz.tz_minuteswest;
	timezone_str_format(impl->timezone, sizeof(impl->timezone), impl->set_gmtoff);
}
DateTime::~DateTime() {
	if (impl) {
		delete impl;
		impl = nullptr;
	}
}

/*
@param gmtoff: east of utc timestamp uint32 .
*/
int					DateTime::set_gmt_offset(int gmtoff, int dst){
	if (gmtoff < -12 * TIME_RATIO_HOUR_SECONDS || gmtoff > 12 * TIME_RATIO_HOUR_SECONDS || dst < -1 || dst > 1) {
		return -1;
	}
	impl->local_gmtoff_adjust = gmtoff + 60*impl->local_tz.tz_minuteswest;//- -west = +
	impl->set_gmtoff = gmtoff;
	impl->set_dst = dst;
	timezone_str_format(impl->timezone, sizeof(impl->timezone), impl->set_gmtoff);
	return 0;
}
int					DateTime::add_time_offset(int seconds) {
	impl->timeoffset += seconds;
	return 0;
}

int64_t 			DateTime::update() {
	gettimeofday(&impl->tv, NULL);
	int64_t diffv = diff(impl->tv, impl->update_tv);
	impl->update_tv = impl->tv;
	return diffv;
}

uint32_t			DateTime::now() const {
	return impl->tv.tv_sec + impl->timeoffset;
}
struct timeval &	DateTime::now_tv() const {
	gettimeofday(&impl->tv, NULL);
	impl->tv.tv_sec += impl->timeoffset;
	return impl->tv;
}
int64_t				DateTime::diff(const struct timeval & tv1, const struct timeval & tv2) const {
	return (tv1.tv_sec - tv2.tv_sec)*TIME_RATIO_SECOND_MICRO_SECONDS+ (tv1.tv_usec - tv2.tv_usec);
}
void				DateTime::add(struct timeval & tv, int64_t diff) const {
	tv.tv_sec += diff / TIME_RATIO_SECOND_MICRO_SECONDS;
	tv.tv_usec += diff % TIME_RATIO_SECOND_MICRO_SECONDS;
	if (tv.tv_usec >= TIME_RATIO_SECOND_MICRO_SECONDS) {
		tv.tv_sec += tv.tv_usec / TIME_RATIO_SECOND_MICRO_SECONDS;
		tv.tv_usec %= TIME_RATIO_SECOND_MICRO_SECONDS;
	}
}

struct tm *			DateTime::localtime(uint32_t tmstmp, struct tm * tmbuff) const {
	time_t ttm = tmstmp + impl->set_gmtoff + impl->timeoffset;
	if (tmbuff) {
		return gmtime_r(&ttm, tmbuff);
	}
	else {
		return gmtime(&ttm);
	}
}
uint32_t			DateTime::mktime(const struct tm & vtm) const {
	struct tm ttm = vtm;
	ttm.tm_isdst = impl->local_tz.tz_dsttime;
#if	_BSD_SOURCE || _SVID_SOURCE
	time_t tmstmp = timegm(&ttm);
	return tmstmp - impl->set_gmtoff - impl->timeoffset;
#else
	time_t tmstmp = mktime(&ttm); //<=> timelocal
	return tmstmp - impl->local_gmtoff_adjust - impl->timeoffset;
#endif
}
uint32_t			DateTime::parse(const char * datetime, const char * fmt /*= "%Y-%m-%d %H:%M:%S"*/) const {
	//char *strptime(const char *s, const char *format, struct tm *tm);
	struct tm ttm;
	char * pps = strptime(datetime, fmt, &ttm);
	if (pps == datetime) {
		return -1;
	}
	return mktime(ttm);
}
const char *		DateTime::format(std::string & str, uint32_t tmstp, const char * fmt /* = "%Y-%m-%d %H:%M:%S"*/) const {
	str.reserve(64);
	struct tm ttm;
	strftime((char*)str.data(), str.capacity(), fmt, localtime(tmstp, &ttm));
	return str.data();
}
const char *		DateTime::timezone() const {
	return impl->timezone;
}
int					DateTime::gmt_offset() const {
	return impl->set_gmtoff;
}
int					DateTime::time_offset() const {
	return impl->timeoffset;
}