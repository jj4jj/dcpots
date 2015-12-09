#pragma once
#include "base/stdinc.h"


namespace google {
    namespace protobuf {
        class Message;
        class Descriptor;
    }
}

struct	logger_t;
NS_BEGIN(dcsutil)
////////////////////////////////////////////////////////////////


void			protobuf_logger_init(logger_t * logger = nullptr);

google::protobuf::Message *
                protobuf_alloc_msg(const std::string & full_name);
void            protobuf_free_msg(google::protobuf::Message *);
const google::protobuf::Descriptor *
                protobuf_find_desc(const std::string & full_name);


////////////////////////////////////////////////////////////////
NS_END()