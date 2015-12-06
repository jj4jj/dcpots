#include "base/stdinc.h"
#include "base/msg_buffer.hpp"
#include "3rd/pbjson/pbjson.hpp"
#include "../mongoproxy_msg.h"
#include "mongoproxy_api.h"
#include "dcnode/dcnode.h"
#include "base/msg_buffer.hpp"

using namespace dcorm;
using namespace std;
//using namespace dcsutil;

struct mongoproxy_t {
	string					proxyaddr;
	dcnode_t				*dc;
	mongoproxy_cmd_cb_t		cb;
	void					*cb_ud;
	msg_buffer_t			msg_buffer;
}	MONGO;
static int	
on_proxy_rsp(void * ud, const char * src, const msg_buffer_t & msg_buffer ){
	mongo_msg_t msg;
	if (!msg.Unpack(msg_buffer)){
		cerr << "msg unpack error !" << endl;
		return -1;
	}
	mongoproxy_result_t result;
	result.status = msg.rsp().status();
	result.error = msg.rsp().error().c_str();
	mongoproxy_cmd_t cmd_type = MONGO_CMD;
	//json2pb
	switch (msg.op()){
	case MONGO_OP_CMD:
		cmd_type = MONGO_CMD;
		break;
	case MONGO_OP_INSERT:
		cmd_type = MONGO_INSERT;
		break;
	case MONGO_OP_DELETE:
		cmd_type = MONGO_REMOVE;
		break;
	case MONGO_OP_FIND:
		cmd_type = MONGO_FIND;
		break;
	case MONGO_OP_UPDATE:
		cmd_type = MONGO_UPDATE;
		break;
	case MONGO_OP_COUNT:
		cmd_type = MONGO_COUNT;
		break;
	default:
		cerr << "unkown op !" << endl;
		return -2;
	}
	MONGO.cb(cmd_type, MONGO.cb_ud, result);
	return 0;
}
int		
mongoproxy_init(const char * proxyaddr){
	dcnode_config_t dconf;
	dconf.addr.msgq_path = proxyaddr;
	dconf.addr.msgq_push = true;
	dconf.name = "mongoproxyapi";
	dcnode_t * dc = dcnode_create(dconf);
	if (!dc){
		cerr << "dcnode create error !" << endl;
		return -1;
	}
	dcnode_set_dispatcher(dc, on_proxy_rsp , 0);
	MONGO.msg_buffer.create(MAX_MONGOPROXY_MSG_SIZE);
	MONGO.dc = dc;
	MONGO.proxyaddr = proxyaddr;
	return 0;
}
void	
mongoproxy_destroy(){
	if (MONGO.dc){
		dcnode_destroy(MONGO.dc);
		MONGO.dc = nullptr;
	}
}
void	
mongoproxy_set_cmd_cb(mongoproxy_cmd_cb_t cb, void * cb_ud){
	MONGO.cb = cb;
	MONGO.cb_ud = cb_ud;
}

int		mongoproxy_poll(int timeout_ms){
	if (MONGO.dc){
		return dcnode_update(MONGO.dc, timeout_ms*1000);
	}
	return 0;
}
static int 
_mongoproxy_cmd(MongoOP op, const google::protobuf::Message & msg, bool update){
	mongo_msg_t mongo_msg;
	mongo_msg.set_op(op);
	mongo_msg.set_db(msg.GetDescriptor()->file()->package());
	mongo_msg.set_coll(msg.GetDescriptor()->name());
	string json;
	pbjson::pb2json(&msg, json);
	GLOG(LOG_LVL_TRACE, "pb2json :%s", json.c_str());
	if (update){
		mongo_msg.mutable_req()->set_u(json);
	}
	else {
		mongo_msg.mutable_req()->set_q(json);
	}
	if (!mongo_msg.Pack(MONGO.msg_buffer)){
		GLOG(LOG_LVL_WARNING, "pack error :%s", mongo_msg.Debug());
		return -1;
	}
	return dcnode_send(MONGO.dc, "", MONGO.msg_buffer.buffer, MONGO.msg_buffer.valid_size);
}
int		mongoproxy_insert(const google::protobuf::Message & msg){
	return _mongoproxy_cmd(MONGO_OP_INSERT, msg, true);
}
int		mongoproxy_remove(const google::protobuf::Message & msg){
	return _mongoproxy_cmd(MONGO_OP_DELETE, msg, false);
}
int		mongoproxy_find(const google::protobuf::Message & msg){
	return _mongoproxy_cmd(MONGO_OP_FIND, msg, false);
}
int		mongoproxy_update(const google::protobuf::Message & msg){
	return _mongoproxy_cmd(MONGO_OP_UPDATE, msg, true);
}
int		mongoproxy_count(const google::protobuf::Message & msg){
	return _mongoproxy_cmd(MONGO_OP_COUNT, msg, false);
}
