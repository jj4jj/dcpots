#include "google/protobuf/message.h"
#include "../../3rd/pbjson/pbjson.hpp"
#include "../../base/logger.h"
#include "dcproto.h"
#include "dcxml.h"
#include "dcjson.hpp"

NS_BEGIN(dcsutil)

using namespace google::protobuf;
static logger_t * g_debug_logger = nullptr;
static void 
_protobuf_global_logger_printer(google::protobuf::LogLevel level, 
	const char* filename, int line,
	const std::string& message){
	log_msg_level_type	loglv;
	switch (level){
	case google::protobuf::LOGLEVEL_INFO:
		loglv = LOG_LVL_INFO;
	case google::protobuf::LOGLEVEL_WARNING:
		loglv = LOG_LVL_WARNING;
	case google::protobuf::LOGLEVEL_ERROR:
		loglv = LOG_LVL_WARNING;
	case google::protobuf::LOGLEVEL_FATAL:
		loglv = LOG_LVL_FATAL;
	default:
		loglv = LOG_LVL_DEBUG;
	}
	LOG(g_debug_logger, loglv, 0, "protobuf log info (lv:%d filename:%s:%d msg:%s)",
		level, filename, line, message.c_str());
}
void
protobuf_logger_init(logger_t * logger){
    g_debug_logger = logger;
	google::protobuf::SetLogHandler(_protobuf_global_logger_printer);
}

google::protobuf::Message *
protobuf_alloc_msg(const std::string & full_name){
    auto desc = protobuf_find_desc(full_name);
    if (desc){
        auto prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(desc);
        return prototype->New();
    }
    return nullptr;
}
void
protobuf_free_msg(google::protobuf::Message * msg){
    if (msg){
        delete msg;
    }
}
const google::protobuf::Descriptor *
protobuf_find_desc(const std::string & full_name){
     return google::protobuf::DescriptorPool::internal_generated_pool()->FindMessageTypeByName(full_name);
}
void      protobuf_msg_fill_default(google::protobuf::Message * msgp) {
    const Reflection* reflection = msgp->GetReflection();
    for (int i = 0; i < msgp->GetDescriptor()->field_count(); ++i) {
        const FieldDescriptor * field = msgp->GetDescriptor()->field(i);
        if (field->is_repeated()) {
            switch (field->cpp_type()) {
            case FieldDescriptor::CPPTYPE_INT32:     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
            reflection->AddInt32(msgp, field, field->default_value_int32());
            break;
            case FieldDescriptor::CPPTYPE_INT64:     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
            reflection->AddInt64(msgp, field, field->default_value_int64());
            break;
            case FieldDescriptor::CPPTYPE_UINT32:     // TYPE_UINT32, TYPE_FIXED32
            reflection->AddUInt32(msgp, field, field->default_value_uint32());
            break;
            case FieldDescriptor::CPPTYPE_UINT64:     // TYPE_UINT64, TYPE_FIXED64
            reflection->AddUInt64(msgp, field, field->default_value_uint64());
            break;

            case FieldDescriptor::CPPTYPE_DOUBLE:     // TYPE_DOUBLE
            reflection->AddDouble(msgp, field, field->default_value_double());
            break;

            case FieldDescriptor::CPPTYPE_FLOAT:     // TYPE_FLOAT
            reflection->AddFloat(msgp, field, field->default_value_float());
            break;

            case FieldDescriptor::CPPTYPE_BOOL:     // TYPE_BOOL
            reflection->AddBool(msgp, field, field->default_value_bool());
            break;

            case FieldDescriptor::CPPTYPE_ENUM:     // TYPE_ENUM
            reflection->AddEnum(msgp, field, field->default_value_enum());
            break;

            case FieldDescriptor::CPPTYPE_STRING:     // TYPE_STRING, TYPE_BYTES
            reflection->AddString(msgp, field, field->default_value_string());
            break;

            case FieldDescriptor::CPPTYPE_MESSAGE:    // TYPE_MESSAGE, TYPE_GROUP
            if (true) {
                ::google::protobuf::Message * pFieldMsg = reflection->AddMessage(msgp, field);
                protobuf_msg_fill_default(pFieldMsg);
            }
            break;
            default:
            break;
            }
        }
        else {
            switch (field->cpp_type()) {
            case FieldDescriptor::CPPTYPE_INT32:     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
            reflection->SetInt32(msgp, field, field->default_value_int32());
            break;
            case FieldDescriptor::CPPTYPE_INT64:     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
            reflection->SetInt64(msgp, field, field->default_value_int64());
            break;
            case FieldDescriptor::CPPTYPE_UINT32:     // TYPE_UINT32, TYPE_FIXED32
            reflection->SetUInt32(msgp, field, field->default_value_uint32());
            break;
            case FieldDescriptor::CPPTYPE_UINT64:     // TYPE_UINT64, TYPE_FIXED64
            reflection->SetUInt64(msgp, field, field->default_value_uint64());
            break;

            case FieldDescriptor::CPPTYPE_DOUBLE:     // TYPE_DOUBLE
            reflection->SetDouble(msgp, field, field->default_value_double());
            break;

            case FieldDescriptor::CPPTYPE_FLOAT:     // TYPE_FLOAT
            reflection->SetFloat(msgp, field, field->default_value_float());
            break;

            case FieldDescriptor::CPPTYPE_BOOL:     // TYPE_BOOL
            reflection->SetBool(msgp, field, field->default_value_bool());
            break;

            case FieldDescriptor::CPPTYPE_ENUM:     // TYPE_ENUM
            reflection->SetEnum(msgp, field, field->default_value_enum());
            break;

            case FieldDescriptor::CPPTYPE_STRING:     // TYPE_STRING, TYPE_BYTES
            reflection->SetString(msgp, field, field->default_value_string());
            break;

            case FieldDescriptor::CPPTYPE_MESSAGE:    // TYPE_MESSAGE, TYPE_GROUP
            if (true) {
                ::google::protobuf::Message * pFieldMsg = reflection->MutableMessage(msgp, field);
                protobuf_msg_fill_default(pFieldMsg);
            }
            break;
            default:
            break;
            }
        }
    }
}

std::string 
protobuf_msg_field_get_value(const Message & msg, const string & name, int idx){
	const Reflection* reflection = msg.GetReflection();
	const FieldDescriptor * field = msg.GetDescriptor()->FindFieldByName(name);
	if (!reflection || !field){
		GLOG_ERR("not found field :%s", name.c_str());
		return "";
	}
	if (field->is_repeated()){
		switch (field->cpp_type()){
		case FieldDescriptor::CPPTYPE_INT32:     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
			return to_string(reflection->GetRepeatedInt32(msg, field, idx));
		case FieldDescriptor::CPPTYPE_INT64 :     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
			return to_string(reflection->GetRepeatedInt64(msg, field, idx));
		case FieldDescriptor::CPPTYPE_UINT32:     // TYPE_UINT32, TYPE_FIXED32
			return to_string(reflection->GetRepeatedUInt32(msg, field, idx));
		case FieldDescriptor::CPPTYPE_UINT64:     // TYPE_UINT64, TYPE_FIXED64
			return to_string(reflection->GetRepeatedUInt64(msg, field, idx));
		case FieldDescriptor::CPPTYPE_DOUBLE:     // TYPE_DOUBLE
			return to_string(reflection->GetRepeatedDouble(msg, field, idx));
		case FieldDescriptor::CPPTYPE_FLOAT:     // TYPE_FLOAT
			return to_string(reflection->GetRepeatedFloat(msg, field, idx));
		case FieldDescriptor::CPPTYPE_BOOL:     // TYPE_BOOL
			return reflection->GetRepeatedBool(msg, field, idx)? "true": "false";
		case FieldDescriptor::CPPTYPE_ENUM:     // TYPE_ENUM
			return reflection->GetRepeatedEnum(msg, field, idx)->name();
		case FieldDescriptor::CPPTYPE_STRING:     // TYPE_STRING, TYPE_BYTES
			return reflection->GetRepeatedString(msg, field, idx);		
		case FieldDescriptor::CPPTYPE_MESSAGE :    // TYPE_MESSAGE, TYPE_GROUP
			return reflection->GetRepeatedMessage(msg, field, idx).SerializeAsString();
		default:
			return "";
		}
	}
	else {
		switch (field->cpp_type()){
		case FieldDescriptor::CPPTYPE_INT32:     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
			return to_string(reflection->GetInt32(msg, field));
		case FieldDescriptor::CPPTYPE_INT64:     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
			return to_string(reflection->GetInt64(msg, field));
		case FieldDescriptor::CPPTYPE_UINT32:     // TYPE_UINT32, TYPE_FIXED32
			return to_string(reflection->GetUInt32(msg, field));
		case FieldDescriptor::CPPTYPE_UINT64:     // TYPE_UINT64, TYPE_FIXED64
			return to_string(reflection->GetUInt64(msg, field));
		case FieldDescriptor::CPPTYPE_DOUBLE:     // TYPE_DOUBLE
			return to_string(reflection->GetDouble(msg, field));
		case FieldDescriptor::CPPTYPE_FLOAT:     // TYPE_FLOAT
			return to_string(reflection->GetFloat(msg, field));
		case FieldDescriptor::CPPTYPE_BOOL:     // TYPE_BOOL
			return reflection->GetBool(msg, field) ? "true" : "false";
		case FieldDescriptor::CPPTYPE_ENUM:     // TYPE_ENUM
			return reflection->GetEnum(msg, field)->name();
		case FieldDescriptor::CPPTYPE_STRING:     // TYPE_STRING, TYPE_BYTES
			return reflection->GetString(msg, field);
		case FieldDescriptor::CPPTYPE_MESSAGE:    // TYPE_MESSAGE, TYPE_GROUP
			return reflection->GetMessage(msg, field).SerializeAsString();
		default:
			return "";
		}
	}
}
static inline void
protobuf_path_split(const std::string & path, std::vector<std::string> & vps){
    dcsutil::strsplit(path, ".", vps);
}

static inline int 
protobuf_path_field_split(const std::string & field, std::string & name){
    std::vector<std::string> vsf;
    strsplit(field, ":", vsf);
    name = vsf[0];
    if (vsf.size() > 1){
        return std::stoi(vsf[1]);
    }
    return -1;
}
const Message * protobuf_msg_field_path_get(const google::protobuf::Message & msg, const string & path, string & error){
    const google::protobuf::Message * msgp = &msg;
    std::vector<std::string>    vs;
    protobuf_path_split(path, vs);
    const FieldDescriptor * field = nullptr;
    const Reflection* reflection = nullptr;
    std::vector<std::string>    vsf;
    std::string field_name;
    int   field_idx = -1;
    for (size_t i = 0; i < vs.size(); ++i) {
        field_idx = protobuf_path_field_split(vs[i], field_name);
        field = msgp->GetDescriptor()->FindFieldByName(field_name);
        if (!field) {
            strnprintf(error, 128 + path.length(), "not found field :%s in path:%s", vs[i].c_str(), path.c_str());
            return nullptr;
        }
        if (field->message_type()){
            if (i + 1 == vs.size()){
                strnprintf(error, 128 + path.length(), "error :field :%s is a message in path:%s!", vs.back().c_str(), path.c_str());
                return nullptr;
            }
            else {
                reflection = msgp->GetReflection();
                if (!reflection){
                    strnprintf(error, 64, "not found reflection :%s", msgp->GetTypeName().c_str());
                    return nullptr;
                }
                if (field->is_repeated()){
                    //get
                    msgp = &reflection->GetRepeatedMessage(*msgp, field, field_idx);
                }
                else {
                    msgp = &reflection->GetMessage(*msgp, field);
                }
            }
        }
        else {
            if ((i + 1) != vs.size()){
                strnprintf(error, 128 + path.length(), "error :last field :%s is not a message in path:%s!", vs.back().c_str(), path.c_str());
                return nullptr;
            }
        }
    }
    return msgp;
}
Message * protobuf_msg_field_path_get(google::protobuf::Message & msg, const string & path, string & error){
    google::protobuf::Message * msgp = &msg;
    std::vector<std::string>    vs;
    dcsutil::strsplit(path, ".", vs);
    const FieldDescriptor * field = nullptr;
    const Reflection* reflection = nullptr;
    std::vector<std::string>    vsf;
    std::string field_name;
    int   field_idx = -1;
    for (size_t i = 0; i < vs.size(); ++i) {
        field_idx = protobuf_path_field_split(vs[i], field_name);
        field = msgp->GetDescriptor()->FindFieldByName(field_name);
        if (!field) {
            strnprintf(error, 128 + path.length(), "not found field :%s in path:%s", vs[i].c_str(), path.c_str());
            return nullptr;
        }
        if (field->message_type()){
            if (i + 1 == vs.size()){
                strnprintf(error, 128 + path.length(), "error :field :%s is a message in path:%s!", vs.back().c_str(), path.c_str());
                return nullptr;
            }
            else {
                reflection = msgp->GetReflection();
                if (!reflection){
                    strnprintf(error, 64, "not found reflection :%s", msgp->GetTypeName().c_str());
                    return nullptr;
                }
                if (field->is_repeated()){
                    //get
                    msgp = reflection->MutableRepeatedMessage(msgp, field, field_idx);
                }
                else {
                    msgp = reflection->MutableMessage(msgp, field);
                }
            }
        }
        else {
            if ((i + 1) != vs.size()){
                strnprintf(error, 128 + path.length(), "error :last field :%s is not a message in path:%s!", vs.back().c_str(), path.c_str());
                return nullptr;
            }
        }
    }
    return msgp;
}
int	protobuf_msg_field_path_get_value(std::string & value, const google::protobuf::Message & msg, const string & path, std::string & error){
    const Message * msgp = protobuf_msg_field_path_get(msg, path, error);
    if (!msgp){
        return -1;
    }
    std::vector<std::string>    vps;
    protobuf_path_split(path, vps);
    std::string field_name;
    int field_idx = protobuf_path_field_split(vps.back(), field_name);
    value = protobuf_msg_field_get_value(*msgp, field_name, field_idx);
    return 0;
}
int				
protobuf_msg_field_path_set_value(google::protobuf::Message & msg, const string & path, const string & value, string & error) {
    Message * msgp = protobuf_msg_field_path_get(msg, path, error);
    if (!msgp){
        return -1;
    }
    std::vector<std::string>    vps;
    protobuf_path_split(path, vps);
    std::string field_name;
    int field_idx = protobuf_path_field_split(vps.back(), field_name);
    return protobuf_msg_field_set_value(*msgp, field_name, field_idx, value, error);
}
int
protobuf_msg_field_set_value(Message & msg, const string & name, int idx,
	const string & value, string & error){
	const Reflection* reflection = msg.GetReflection();
	const FieldDescriptor * field = msg.GetDescriptor()->FindFieldByName(name);
	if (!reflection || !field){
		strnprintf(error, 64, "not found field :%s", name.c_str());
		return -1;
	}
	if (field->is_repeated() && idx >= 0){ //set
		switch (field->cpp_type()){
		case FieldDescriptor::CPPTYPE_INT32:     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
			reflection->SetRepeatedInt32(&msg, field, idx, std::stoi(value));
			break;
		case FieldDescriptor::CPPTYPE_INT64:     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
			reflection->SetRepeatedInt64(&msg, field, idx, stoll(value));
			break;
		case FieldDescriptor::CPPTYPE_UINT32:     // TYPE_UINT32, TYPE_FIXED32			
			reflection->SetRepeatedUInt32(&msg, field, idx, (uint32_t)(stoi(value)));
			break;
		case FieldDescriptor::CPPTYPE_UINT64:     // TYPE_UINT64, TYPE_FIXED64
			reflection->SetRepeatedUInt64(&msg, field, idx, (uint64_t)(stoll(value)));
			break;
		case FieldDescriptor::CPPTYPE_DOUBLE:     // TYPE_DOUBLE
			reflection->SetRepeatedDouble(&msg, field, idx, stod(value));
			break;
		case FieldDescriptor::CPPTYPE_FLOAT:     // TYPE_FLOAT
			reflection->SetRepeatedFloat(&msg, field, idx, stof(value));
			break;
		case FieldDescriptor::CPPTYPE_BOOL:     // TYPE_BOOL
			reflection->SetRepeatedBool(&msg, field, idx, value == "true" ? true : false);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:     // TYPE_ENUM
			do {
				auto ev = field->enum_type()->FindValueByName(value);
				if (!ev){
					strnprintf(error, 64, "enum value:%s not found", value.c_str());
					return -1;
				}
				reflection->SetRepeatedEnum(&msg, field, idx, ev);
				break;
			} while (false);
			break;
		case FieldDescriptor::CPPTYPE_STRING:     // TYPE_STRING, TYPE_BYTES
			reflection->SetRepeatedString(&msg, field, idx, value);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:    // TYPE_MESSAGE, TYPE_GROUP
			do {
				auto nmsg = reflection->MutableRepeatedMessage(&msg, field, idx);
				if (!nmsg){
					strnprintf(error, 64, "mutable idx msg error :%s , %d", field->full_name().c_str(), idx);
					return -2;
				}
				if (!nmsg->ParseFromString(value)){
					strnprintf(error, 64, "parse from value error :%s <= %s !", field->full_name().c_str(), value.c_str());
					return -3;
				}
			} while (false);
			break;
		default:
			return -10;
		}
	}
	else if (field->is_repeated() && idx < 0){ //add
		switch (field->cpp_type()){
		case FieldDescriptor::CPPTYPE_INT32:     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
			reflection->AddInt32(&msg, field, std::stoi(value));
			break;
		case FieldDescriptor::CPPTYPE_INT64:     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
			reflection->AddInt64(&msg, field, stoll(value));
			break;
		case FieldDescriptor::CPPTYPE_UINT32:     // TYPE_UINT32, TYPE_FIXED32			
			reflection->AddUInt32(&msg, field, (uint32_t)(stoi(value)));
			break;
		case FieldDescriptor::CPPTYPE_UINT64:     // TYPE_UINT64, TYPE_FIXED64
			reflection->AddUInt64(&msg, field, (uint64_t)(stoll(value)));
			break;
		case FieldDescriptor::CPPTYPE_DOUBLE:     // TYPE_DOUBLE
			reflection->AddDouble(&msg, field, stod(value));
			break;
		case FieldDescriptor::CPPTYPE_FLOAT:     // TYPE_FLOAT
			reflection->AddFloat(&msg, field, stof(value));
			break;
		case FieldDescriptor::CPPTYPE_BOOL:     // TYPE_BOOL
			reflection->AddBool(&msg, field, (value == "true") ? true : false);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:     // TYPE_ENUM
			do {
				auto ev = field->enum_type()->FindValueByName(value);
				if (!ev){
					strprintf(error, "enum value:%s not found", value.c_str());
					return -1;
				}
				reflection->AddEnum(&msg, field, ev);
				break;
			} while (false);
			break;
		case FieldDescriptor::CPPTYPE_STRING:     // TYPE_STRING, TYPE_BYTES
			reflection->AddString(&msg, field, value);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:    // TYPE_MESSAGE, TYPE_GROUP
			do {
				auto nmsg = reflection->AddMessage(&msg, field);
				if (!nmsg){
					strnprintf(error, 200, "mutable idx msg error :%s , %d", field->full_name().c_str(), idx);
					return -2;
				}
				if (!nmsg->ParseFromString(value)){
					strnprintf(error, 200, "parse from value error :%s <= %s !", field->full_name().c_str(), value.c_str());
					return -3;
				}
			} while (false);
			break;
		default:
			return -10;
		}
	}
	else {
		switch (field->cpp_type()){
		case FieldDescriptor::CPPTYPE_INT32:     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
			reflection->SetInt32(&msg, field, std::stoi(value));
			break;
		case FieldDescriptor::CPPTYPE_INT64:     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
			reflection->SetInt64(&msg, field, stoll(value));
			break;
		case FieldDescriptor::CPPTYPE_UINT32:     // TYPE_UINT32, TYPE_FIXED32			
			reflection->SetUInt32(&msg, field, (uint32_t)(stoi(value)));
			break;
		case FieldDescriptor::CPPTYPE_UINT64:     // TYPE_UINT64, TYPE_FIXED64
			reflection->SetUInt64(&msg, field, (uint64_t)(stoll(value)));
			break;
		case FieldDescriptor::CPPTYPE_DOUBLE:     // TYPE_DOUBLE
			reflection->SetDouble(&msg, field, stod(value));
			break;
		case FieldDescriptor::CPPTYPE_FLOAT:     // TYPE_FLOAT
			reflection->SetFloat(&msg, field, stof(value));
			break;
		case FieldDescriptor::CPPTYPE_BOOL:     // TYPE_BOOL
			reflection->SetBool(&msg, field, (value == "true")?true:false);
			break;
		case FieldDescriptor::CPPTYPE_ENUM:     // TYPE_ENUM
			do {
				auto ev = field->enum_type()->FindValueByName(value);
				if (!ev){
					strprintf(error, "enum value:%s not found", value.c_str());
					return -1;
				}
				reflection->SetEnum(&msg, field, ev);
				break;
			} while (false);
			break;
		case FieldDescriptor::CPPTYPE_STRING:     // TYPE_STRING, TYPE_BYTES
			reflection->SetString(&msg, field, value);
			break;
		case FieldDescriptor::CPPTYPE_MESSAGE:    // TYPE_MESSAGE, TYPE_GROUP
			do {
				auto nmsg = reflection->MutableMessage(&msg, field);
				if (!nmsg){
					strnprintf(error, 200, "mutable idx msg error :%s , %d", field->full_name().c_str(), idx);
					return -2;
				}
				if (!nmsg->ParseFromString(value)){
					strnprintf(error, 200, "parse from value error :%s <= %s !", field->full_name().c_str(), value.c_str());
					return -3;
				}
			} while (false);
			break;
		default:
			return -10;
		}
	}
	return 0;
}

void	
protobuf_msg_sax(const string & name, const Message & msg, sax_event_cb_t fn, void *ud, int level, bool default_init){
	auto desc = msg.GetDescriptor();
	auto reflection = msg.GetReflection();
	fn(name, msg, -1, level, ud, BEGIN_MSG);
	for (int i = 0; i < desc->field_count(); ++i){
		auto field = desc->field(i);
        if (!default_init && !reflection->HasField(msg, field)){
			continue;
		}
		if (field->is_repeated()){
			fn(field->name(), msg, -1, level + 1, ud, BEGIN_ARRAY);
			int acount = reflection->FieldSize(msg, field);
			for (int j = 0; j < acount; ++j){
				if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE){
					auto & amsg = reflection->GetRepeatedMessage(msg, field, j);
                    protobuf_msg_sax(field->name(), amsg, fn, ud, level + 1, default_init);
				}
				else {
					fn(field->name(), msg, j, level + 1, ud, VISIT_VALUE);
				}
			}
			fn(field->name(), msg, -1, level + 1, ud, END_ARRAY);
		}
		else {
			if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE){
				auto & amsg = reflection->GetMessage(msg, field);
                protobuf_msg_sax(field->name(), amsg, fn, ud, level + 1, default_init);
			}
			else {
				fn(field->name(), msg, -1, level + 1, ud, VISIT_VALUE);
			}
		}
	}
	fn(name, msg, -1, level, ud, END_MSG);
}

static void
convert_to_xml(const string & name, const Message & msg, int idx,
int level, void * ud, protobuf_sax_event_type ev_type){
	string * xml = (string*)ud;
	//<xml>
	strrepeat(*xml, "    ", level);
	switch (ev_type){
	case BEGIN_MSG:
        if (level == 0){
            xml->append("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
        }
		xml->append("<");
		xml->append(name);
		xml->append(" type=\"");
		xml->append(msg.GetDescriptor()->name());
		xml->append("\">");
		break;
	case END_MSG:
		xml->append("</");
        xml->append(name);
        xml->append(">");
		break;
	case BEGIN_ARRAY:
		break;
	case END_ARRAY:
		break;
	case VISIT_VALUE:
		xml->append("<");
		xml->append(name);
		xml->append(">");

		xml->append(protobuf_msg_field_get_value(msg, name, idx));

		xml->append("</");
		xml->append(name);
		xml->append(">");
		break;
	}
	xml->append("\n");
}
int				
protobuf_msg_to_xml_file(const google::protobuf::Message & msg, const std::string & xmlfile){
    string xml;
    protobuf_msg_to_xml_string(msg, xml);
    int sz = writefile(xmlfile.c_str(), xml.c_str(), xml.length());
    if (sz > 0)
        return 0;
    else {
        GLOG_ERR("write file :%s error : %d", xmlfile.c_str(), sz);
        return -1;
    }
}
int			
protobuf_msg_to_xml_string(const google::protobuf::Message & msg, std::string & sxml){
    protobuf_msg_sax(msg.GetDescriptor()->name(), msg, convert_to_xml, &sxml);
    return 0;
}

static void
convert_xml_to_pb(xml_node_t * node, int lv, void *ud, xml_doc_t::sax_event_type evt){
    UNUSED(lv);
	std::stack<Message *> * msgstack = (std::stack<Message *> *)ud;
    if (xml_doc_t::node_type(node) != 1){//must be element
        return;
    }
    if (msgstack->empty()){
        GLOG_ERR("msg stack is empty ! please check the error !");
        return;
    }
    auto msg = msgstack->top();
	auto desc =	msg->GetDescriptor();
	auto reflection = msg->GetReflection();
    auto name = xml_doc_t::node_name(node);
    // GLOG_TRA("xml sax converting => %s.%s evt:%d node type:%d",
    //     msg->GetTypeName().c_str(), name, evt, xml_doc_t::node_type(node));
    auto field = desc->FindFieldByName(name);
    if (!field){
        if (evt == xml_doc_t::END_NODE){
            //GLOG_TRA("pop msg type:%s", msg->GetTypeName().c_str());
            msgstack->pop();
        }
        else if (msg->GetDescriptor()->name() != name) {
            GLOG_WAR("not found the field current env msg:%s node:%s", msg->GetTypeName().c_str(), name);
        }
        return;
    }
	switch (evt){
	case xml_doc_t::BEGIN_NODE:
		if (field){
			if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE){
				Message * nmsg = nullptr;
				if (field->is_repeated()){
					nmsg = reflection->AddMessage(msg, field);
				}
				else {
					nmsg = reflection->MutableMessage(msg, field);
				}
                //GLOG_TRA("push msg type:%s", nmsg->GetTypeName().c_str());
                msgstack->push(nmsg);
			}
			else {
				string error;
				if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE){
					int ret = protobuf_msg_field_set_value(*msg, name, -1, xml_doc_t::node_value(node), error);
					if (ret){
						GLOG_ERR("set value error ret:%d for :%s", ret, error.c_str());
					}
				}
			}
		}
		break;
	case xml_doc_t::END_NODE:
        //visit end
		break;
	default:
		return;
	}
}
int
protobuf_msg_from_xml_string(google::protobuf::Message & msg, const std::string & sxml, std::string & error){
    xml_doc_t xml;
    string xmlstr = sxml;
    int ret = xml.loads((char*)xmlstr.data());
    if (ret){
        error = "parse from buffer error !";
        return ret;
    }
    std::stack<Message*>		msgstack;
    msgstack.push(&msg);
    xml.sax(convert_xml_to_pb, &msgstack, nullptr);
    return 0;
}
int				
protobuf_msg_from_xml_file(google::protobuf::Message & msg, const std::string & xmlfile, std::string & error){
	xml_doc_t xml;
	int ret = xml.parse_file(xmlfile.c_str());
	if (ret){
		error = "parse from file error !";
		return ret;
	}
	std::stack<Message*>		msgstack;
	msgstack.push(&msg);
	xml.sax(convert_xml_to_pb, &msgstack, nullptr);
	return 0;
}


int
protobuf_msg_to_json_string(const google::protobuf::Message & msg, std::string & json){
    pbjson::pb2json(&msg, json);
    return 0;
}
int
protobuf_msg_from_json_string(google::protobuf::Message & msg, const std::string & sjson, std::string & error){
    int ret = pbjson::json2pb(sjson, &msg, error);
    if (ret){
        GLOG_ERR("json to pb error :%d error :%s", ret, error.c_str());
        return ret;
    }
    return 0;
}
int 
protobuf_msg_to_json_file(const google::protobuf::Message & msg, const std::string & jsonfile){
    string msg_buffer;
    protobuf_msg_to_json_string(msg, msg_buffer);
    int sz = dcsutil::writefile(jsonfile, msg_buffer.data(), msg_buffer.length());
    if (sz <= 0){
        GLOG_SER("write file :%s error :%d", jsonfile.c_str(), sz);
        return -1;
    }
    return 0;
}
int             
protobuf_msg_to_msgb_file(const google::protobuf::Message & msg, const std::string & msgbfile) {
    std::string msgb;
    if (!msg.SerializeToString(&msgb)) {
        GLOG_ERR("protomsg msg serialize error !");
        return -1;
    }
    int sz = dcsutil::writefile(msgbfile, msgb.data(), msgb.length());
    if (sz <= 0) {
        GLOG_SER("write file :%s error :%d", msgbfile.c_str(), sz);
        return -1;
    }
    return 0;
}
int             
protobuf_msg_from_msgb_file(google::protobuf::Message & msg, const std::string & msgbfile) {
    string sfile;
    int sz = dcsutil::filesize(msgbfile);
    sfile.reserve(sz);
    int n = dcsutil::readfile(msgbfile.c_str(), (char*)sfile.data(), sfile.capacity());
    if (n <= 0) {
        GLOG_ERR("readfile :%s error ret :%d", msgbfile.c_str(), n);
        return -1;
    }
    if (!msg.ParseFromArray(sfile.data(), n)) {
        GLOG_ERR("protobuf parse from array ereror ! buff size:%d", n);
        return -1;
    }
    return 0;
}

int 
protobuf_msg_from_json_file(google::protobuf::Message & msg, const std::string & jsonfile, std::string & error){
    string sfile;
    int sz = dcsutil::filesize(jsonfile);
    sfile.reserve(sz);
    int n = dcsutil::readfile(jsonfile.c_str(), (char*)sfile.data(), sfile.capacity());
    if (n <= 0){
        GLOG_ERR("readfile :%s error ret :%d", jsonfile.c_str(), n);
        return -1;
    }
    string sjson(sfile.data(), n);
    return protobuf_msg_from_json_string(msg, sjson, error);
}




NS_END()