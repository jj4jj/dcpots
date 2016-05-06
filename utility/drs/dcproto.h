#pragma once
#include "../../base/stdinc.h"

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
void			protobuf_msg_sax(const string & name, const google::protobuf::Message & msg, sax_event_cb_t fn, void *ud, int level = 0, bool default_init = true);

int				protobuf_msg_to_xml_file(const google::protobuf::Message & msg, const std::string & xmlfile);
int				protobuf_msg_from_xml_file(google::protobuf::Message & msg, const std::string & xmlfile, std::string & error);
int				protobuf_msg_to_xml_string(const google::protobuf::Message & msg, std::string & sxml);
int				protobuf_msg_from_xml_string(google::protobuf::Message & msg, const std::string & sxml, std::string & error);


int             protobuf_msg_to_json_file(const google::protobuf::Message & msg, const std::string & jsonfile);
int             protobuf_msg_from_json_file(google::protobuf::Message & msg, const std::string & jsonfile, std::string & error);
int             protobuf_msg_to_json_string(const google::protobuf::Message & msg, std::string & json);
int             protobuf_msg_from_json_string(google::protobuf::Message & msg, const std::string & json, std::string & error);


std::string		protobuf_msg_field_get_value(const google::protobuf::Message & msg, const string & name, int idx);
int				protobuf_msg_field_set_value(google::protobuf::Message & msg, const string & name, int idx, const string & value, string & error);

////////////////////////////////////////////////////////////////
NS_END()