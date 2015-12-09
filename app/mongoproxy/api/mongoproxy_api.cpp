#include "base/stdinc.h"
#include "base/msg_buffer.hpp"
#include "3rd/pbjson/pbjson.hpp"
#include "../mongoproxy_msg.h"
#include "mongoproxy_api.h"
#include "dcnode/dcnode.h"
#include "base/msg_buffer.hpp"
#include "utility/json_doc.hpp"
#include "utility/util_proto.h"
#include "google/protobuf/descriptor.h"

using namespace dcorm;
using namespace std;
using namespace dcsutil;

struct mongoproxy_t {
	string					proxyaddr;
	dcnode_t				*dc;
	mongoproxy_cmd_cb_t		cb;
	void					*cb_ud;
	msg_buffer_t			msg_buffer;
}	MONGO;

static	inline	mongoproxy_cmd_t  mongoop_to_proxycmd(dcorm::MongoOP	op){
	switch (op){
	case MONGO_OP_CMD:
		return MONGO_CMD;
		break;
	case MONGO_OP_INSERT:
		return MONGO_INSERT;
		break;
	case MONGO_OP_DELETE:
		return MONGO_REMOVE;
		break;
	case MONGO_OP_FIND:
		return MONGO_FIND;
		break;
	case MONGO_OP_UPDATE:
		return MONGO_UPDATE;
		break;
	case MONGO_OP_COUNT:
		return MONGO_COUNT;
		break;
	default:
		cerr << "unkown op !" << endl;
		assert("unknown op !" && false);
		return MONGO_CMD;
	}
}
static int	
on_proxy_rsp(void * ud, const char * src, const msg_buffer_t & msg_buffer ){
	mongo_msg_t msg;
	if (!msg.Unpack(msg_buffer)){
		cerr << "msg unpack error !" << endl;
		return -1;
	}
    GLOG_TRA("recv proxy msg:%s", msg.Debug());
	mongoproxy_result_t result;
	result.status = msg.rsp().status();
	result.error = msg.rsp().error().c_str();
	mongoproxy_cmd_t cmd_type = mongoop_to_proxycmd(msg.op());
    string msg_type_name = msg.db() + "." + msg.coll();
    string debug_msg;
	//json2pb
	if (0 == msg.rsp().status()){
		json_doc_t jdc;
		jdc.loads(msg.rsp().result().c_str());
		string pretty;
		GLOG_TRA("cmmongo proxy result: %s", jdc.pretty(pretty));
		if (cmd_type == MONGO_COUNT){
			//count
		}
        else if (cmd_type == MONGO_INSERT){
            //ok
            result.nsuccess = jdc["ok"].GetInt();
            result.count = jdc["n"].GetInt();
        }
		else if (cmd_type == MONGO_FIND){
            result.nsuccess = jdc["ok"].GetInt();
            result.count = jdc["n"].GetInt();
            mongoproxy_result_t::mongo_record_t record;
            if (result.nsuccess == 1){
                auto oid = jdc.get("value/_id/$oid");
                if (!oid || !oid->IsString()){
                    GLOG_ERR("not found the oid:%s", jdc.pretty(debug_msg));
                }
                else {
                    record.first = oid->GetString();
                    auto new_msg = protobuf_alloc_msg(msg_type_name);
                    if (!new_msg){
                        GLOG_ERR("alloc msg error msg type:%s", msg_type_name.c_str());
                    }
                    else {
                        if (pbjson::jsonobject2pb(jdc.get("value"), new_msg, debug_msg)){
                            GLOG_ERR("json 2 pb error for:%s", debug_msg.c_str());
                            protobuf_free_msg(new_msg);
                            new_msg = nullptr;
                        }
                    }
                    if (new_msg){
                        record.second = new_msg;
                        result.results.push_back(record);
                    }
                }
            }
            else {
                #warning "todo"
            }
            //value
		}
		else if (cmd_type == MONGO_UPDATE){

		}
		else if (cmd_type == MONGO_REMOVE){

		}
	}	
	MONGO.cb(cmd_type, MONGO.cb_ud, result);
    //free msg
    for (auto & record : result.results){
        protobuf_free_msg(record.second);
    }
	return 0;
}
int		
mongoproxy_init(const char * proxyaddr){
	dcnode_config_t dconf;
	dconf.addr.msgq_path = proxyaddr;
	dconf.addr.msgq_push = true;
	dconf.parent_heart_beat_gap = 2;//2s
	//randomname
	dconf.name = "mgpapi";
	strrandom(dconf.name);
	dcnode_t * dc = dcnode_create(dconf);
	if (!dc){
		cerr << "dcnode create error !" << endl;
		return -1;
	}
	dcnode_set_dispatcher(dc, on_proxy_rsp , 0);
	MONGO.msg_buffer.create(MAX_MONGOPROXY_MSG_SIZE);
	MONGO.dc = dc;
	MONGO.proxyaddr = proxyaddr;
    protobuf_logger_init();

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
