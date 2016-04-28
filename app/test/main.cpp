#include "base/dcsmq.h"
#include "base/dctcp.h"
#include "base/logger.h"
#include "dcnode/dcnode.h"
#include "dagent/dagent.h"
#include "base/dcutils.hpp"
#include "base/msg_proto.hpp"
#include "utility/util_mysql.h"

static int max_ping_pong = 100000;
static int max_ppsz = 0;
static int pingpong = 0;
static const char * msgqpath = nullptr;
static uint64_t start_us = 0;
int mq_cb(dcsmq_t * mq, uint64_t src, const dcsmq_msg_t & msg, void * ud)
{
	//LOGP("mq cb msg size:%d src:%lu", msg.sz, src);
	max_ppsz += msg.sz;
	pingpong++;
	if (pingpong == 1){
		start_us = dcsutil::time_unixtime_us();
	}
	dcsmq_send(mq, src, msg);
	if (pingpong > 5){
		logger_set_level(NULL, LOG_LVL_INFO);
	}
	if (pingpong >= max_ping_pong)
	{
		start_us = dcsutil::time_unixtime_us() - start_us;
		GLOG_TRA("mq cb msg size:%d src:%lu total:%d MB time:%ldus pingpong:%d",
			msg.sz, src, max_ppsz / 1048576, start_us, max_ping_pong);
		exit(0);
	}
	return 0;
}
#define CHECK(r)	\
if (r) {\
\
	GLOG_TRA("check error :%d sys error:%s",r, strerror(errno)); \
	return -1;\
}


int test_mq(const char * ap)
{
	const int test_msg_len = 1024;
	char * test_msg = (char*)malloc(test_msg_len);

	dcsmq_config_t	sc;
	sc.keypath = msgqpath;
	sc.passive = ap ? true : false;
	auto p = dcsmq_create(sc);
	if (!p)
	{
		GLOG_TRA("error create smq errno:%s", strerror(errno));
		return -1;
	}
	dcsmq_msg_cb(p, mq_cb, nullptr);
	if (!sc.passive)
	{
		start_us = dcsutil::time_unixtime_us();
		pingpong = 1;
		dcsmq_send(p, getpid(), dcsmq_msg_t(test_msg, test_msg_len));
	}
	while (true)
	{
		dcsmq_poll(p, 100);//max proc time is 80-> 12.5 000 * 5 = 1.25W
	}
	return 0;
}
const char * stmsg = "hello,world!";
static int tcp_server_mode = 1;
int _dctcp_cb(dctcp_t* stc, const dctcp_event_t & ev, void * ud)
{
	//LOGP("stcp event type:%d fd:%d reason:%d last error msg:%s", ev.type, ev.fd, ev.reason, strerror(ev.error));
	if (ev.type == dctcp_event_type::DCTCP_CONNECTED){
		tcp_server_mode = 0;
		for (int i = 0; i < 60; i++){
			dctcp_send(stc, ev.fd, dctcp_msg_t(stmsg, strlen(stmsg) + 1));
		}
		start_us = dcsutil::time_unixtime_us();
	}

	if (ev.type == dctcp_event_type::DCTCP_READ){
		GLOG_TRA("ping pang get msg from fd:%d msg:%s",ev.fd, ev.msg->buff);
		
		pingpong++;
		if (pingpong == 1 && tcp_server_mode){
			start_us = dcsutil::time_unixtime_us();
		}
		//LOGP("mq cb msg size:%d src:%lu", msg.sz, src);		
		max_ppsz += ev.msg->buff_sz;
		if (pingpong > 5){
			logger_set_level(NULL, LOG_LVL_INFO);
		}
		if (pingpong >= max_ping_pong)
		{
			logger_set_level(NULL, LOG_LVL_PROF);
			if (!tcp_server_mode){
				dctcp_send(stc, ev.fd, *ev.msg);
			}
			start_us = dcsutil::time_unixtime_us() - start_us;
			GLOG_TRA("mq cb msg size:%d total:%d MB time:%ldus pingpong:%d",
				ev.msg->buff_sz,  max_ppsz / 1048576, start_us, max_ping_pong);
			exit(0);
		}
		dctcp_send(stc, ev.fd, *ev.msg);
	}
	return 0;
}
int test_tcp(const char * ap)
{
	dctcp_config_t sc;
	string listen_addr = "127.0.0.1:8888";
	auto * p = dctcp_create(sc);
	if (!p){
		GLOG_TRA("create stcp error ! syserror:%s", strerror(errno));
		return -1;
	}
	if (ap){
		dctcp_listen(p, listen_addr);
	}
	dctcp_event_cb(p, _dctcp_cb, nullptr);
	if (!ap){
		int ret = dctcp_connect(p, listen_addr, 5);
		CHECK(ret)		
	}
	logger_set_level(NULL, LOG_LVL_PROF);
	while (true)
	{
		dctcp_poll(p, 1000);
		usleep(10);
	}	
	return 0;
}

int dc_cb(void * ud, const char* src, const msg_buffer_t & msg)
{
	GLOG_TRA("dc msg recv :%s", msg.buffer);
	return 0;
}

//l3:test callbaker
int test_node(const char * p)
{
	dcnode_config_t dcf;
	dcf.addr = "pull:msgq://./gmon.out";
	dcf.max_channel_buff_size = 1024 * 1024;
	dcf.name = "leaf";
	dcf.parent_heart_beat_gap = 10;
	dcf.max_children_heart_beat_expired = 20;
	int ltest = 0;
	logger_config_t loger_conf;
	global_logger_init(loger_conf);	
	auto dc = dcnode_create(dcf);
	CHECK(!dc)
	dcnode_set_dispatcher(dc, dc_cb, dc);
	int times = 0;
	time_t last_time = time(NULL);
	while (true)
	{
		dcnode_update(dc, 10000);
		if (dcnode_ready(dc) == -1){
			GLOG_TRA("dcnode stoped ....");
			return -1;
		}
		if (ltest == 4 && last_time + 5 < time(NULL)){
			CHECK(dcnode_send(dc, "leaf", stmsg, strlen(stmsg)))
		}
		usleep(10000);
	}
	return 0;
}
#include "utility/script_vm.h"
static int python_test(){
	script_vm_config_t smc;
	smc.type = SCRIPT_VM_PYTHON;
	script_vm_t * vm = script_vm_create(smc);
	if (!vm){
		GLOG_TRA("error create vm !");
		return -1;
	}
	int r = script_vm_run_string(vm, "a=20");
	r |= script_vm_run_string(vm, "print('hello,world!')");
	r |= script_vm_run_string(vm, "print('hello,world!'+str(a))");
	GLOG_TRA("vm run ret:%d",r);
	script_vm_destroy(vm);

	GLOG_TRA("destroyed vm");
	smc.path = "./plugins";
	vm = script_vm_create(smc);
	if (!vm){
		GLOG_TRA("error create vm agin !");
		return -1;
	}
	r = script_vm_run_string(vm, "a=20");
	r |= script_vm_run_string(vm, "print('hello,world! %s' % a)");
	GLOG_TRA("vm run ret:%d", r);
	r = script_vm_run_file(vm, "hello.py");
	GLOG_TRA("vm run ret:%d", r);

	return 0;
}
int log_test(){
	logger_config_t lc;
	lc.max_file_size = 1024;
	lc.max_roll = 3;
	lc.dir = "./";
	lc.pattern = "test.log";
	int ret = global_logger_init(lc);
	if (ret){
		return ret;
	}
	int n = 3 * 1000;
	while (n--){
		GLOG_IFO("test logger msg just for size , this is dummy");
	}
	return 0;
}
#include "dcnode/proto/dcnode.pb.h"
int perf_test(const char * arg){
	
	typedef msgproto_t<dcnode::MsgDCNode>	msg_t;
	start_us = dcsutil::time_unixtime_us();
	int pack_unpack_times = 1000000;
	msg_buffer_t msgbuf;
	const char * s_test_msg = "hello,worlddfffxxxxxfggggggggggggggdfxsfsddddddddddddddddddfxxxxxxxxxxxxxxxffffffffffffffffffffffffff";
	msgbuf.create(10240);
	uint64_t packsize = 0;
	for (int i = 0; i < pack_unpack_times; ++i){
		msg_t	msg;
		msg.set_dst("hello");
		msg.set_src("heffff");
		msg.set_type(dcnode::MSG_DATA);
		msg.set_msg_data(s_test_msg, strlen(s_test_msg)+1);
		msg.mutable_ext()->set_unixtime(dcsutil::time_unixtime_ms() / 1000);
		if (!msg.Pack(msgbuf))
		{
			printf("error pack!\n");
		}
		if (!msg.Unpack(msgbuf)){
			printf("error unpack!\n");
		}
		//
		packsize += msg.PackSize();
	}
	int64_t cost_time = dcsutil::time_unixtime_us() - start_us;
	double speed = pack_unpack_times*1000000.0 / cost_time ;
	GLOG_TRA("print pack size:%lu msg size:%zd cost time:%ld total times:%d speed:%lf /s", 
		packsize, strlen(s_test_msg)+1,
		cost_time,
		pack_unpack_times, speed);
	return 0;
}
#include "utility/json_doc.hpp"
#include "iostream"
using namespace  std;
static int json_test(const char * file){
	json_doc_t jd;
	jd.parse_file(file);
	cout<<"jd-hh:"<<jd.HasMember("hh")<<endl;
	cout << "jd-hh null:" << jd["hh"].IsNull() << endl;
	cout << "jd-dcagent:" << jd.HasMember("dcagent") << endl;
	cout << "jd-dcagent null:" << jd[("dcagent")].IsNull() << endl;
	string s;
	puts(jd.pretty(s));

	json_doc_t jd2;
	jd2.AddMember("add3","hello",jd2.GetAllocator());
	jd2.AddMember("add4-bool", false, jd2.GetAllocator());
	cout << "jd2-add1 type:" << jd2["add1"].GetType() << endl;
	cout << "jd2-add2 type:" << jd2["add2"].GetType() << endl;

	rapidjson::SetValueByPointer(jd2, "/foo/0", 456);
	rapidjson::SetValueByPointer(jd2, "/foo/1", true);
	rapidjson::SetValueByPointer(jd2, "/foo/2", "44456");
	rapidjson::SetValueByPointer(jd2, "/foo/3", 125344566786103L);
	jd2.set("/foo/4", true);
	json_obj_t * v1 = rapidjson::GetValueByPointer(jd2,"/foo/0");
	json_obj_t * v2 = rapidjson::GetValueByPointer(jd2, "/foo/1");
	json_obj_t * v3 = rapidjson::GetValueByPointer(jd2, "/foo/2");
	json_obj_t * v4 = rapidjson::GetValueByPointer(jd2, "/foo/3");
	json_obj_t * v5 = jd2.get("/foo/4");
	json_obj_t * v6 = jd2.get("/foo/5", 12.6);//if empty create

	cout << v1->GetType() << endl <<
		v2->GetType() << endl <<
		v3->GetType() << endl <<
		v4->GetType() << endl <<
		v5->GetType() << endl <<
		v6->GetType() << endl;

	jd2.dump_file("test.out.json");
	return 0;
}
#include "utility/xml_doc.h"
static int xml_test(const char * xmlfile){
	xml_doc_t xml;
	if (xml.parse_file(xmlfile)){
		return -1;
	}
	string s;
	cout << (xml.dumps(s)) <<endl;

	auto n1 = xml.get_node("node1", nullptr,0,"node1text");
	auto n2 = xml.get_node("node2", nullptr, 0, "node2text");
	auto n3 = xml.get_node("node3", nullptr, 0, "node3ext");
	xml.get_attr("attr1", n2, "a1");
	xml.path_set("node3:hello", "world!");
	auto n11 = xml.get_node("node11", n1, 0, "node11text");
	auto n12 = xml.get_node("node12", n1, 0, "node12text");

	xml.path_set("node1.node11.a#0:a", "a0");
	xml.path_set("node1.node11.a#5:a", "a1");
	xml.path_set("node1.node11.a#1:a", "a2");
	xml.path_set("node1.node11.a#2:a", "a3");
	int na = xml.path_node_array_length("node1.node11.a");
	cout << "na:" <<na<<endl;
	xml.add_comment("hello,wolrld, this's a comments for n1!!!!", n11);
	xml.add_cdata("dddddddddddddd, this's a data node for n1!!!!", n1);

	cout << (xml.dumps(s));
	return 0;
}
static int lock_test(const char * pidfile){

	int pid = dcsutil::lockpidfile(pidfile, SIGTERM);
	if (pid <= 0){
		GLOG_TRA("error:%d lock errno:%d for:%s",pid, errno, strerror(errno));
		return -2;
	}
	if (pid != getpid()){
		return -3;
	}
	else {
		GLOG_TRA("lock file:%s success!", pidfile);
		while (true){
			sleep(5);
		}
	}
	return 0;
}
static int daemon_test(const char * arg){
	//dcsutil::daemonlize(0);
	std::vector<std::string> vs;
	std::vector<std::string> vss;
	int n = dcsutil::strsplit("..b.abc..def..", ".", vs);
	int m = dcsutil::strsplit("ffffdccvf", ".", vss);
	GLOG_TRA("split test ret:%d [0]:%s [1]:%s [2]:%s", n, vs[0].c_str(), vs[1].c_str(), vs[2].c_str());
	GLOG_TRA("split test ret:%d [0]:%s", m, vss[0].c_str());
	std::string str;
	GLOG_TRA("strtime:%s", dcsutil::strftime(str));
	GLOG_TRA("from_strtime:%lu", dcsutil::stdstrtime());
    if(arg){
        GLOG_TRA("format time:%s [%s]", arg, dcsutil::strftime(str, time(NULL), arg));
    }
	while (true){
		//LOGP("test ....");
		sleep(5);
	}
	return 0;
}
static int mysql_test(const char * p){
	using namespace dcsutil;
	mysqlclient_t	mc;
	mysqlclient_t::cnnx_conf_t	conf;
	conf.ip = "127.0.0.1";
	conf.uname = "test";
	conf.passwd = "123456";
	conf.port = 3306;
	if (mc.init(conf)){
		return -1;
	}
	int ret = mc.execute(
		"use test;");
	if (ret < 0){
		std::cerr << ret << " error:"<< mc.err_msg() << endl;
		return -1;
	}
	ret = mc.execute(" create table if not exists `mc_hello` (`h1` int not null);");
	if (ret < 0){
		std::cerr << ret << " error:" << mc.err_msg() << endl;
		return -1;
	}
	ret = mc.execute("insert into mc_hello values(24);");
	if (ret < 0){
		std::cerr << ret << " error:" << mc.err_msg() << endl;
		return -1;
	}
	std::cout << "affects:" << ret << endl;
	ret = mc.execute("select h1 from `mc_hello`;");
	if (ret < 0){
		std::cerr << ret << " error:" << mc.err_msg() << endl;
		return -1;
	}
	struct _test {
		static void 	cb(void* ud, INOUT bool & need_more, const dcsutil::mysqlclient_t::table_row_t & row){
			GLOG_TRA("cb ud:%p row:%s (%zu) name:%s total:%zu offset:%zu! more:%d",
				ud,row.row_data[0],row.row_length[0],row.fields_name[0],row.row_total, row.row_offset, need_more);
			if (row.row_offset > 5){
				need_more = false;
			}
		}
	};

	ret = mc.result(NULL, _test::cb);
	if (ret < 0){
		std::cerr << ret << " error:" << mc.err_msg() << endl;
		return -1;
	}
	return 0;
}
#include "utility/util_mongo.h"
static int mongo_test(const char * p){
	dcsutil::mongo_client_config_t conf;
	dcsutil::mongo_client_t		mg;
	conf.mongo_uri = "mongodb://127.0.0.1:27017";
	conf.multi_thread = 1;
	int ret = mg.init(conf);
	if (ret){
		GLOG_TRA("init error :%d!", ret);
		return -1;
	}
	using namespace dcsutil;
	string cmd = "{\"ping\": 1}";
	cmd = "{\"insert\": \"test\",\"documents\" : [{\"execute\":1}]}";
	struct _test_cb {
		static void cb(void * ud, const mongo_client_t::result_t & rst, const mongo_client_t::command_t & cmd){
			int * pn = (int*)ud;
			GLOG_TRA("rst response:%s  error:%s errno:%d param:%d",
				rst.rst.c_str(), rst.err_msg.c_str(),rst.err_no, *pn);
		}
	};
	int n = 0;
	while (true){
		if (mg.poll() == 0){
			//usleep(1000000);
		}
		if (!mg.running()){
			GLOG_TRA("mongo test stoped ...");
			break;
		}
		switch (n){
		case 0:
			ret = mg.command("test", "test", _test_cb::cb, &n, 0, cmd.c_str());
			break;
		case 1:
			ret = mg.insert("test", "test", "{\"mongo_insert\":1}", _test_cb::cb, &n);
			break;
		case 2:
			ret = mg.remove("test", "test", "{\"execute\":1}", _test_cb::cb, &n);
			break;
		case 3:
			ret = mg.find("test", "test", "{\"mongo_insert\":1}", _test_cb::cb, &n);
			break;
		case 4:
			ret = mg.count("test", "test", "{}", _test_cb::cb, &n);
			break;
		case 5:
			ret = mg.update("test", "test", "{\"q\": {\"mongo_insert\":1},\"u\":{\"mongo_insert\":100}}", _test_cb::cb, &n);
			break;
		case 6:
			ret = mg.count("test", "test", "{}", _test_cb::cb, &n);
			break;
		case 7:
			mg.stop();
			break;
		}
		++n;
		if (ret){
			GLOG_TRA("excute error ret:%d", ret);
		}
		sleep(1);
	}
	return 0;
}

#include "dcnode/proto/dcnode.pb.h"
#include "utility/util_proto.h"
static int pbxml_test(const char* arg){
	dcnode::MsgDCNode mdn;
	string error;
	mdn.set_dst("fffffffffffff");
	mdn.set_src("src-----");
	mdn.set_type(dcnode::MSG_HEART_BEAT);
	mdn.mutable_ext()->set_unixtime(time(NULL));
	dcsutil::protobuf_msg_to_xml_file(mdn, "dcnode.xml");
	return 0;
}
#include "base/dcdebug.h"
struct test_st1 {
    static void f(int n){
        string str;
        if (n <= 0){
            std::cout << dcsutil::stacktrace(str) << std::endl;
        }
        else {
            f(n - 1);
        }
    }
    void  g(char c){
        f(c);
    }
};
struct test_st2 {
    static void f(int n){
        test_st1::f(n);
    }
    void  g(char c){
        f(c);
    }
};

static int stacktrace_test(const char * arg){
    test_st2 ts;
    if (arg){
        ts.g(18);
    }
    else {
        ts.g(6);
    }
    return 0;
}
using namespace dcsutil;
static int http_test(const char * arg){
    if (!arg){
        arg = "baidu.com";
    }
    string uri = "tcp://";
    uri += arg;
    uri += ":80";
    cout << "open:" << uri << endl;
    int fd = dcsutil::openfd(uri);
    cout << "connect uri:" << uri << " fd:" << fd << endl;
    if (fd < 0){
        return -2;
    }
    string cmd = "GET / HTTP/1.1\r\n\r\n";
    int n = dcsutil::writefd(fd, cmd.c_str(), cmd.length());
    cout << "write size:" << n << endl;
    static char buffer[102400];
    //n = dcsutil::readfd(fd, buffer, sizeof(buffer), "end");
    n = readfd(fd, buffer, sizeof(buffer), "token:\r\n");
    cout << "read size:" << n << endl;
    cout << buffer << endl;
    return 0;
}
static int uri_test(const char * arg){
    if (!arg){
        arg = "http://qq.com";
    }    
    int fd = dcsutil::openfd(arg);
    cout << "connect uri:" << arg << " fd:" << fd << endl;
    if (fd < 0){
        return -2;
    }
    static char buffer[102400];
    int n = dcsutil::readfd(fd, buffer, sizeof(buffer), "end");
    //n = readfd(fd, buffer, sizeof(buffer), "token:\r\n");
    cout << "read size:" << n << endl;
    cout << buffer << endl;
    return 0;
}

int main(int argc, char* argv[])
{
	global_logger_init(logger_config_t());
	int agent_mode = 0;
	msgqpath = argv[0];
	if (argc >= 2)
	{
		if (strstr(argv[1], "mongo")){
			return mongo_test(argv[2]);
		}
		if (strstr(argv[1], "pbxml")){
			return pbxml_test(argv[2]);
		}
        if (strstr(argv[1], "bt")){
            return stacktrace_test(argv[2]);
        }
        if (strstr(argv[1], "http")){
            return http_test(argv[2]);
        }
        if (strstr(argv[1], "uri")){
            return uri_test(argv[2]);
        }

		switch (argv[1][0])
		{
		case 'm':
			return test_mq(argv[2]);
		case 't':
			return test_tcp(argv[2]);
		case 'n':
			return test_node(argv[2]);
		case 'a':
			agent_mode = 1;
			break;
		case 'p':
			return python_test();
		case 'l':
			return log_test();
		case 'f':
			return perf_test(argv[1]);
		case 'j':
			return json_test(argv[2]);
		case 'x':
			return xml_test(argv[2]);
		case 'o':
			return lock_test(argv[2]);
		case 'd':
			return daemon_test(argv[2]);
		case 'M':
			return mysql_test(argv[2]);	

		default:
			break;
		}
	}
	return 0;
}
