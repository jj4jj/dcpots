#pragma  once

//create / destroy / poll
int     reporter_init(const const char * parent, const char * name);
void    reporter_destroy();
int     reporter_ready();//1:ok,0:connecting;-1:error
void    reporter_update(int timeout_ms);


//main interface
int     report_set(const char * k, const char * val);
int	    report_inc(const char * k, int inc = 1, const char * param = nullptr);
int     report_dec(const char * k, int dec = 1, const char * param = nullptr);

