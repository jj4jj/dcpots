#include "dagent/dagent.h"
#include "dcnode/libmq.h"
#include "dcnode/libtcp.h"


static int max_ping_pong = 10000;
static int max_ppsz = 0;
static int max_ping_time = 100000;
int mq_cb(smq_t * mq, uint64_t src, const smq_msg_t & msg, void * ud)
{
	LOGP("mq cb msg size:%d src:%lu", msg.sz, src);
	max_ppsz += msg.sz;
	if (max_ping_time <= 0)
	{
		LOGP("mq cb msg size:%d src:%lu total:%d MB", msg.sz, src, max_ppsz/1048576);
		exit(0);
	}
	max_ping_time--;
	smq_send(mq, src, msg);
	return 0;
}
#define CHECK(r)	\
if (r) {\
\
	LOGP("check error :%d sys error:%s",r, strerror(errno)); \
	return -1;\
}


int test_mq(const char * ap)
{
	char * test_msg = (char*)malloc(1024*10);

	smq_config_t	sc;
	sc.key = "./dcagent";
	sc.is_server = ap ? true : false;
	auto p = smq_create(sc);
	if (!p)
	{
		LOGP("error create smq errno:%s", strerror(errno));
		return -1;
	}
	smq_msg_cb(p, mq_cb, nullptr);
	if (!sc.is_server)
	{
		smq_send(p, getpid(), smq_msg_t(test_msg, 1024*10));
	}
	while (true)
	{
		smq_poll(p, 500);
	}
	return 0;
}
const char * stmsg = "hello,world!";
int _stcp_cb(stcp_t* stc, const stcp_event_t & ev, void * ud)
{
	LOGP("stcp event type:%d fd:%d reason:%d last error msg:%s", ev.type, ev.fd, ev.reason, strerror(ev.error));
	if (ev.type == stcp_event_type::STCP_CONNECTED){
		stcp_send(stc, ev.fd, stcp_msg_t(stmsg, strlen(stmsg)+1));
	}
	if (ev.type == stcp_event_type::STCP_READ){
		LOGP("ping pang get msg from fd:%d msg:%s",ev.fd, ev.msg->buff);
		stcp_send(stc, ev.fd, *ev.msg);
	}
	return -1;
}

int test_tcp(const char * ap)
{
	stcp_config_t sc;
	sc.is_server = ap ? true : false;
	sc.listen_addr.ip = "127.0.0.1";
	sc.listen_addr.port = 8888;
	auto * p = stcp_create(sc);
	if (!p)
	{
		LOGP("create stcp error ! syserror:%s", strerror(errno));
		return -1;
	}
	stcp_event_cb(p, _stcp_cb, nullptr);
	if (!sc.is_server)
	{
		int ret = stcp_connect(p, sc.listen_addr, 5);
		CHECK(ret)		
	}
	while (true)
	{
		stcp_poll(p, 1000);
		usleep(1000);
	}	
	return 0;
}

int dc_cb(void * ud, const dcnode_msg_t & msg)
{
	LOGP("dc msg recv :%s", msg.debug());
	return 0;
}

//l3:test callbaker
int test_node(const char * p)
{
	dcnode_config_t dcf;
	dcf.addr.msgq_key = "./gmon.out";
	dcf.max_channel_buff_size = 1024 * 1024;
	dcf.name = "leaf";
	dcf.heart_beat_gap = 10;
	dcf.max_live_heart_beat_gap = 20;
	int ltest = 0;
	if (p)
	{
		if (strcmp(p, "l1") == 0){
			//client leaf node
			dcf.addr.msgq_key = "./gmon.out";
			dcf.addr.parent_addr = "127.0.0.1:8880";
			dcf.name = "layer1";
		}
		else
		if (strcmp(p, "l2") == 0){
			dcf.addr.msgq_key = "";
			dcf.name = "layer2";
			dcf.addr.listen_addr = "127.0.0.1:8880";
		}
		else 
		if (strcmp(p, "l3") == 0){
			dcf.addr.msgq_key = "";
			dcf.name = "test";
			dcf.addr.listen_addr = "";
			dcf.heart_beat_gap = 0;
			dcf.max_live_heart_beat_gap = 0;
			ltest = 3;
		}
	}
	auto dc = dcnode_create(dcf);
	CHECK(!dc)
	dcnode_set_dispatcher(dc, dc_cb, nullptr);
	int times = 0;
	uint64_t t1, t2;
	if (ltest == 3){
		LOGP("add timer test in node....");
		t1 = dcnode_timer_add(dc, 1000, [&times, &t1, &t2, dc](){
			puts("test timer 1s");
			times++;
			if (times > 5 && t2 > 0){
				puts("cancel t1");
				dcnode_timer_cancel(dc, t2);
				t2 = 0;
			}
			if (times > 10 && t1 > 0){
				puts("cancel t1");
				dcnode_timer_cancel(dc, t1);
				t1 = 0;
			}

		}, true);
		t2 = dcnode_timer_add(dc, 1000 * 3, [](){
			puts("test timer 3s");
		}, true);
	}
	while (true)
	{
		dcnode_update(dc, 10000);
		usleep(10000);
	}
	return 0;
}

int main(int argc, char* argv[])
{
	if (argc >= 2)
	{
		switch (argv[1][0])
		{
		case 'm':
			return test_mq(argv[2]);
		case 't':
			return test_tcp(argv[2]);
		case 'n':
			return test_node(argv[2]);
		default:
			break;
		}
	}


	dagent_config_t	conf;

	//an agent
	auto & ncf = conf.node_conf;
	conf.max_msg_size = 1024 * 1024;
	ncf.addr.listen_addr = "127.0.0.1:8888";
	ncf.heart_beat_gap = 60;
	ncf.max_channel_buff_size = 1024576;
	ncf.max_register_children = 10;
	ncf.name = "test1";
	ncf.addr.msgq_key = "./dcagent";

	if (argc == 2)
	{
		//client
		ncf.addr.listen_addr = "";
		ncf.name = "test2";
	}

	int ret = dagent_init(conf);
	if (ret)
	{
		puts("error init!");
		puts(DAGENT_ERRMSG(" < "));
		perror("system error ");
		return -1;
	}
	while (true)
	{
		dagent_update();
		usleep(10000);//10ms
	}
	dagent_destroy();
	return 0;
}
