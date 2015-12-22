#pragma once
#include "stdinc.h"

struct dcsmq_t;

//initiative							passive
//											s
//	c1/2/3/4/.. = msgq.type
//	c1.send(c1, msg)		=>		s.recive(any, msg) , any = c1
//	c1.receive(c1, msg)		<=		s.send(c1, msg)


//	c1				----------->					
//					<-----------

//	c2				----------->			
//					<-----------

//	c3				----------->			
//					<-----------

//	c4				----------->			
//					<-----------


struct dcsmq_config_t {
    string			keypath;
	bool			passive;//if true, receive all type msg, else receive 
	int				msg_buffsz;
	int				max_queue_buff_size;
	bool			attach;
	dcsmq_config_t(){
		msg_buffsz = 1024 * 1024;
		max_queue_buff_size = 10 * 1024 * 1024; //10MB
		passive = false;
		attach = false;
	}
};

struct dcsmq_msg_t {
    const char * buffer;
    int			 sz;
	dcsmq_msg_t(const char * buf, int s) :buffer(buf), sz(s){}
};

struct dcsmq_stat_t {
	//global data
	int					client_key;
	int					server_key;
	size_t				total_size;
	struct msqid_ds		ds;
	//-------------------------------
	int					nchannel;
	struct dcsmq_channel_t {
		uint64_t	session;
		size_t		req_num;
		size_t		rsp_num;
	}	*channels;
	dcsmq_stat_t();
	~dcsmq_stat_t();
};

typedef int (*dcsmq_msg_cb_t)(dcsmq_t * , uint64_t src, const dcsmq_msg_t & msg, void * ud);
dcsmq_t *	dcsmq_create(const dcsmq_config_t & conf);
void		dcsmq_destroy(dcsmq_t*);
void		dcsmq_msg_cb(dcsmq_t *, dcsmq_msg_cb_t cb, void * ud);
int			dcsmq_poll(dcsmq_t*, int max_time_us );
int			dcsmq_send(dcsmq_t*,uint64_t dst, const dcsmq_msg_t & msg);
int			dcsmq_push(dcsmq_t*, uint64_t dst, const dcsmq_msg_t & msg);//send to peer like himself
bool		dcsmq_server_mode(dcsmq_t *);
void		dcsmq_set_session(dcsmq_t *, uint64_t session); //send or recv type
uint64_t	dcsmq_session(dcsmq_t *);
//status report for debug
void		dcsmq_stat(const std::string & keypath, dcsmq_stat & stat);

