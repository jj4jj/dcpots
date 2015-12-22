#include "utility/util_mongo.h"
#include "utility/util_proto.h"
#include "base/cmdline_opt.h"
#include "dcnode/dcnode.h"
#include "mongoproxy_msg.h"
#include "base/logger.h"


struct dispatch_param {
	dcsutil::mongo_client_t * mc;
	dcnode_t *		 dc;
	string			 src;
	dcorm::MongoOP	 op;
};

msg_buffer_t g_msg_buffer;

static  void 
on_mongo_result(void * ud, const dcsutil::mongo_client_t::result_t & result, const dcsutil::mongo_client_t::command_t & cmd){
	dispatch_param * cbp = (dispatch_param*)ud;
	mongo_msg_t msg;
	msg.set_op(cbp->op);
	msg.set_db(cmd.db);
    msg.set_coll(cmd.coll);
    if (cmd.cb_size > 0){
        msg.set_cb(cmd.cb_data.data(), cmd.cb_size);
    }
	dcorm::MongoOPRsp & rsp = *msg.mutable_rsp();
	if (result.err_no){
		rsp.set_status(result.err_no);
		rsp.set_error(result.err_msg.data());
	}
	else {
		rsp.set_status(0);
		rsp.set_result(result.rst);
	}
    GLOG_TRA("send msg to:%s msg:%s", cbp->src.c_str(), msg.Debug());
    if (!msg.Pack(g_msg_buffer)){
		GLOG_ERR("pack msg error ! to dst:%s", cbp->src.c_str());
		return;
	}
    int ret = dcnode_send(cbp->dc, cbp->src.c_str(), g_msg_buffer.buffer, g_msg_buffer.valid_size);
	if (ret){
		GLOG_ERR("dcnode send to :%s error !", cbp->src.c_str());
		return;
	}
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
	cbp->op = msg.op();
	GLOG_TRA("recv query :%s", msg.Debug());
	switch (msg.op()){
	case dcorm::MONGO_OP_CMD:
		cbp->mc->command(msg.db(), msg.coll(), on_mongo_result, cbp, 0, msg.req().cmd().c_str());
		break;
	case dcorm::MONGO_OP_FIND:
        do{
            //----------------------------------------------------------------------------
            string projection;
            std::vector<string>     projects;
            std::transform(msg.req().find().projection().begin(), msg.req().find().projection().end(),
                projects.begin(), [](const string & v){return "\"" + v + "\": 1"; });
            dcsutil::strjoin(projection, ",", projects);
            //----------------------------------------------------------------------------
            string sort;
            std::vector<string>     sorts;
            std::transform(msg.req().find().sort().begin(), msg.req().find().sort().end(),
                sorts.begin(), [](const string & v){return "\"" + v + "\": 1"; });
            dcsutil::strjoin(sort, ",", sorts);
            //----------------------------------------------------------------------------
            cbp->mc->find(msg.db(), msg.coll(), msg.req().q(), on_mongo_result, cbp,
                projection.c_str(), sort.c_str(), msg.req().find().skip(), msg.req().find().limit());
        } while (false);
		break;
	case dcorm::MONGO_OP_INSERT:
		cbp->mc->insert(msg.db(), msg.coll(), msg.req().u(), on_mongo_result, cbp);
		break;
	case dcorm::MONGO_OP_UPDATE:
        do{
            string updates = "{ \"q\": ";;
            updates.append(msg.req().q());
            updates.append(", \"u\":");
            updates.append(msg.req().u());
            updates.append("}");
            cbp->mc->update(msg.db(),msg.coll(), updates, on_mongo_result, cbp);
        } while (false);
		break;
	case dcorm::MONGO_OP_DELETE:
        do {
            string deletes = "{ \"q\":";
            deletes.append(msg.req().q());
            deletes.append(",\"limit\": ");
            deletes.append(std::to_string(msg.req().remove().limit()) + "}");
            cbp->mc->remove(msg.db(), msg.coll(), deletes, on_mongo_result, cbp);
        } while (false);
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
	dconf.addr.msgq_addr = listen;
	dconf.addr.msgq_push = false;
	dconf.max_children_heart_beat_expired = 5;
	dconf.max_register_children = 64;
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


