#include "base/logger.h"
#include "google/protobuf/message.h"
#include "util_proto.h"

NS_BEGIN(dcsutil)

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
	GLOG(loglv, "protobuf log info (lv:%d filename:%s:%d msg:%s)",
		level, filename, line, message.c_str());
}
void
protobuf_logger_init(logger_t * logger){
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


NS_END()