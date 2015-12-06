
#include "utility/util_mongo.h"
#include "utility/util_proto.h"
#include "base/cmdline_opt.h"
#include "dcnode/dcnode.h"
#include "mongoproxy_msg.h"
struct dispatch_param {
	dcsutil::mongo_client_t * mc;
	dcnode_t *		 dc;
	string			 src;
};

msg_buffer_t g_msg_buffer;

static  void 
on_mongo_result(void * ud, const dcsutil::mongo_client_t::result_t & result){
	dispatch_param * cbp = (dispatch_param*)ud;
	mongo_msg_t msg;
	msg.set_db("unknown");
	msg.set_coll("unknown");
	msg.mutable_rsp()->set_status(-1);
	msg.mutable_rsp()->set_error("not implement");
	if (!msg.Pack(g_msg_buffer)){
		GLOG_ERR("pack msg error ! to dst:%s", cbp->src.c_str());
		return;
	}
	int ret = dcnode_send(cbp->dc, cbp->src.c_str(), g_msg_buffer.buffer, g_msg_buffer.valid_size);
	if (ret){
		GLOG_ERR("dcnode send to :%s error !", cbp->src.c_str());
		return;
	}
	GLOG_TRA("send msg to:%s msg:%s", cbp->src.c_str(), msg.Debug());
}

static int 
dispatch_query(void * ud, const char * src, const msg_buffer_t & msg_buffer){
	dispatch_param * cbp = (dispatch_param*)ud;
	cbp->src = src;
	mongo_msg_t msg;
	if (!msg.Unpack(msg_buffer)){
		GLOG_TRA("unpack error msg !");
		return -1;
	}
	GLOG_TRA("recv query :%s", msg.Debug());
	switch (msg.op()){
	case dcorm::MONGO_OP_CMD:
	do{
		dcsutil::mongo_client_t::command_t command;
		command.db = msg.db();
		command.coll = msg.coll();
		command.cmd = msg.req().cmd();
		command.cmd_length = msg.req().cmd().length();
		cbp->mc->excute(command, on_mongo_result, cbp);
	} while (false);
		break;
	case dcorm::MONGO_OP_FIND:
		cbp->mc->find(msg.db(), msg.coll(), msg.req().q(), on_mongo_result, cbp);
		break;
	case dcorm::MONGO_OP_INSERT:
		cbp->mc->insert(msg.db(), msg.coll(), msg.req().u(), on_mongo_result, cbp);
		break;
	case dcorm::MONGO_OP_UPDATE:
		cbp->mc->update(msg.db(), msg.coll(), msg.req().u(), on_mongo_result, cbp);
		break;
	case dcorm::MONGO_OP_DELETE:
		cbp->mc->remove(msg.db(), msg.coll(), msg.req().q(), on_mongo_result, cbp);
		break;
	case dcorm::MONGO_OP_COUNT:
		cbp->mc->count(msg.db(), msg.coll(), msg.req().q(), on_mongo_result, cbp);
		break;
	default:
		GLOG_TRA("unkown op :%d", msg.op());
		return -2;
	}
	return 0;
}
using namespace std;
int main(int argc, char * argv[]){

	cmdline_opt_t	cmdline(argc, argv);
	cmdline.parse("daemon:n:D:daemon mode;"
		"log-path:r::log path;"
		"listen:r:l:listen a dcnode address:/tmp;"
		"mongo-uri:r::mongo uri address:mongodb://127.0.0.1:27017;"
		"workers:o::the number of workers thread:2");
	//daemon, log, listen, mongo-uri, mongo-worker
	//////////////////////////////////////////////////////////////////////////////////
	const char * logpath = cmdline.getoptstr("log-path");
	if (logpath){
		logger_config_t logconf;
		logconf.dir = logpath;
		logconf.pattern = "mongoproxy.log";
		global_logger_init(logconf);
	}
	else {
		global_logger_init(logger_config_t());
	}
	dcsutil::protobuf_logger_init();

	const char * listen = cmdline.getoptstr("listen");
	bool daemon = cmdline.hasopt("daemon");
	////////////////////////////////////////////////
	dcsutil::mongo_client_config_t	mconf;
	mconf.mongo_uri = cmdline.getoptstr("mongo-uri");
	mconf.multi_thread = cmdline.getoptint("workers");
	dcsutil::mongo_client_t mongo;
	if (mongo.init(mconf)){
		cerr << "mongo client init error !" << endl;
		return -4;
	}
	//dcnode
	////////////////////////////////////////////////
	dcnode_config_t dconf;
	dconf.name = "mongoproxy";
	//dconf.addr.listen_addr = listen;
	dconf.addr.msgq_path = listen;
	dconf.addr.msgq_push = false;
	dcnode_t * dcn = dcnode_create(dconf);
	if (!dcn){
		cerr << "dcnode msgq init error !" << endl;
		return -3;
	}
	dispatch_param cbp;
	cbp.mc = &mongo;
	cbp.dc = dcn;
	g_msg_buffer.create(MAX_MONGOPROXY_MSG_SIZE);
	dcnode_set_dispatcher(dcn, dispatch_query, &cbp);

	/////////////////////////////////////////////
	if (daemon){
		dcsutil::daemonlize();
	}
	//////////////////////////////////////////////////////////////////////////////////
	if (dcsutil::lockpidfile("./mongoproxy.pid") != getpid()){
		cerr << "lock file error !" << endl;
		return -2;
	}
	//////////////////////////////////////////////////////////////////////////////////

	while (true){
		dcnode_update(dcn, 1000);
		mongo.poll();
	}
	return 0;
}


