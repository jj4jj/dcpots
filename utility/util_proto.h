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


enum protobuf_sax_event_type {
	BEGIN_MSG,
	END_MSG,
	BEGIN_ARRAY,
	END_ARRAY,
	VISIT_VALUE
};

void			protobuf_logger_init(logger_t * logger = nullptr);

google::protobuf::Message *
                protobuf_alloc_msg(const std::string & full_name);
void            protobuf_free_msg(google::protobuf::Message *);
const google::protobuf::Descriptor *
                protobuf_find_desc(const std::string & full_name);

typedef void(*sax_event_cb_t)(const string & name, const google::protobuf::Message & msg, int idx, int level, void *ud, protobuf_sax_event_type evt);
void			protobuf_msg_sax(const string & name, const google::protobuf::Message & msg, sax_event_cb_t fn, void *ud, int level = 0);

int				protobuf_saveto_xml(const google::protobuf::Message & msg, const std::string & xmlfile);
int				protobuf_readfrom_xml(google::protobuf::Message & msg, const std::string & xmlfile, std::string & error);

std::string		protobuf_msg_field_get_value(const google::protobuf::Message & msg, const string & name, int idx);
int				protobuf_msg_field_set_value(google::protobuf::Message & msg, const string & name, int idx, const string & value, string & error);

////////////////////////////////////////////////////////////////
NS_END()