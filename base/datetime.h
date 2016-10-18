#pragma once
#include <time.h>
#include <string>
struct DateTimeImpl;

struct DateTime {
	int					set_gmt_offset(int gmtoff , int dst = -1);
	int					add_time_offset(int seconds);
	int64_t				update();

public:
	uint32_t			now() const;
	struct timeval &	now_tv() const;
	int64_t				diff(const struct timeval & tv1, const struct timeval & tv2) const;
	void				add(struct timeval & tv1, int64_t diff) const;
	struct tm  *		localtime(uint32_t tmstp, struct tm * tmbuff = nullptr) const;
	uint32_t			mktime(const struct tm & ) const;
	uint32_t			parse(const char * datetime, const char * fmt = "%Y-%m-%d %H:%M:%S") const;
	std::string			format(uint32_t tmstp, const char * fmt = "%Y-%m-%d %H:%M:%S") const;
	const char *		format(std::string & str, uint32_t tmstp, const char * fmt = "%Y-%m-%d %H:%M:%S") const;
	const char *		timezone() const;
	int					gmt_offset() const;
	int					time_offset() const;

public:
	DateTime();
	~DateTime();
	DateTimeImpl *	impl {nullptr};
};