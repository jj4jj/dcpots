
#include "utility/utility_mongo.h"
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
	if (!msg.Pack(g_msg_buffer)){
		LOGP("pack msg error ! to dst:%s", cbp->src.c_str());
		return;
	}
	int ret = dcnode_send(cbp->dc, cbp->src.c_str(), g_msg_buffer.buffer, g_msg_buffer.valid_size);
	if (ret){
		LOGP("dcnode send to :%s error !", cbp->src.c_str());
		return;
	}
}

static int 
dispatch_query(void * ud, const char * src, const msg_buffer_t & msg_buffer){
	dispatch_param * cbp = (dispatch_param*)ud;
	cbp->src = src;
	mongo_msg_t msg;
	if (!msg.Unpack(msg_buffer)){
		LOGP("unpack error msg !");
		return -1;
	}
	switch (msg.op()){
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
		LOGP("unkown op :%d", msg.op());
		return -2;
	}
	return 0;
}

int main(int argc, char * argv[]){

	cmdline_opt_t	cmdline(argc, argv);
	cmdline.parse("daemon:n:D:daemon mode;"
		"log-path:r::log path;"
		"listen:r:l:listen a dcnode address;"
		"mongo-uri:r::mongo uri address;"
		"workers:o::the number of workers thread (default = 2* <numbers core>)");
	//////////////////////////////////////////////////////////////////////////////////
	const char * logpath = cmdline.getoptstr("log");
	const char * listen = cmdline.getoptstr("listen");
	bool daemon = cmdline.getoptstr("daemon") ? true : false;
	//////////////////////////////////////////////////////////////////////////////////
	if (dcsutil::lockpidfile("./mongoproxy.pid")){
		return -2;
	}
	////////////////////////////////////////////////
	dcsutil::mongo_client_config_t	mconf;
	mconf.mongo_uri = cmdline.getoptstr("mongo-uri");
	mconf.multi_thread = cmdline.getoptint("workers");
	dcsutil::mongo_client_t mongo;
	if (mongo.init(mconf)){
		return -4;
	}
	//daemon, log, listen, mongo-uri, mongo-worker
	/////////////////////////////////////////////
	if (daemon){
		dcsutil::daemonlize();
	}
	//dcnode
	////////////////////////////////////////////////
	dcnode_config_t dconf;
	dconf.name = "mongoproxy";
	dconf.addr.listen_addr = listen;
	dcnode_t * dcn = dcnode_create(dconf);
	if (!dcn){
		return -3;
	}
	dispatch_param cbp;
	cbp.mc = &mongo;
	cbp.dc = dcn;
	g_msg_buffer.create(MAX_MONGOPROXY_MSG_SIZE);
	dcnode_set_dispatcher(dcn, dispatch_query, &cbp);
	while (true){
		dcnode_update(dcn, 1000);
		mongo.poll();
	}
	return 0;
}


