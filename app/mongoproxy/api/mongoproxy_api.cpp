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
    GLOG_TRA("recv proxy msg:%s bytes:%d", msg.Debug(), msg.ByteSize());
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
		GLOG_TRA("cmmongo proxy result: (%s)", jdc.dumps(debug_msg));
        result.ok = jdc["ok"].GetInt();
        if (cmd_type == MONGO_INSERT){
            result.n = jdc["n"].GetInt();
        }
		else if (cmd_type == MONGO_FIND){
            mongoproxy_result_t::mongo_record_t record;
            if (result.ok == 1){//ok
                auto oid = jdc.get("/value/_id/$oid");
                if (!oid || !oid->IsString()){
                    GLOG_TRA("not found the oid result:%s , empty found set", msg.rsp().result().c_str());
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
		}
        else if (cmd_type == MONGO_COUNT){
            result.n = jdc["n"].GetInt();
        }
		else if (cmd_type == MONGO_UPDATE){
            result.n = jdc["n"].GetInt();
            result.modified = jdc["nModified"].GetInt();
		}
		else if (cmd_type == MONGO_REMOVE){
            result.n = jdc["n"].GetInt();
		}
	}	
    result.cb_data = msg.cb().data();
    result.cb_size = msg.cb().length();
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
_mongoproxy_cmd(MongoOP op, const google::protobuf::Message & msg, bool update,
                const dcorm::MongoOPReq * ex, const char * cb_data , int cb_size){
	mongo_msg_t mongo_msg;
	mongo_msg.set_op(op);
	mongo_msg.set_db(msg.GetDescriptor()->file()->package());
	mongo_msg.set_coll(msg.GetDescriptor()->name());
    if (cb_data && cb_size > 0){
        mongo_msg.set_cb(cb_data, cb_size);
    }
	string json;
	pbjson::pb2json(&msg, json);
	GLOG(LOG_LVL_TRACE, "pb2json :%s", json.c_str());
    if (ex){
        mongo_msg.mutable_req()->MergeFrom(*ex);
    }
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
int		mongoproxy_insert(const google::protobuf::Message & msg, const char * cb_data, int cb_size){
	return _mongoproxy_cmd(MONGO_OP_INSERT, msg, true,nullptr, cb_data, cb_size);
}
int		mongoproxy_remove(const google::protobuf::Message & msg, int limit,
                        const char * cb_data, int cb_size){
    MongoOPReq reqremove;
    reqremove.mutable_remove()->set_limit(limit);
    return _mongoproxy_cmd(MONGO_OP_DELETE, msg, false, &reqremove, cb_data, cb_size);
}
int		mongoproxy_find(const google::protobuf::Message & msg, const char * fields ,
                        int skip , int limit , const char * sort,
                        const char * cb_data, int cb_size){
    MongoOPReq reqfind;
    if (limit > 0)
        reqfind.mutable_find()->set_limit(limit);
    if (skip > 0)
       reqfind.mutable_find()->set_skip(skip);
    if (fields){
        std::vector<string> vs;
        dcsutil::strsplit(fields,",", vs);
        for (auto & f : vs){
            reqfind.mutable_find()->add_projection(f);
        }
    }
    if (sort){
        std::vector<string> vs;
        dcsutil::strsplit(fields, ",", vs);
        for (auto & s : vs){
            reqfind.mutable_find()->add_sort(s);
        }
    }
    return _mongoproxy_cmd(MONGO_OP_FIND, msg, false, &reqfind, cb_data, cb_size);
}
int		mongoproxy_update(const google::protobuf::Message & msg, const std::string & fields, const char * cb_data, int cb_size){
    MongoOPReq requpdate;
    std::vector<string> vs;
    dcsutil::strsplit(fields, ",", vs);
    if (vs.empty()){
        GLOG_ERR("not found the fields define :%s", fields.c_str());
        return -1;
    }
    google::protobuf::Message * tmpmsg = msg.New();
    tmpmsg->CopyFrom(msg);
    auto reflect = tmpmsg->GetReflection();
    auto desc = tmpmsg->GetDescriptor();
    for (int i = 0; i < desc->field_count(); ++i){
        auto field = desc->field(i);
        if (std::find(vs.begin(), vs.end(), field->name()) != vs.end()){
            continue;
        }
        reflect->ClearField(tmpmsg, field);
    }
    string fieldsquery;
    pbjson::pb2json(tmpmsg, fieldsquery);
    delete tmpmsg;
    requpdate.set_q(fieldsquery);
    return _mongoproxy_cmd(MONGO_OP_UPDATE, msg, true, &requpdate, cb_data, cb_size);
}
int		mongoproxy_count(const google::protobuf::Message & msg, const char * cb_data, int cb_size){
    return _mongoproxy_cmd(MONGO_OP_COUNT, msg, false, nullptr, cb_data, cb_size);
}
