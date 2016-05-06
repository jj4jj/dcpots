#pragma once
#include "3rd/rapidjson/document.h"
#include "3rd/rapidjson/filereadstream.h"
#include "3rd/rapidjson/filewritestream.h"
#include "3rd/rapidjson/error/en.h"
#include "3rd/rapidjson/stringbuffer.h"
#include "../../3rd/rapidjson/prettywriter.h"
#include "../../3rd/rapidjson/reader.h"
#include "../../3rd/rapidjson/pointer.h"	//getter,setter by path(pointer)

#include "../../base/stdinc.h"
#include "../../base/msg_buffer.hpp"
#include "../../base/logger.h"

typedef	typename rapidjson::Value			        json_obj_t;
typedef typename rapidjson::Value::MemberIterator	json_obj_kv_itr_t;
typedef typename rapidjson::Value::ValueIterator	json_obj_list_itr_t;
NS_BEGIN(dcsutil)
class json_doc_t : public rapidjson::Document {
	#define MAX_CONF_BUFF_SIZE		(1024*1024)
	msg_buffer_t					parse_file_buffer;
public:
	json_doc_t(){
		SetObject();//root default is a object
	}
public:
	int					parse_file(const char * file){
		FILE * fp = fopen(file, "r");
		if (!fp){
			GLOG_TRA("open file :%s error ! ", file);
			return -1;
		}
		if (parse_file_buffer.buffer == nullptr && parse_file_buffer.create(MAX_CONF_BUFF_SIZE)){
			GLOG_TRA("create config buffer error !");
			return -2;
		}
		rapidjson::FileReadStream is(fp, parse_file_buffer.buffer, parse_file_buffer.max_size);
		if (ParseStream(is).HasParseError()){
			rapidjson::ParseErrorCode e = rapidjson::Document::GetParseError();
			size_t o = GetErrorOffset();
			GLOG_TRA("parse json file :%s error info:%s offset:%zu near:%32s", file,
				rapidjson::GetParseError_En(e), o, parse_file_buffer.buffer + o);
			return -1;
		}
		return 0;
	}
	int					dump_file(const char * file , bool pretty = false){
		rapidjson::StringBuffer sb;
		if (pretty){
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
			Accept(writer);    // Accept() traverses the DOM and generates Handler events.
		}
		else{
			rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
			Accept(writer);    // Accept() traverses the DOM and generates Handler events.
		}
		return dcsutil::writefile(file, sb.GetString(), sb.GetSize());
	}
	const	char *		pretty(std::string & str){
		str = "";
		rapidjson::StringBuffer sb;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
		Accept(writer);
		str = sb.GetString();
		return str.c_str();
	}
	int					loads(const char * buffer){
		rapidjson::StringStream	ss(buffer);
		if (ParseStream(ss).HasParseError()){
			rapidjson::ParseErrorCode e = GetParseError();
			size_t o = GetErrorOffset();
			GLOG_TRA("parse buffer error info:%s offset:%zu near:%32s",
				rapidjson::GetParseError_En(e), o, buffer + o);
			return -1;
		}
		return 0;
	}
	const	char *		dumps(std::string & str){
		str = "";
		rapidjson::StringBuffer sb;
		rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
		Accept(writer);
		str = sb.GetString();
		return str.c_str();
	}

	//get("/tmp/1")
	template <typename CharType, size_t N>
	typename rapidjson::Document::ValueType * get(const CharType(&source)[N]) {
		return rapidjson::GenericPointer<typename rapidjson::Document::ValueType>(source, N - 1).Get(*this);
	}

	//get("/tmp/1","v")
	template <typename CharType, size_t N, typename T2>
	typename rapidjson::Document::ValueType * get(const CharType(&source)[N], T2 defaultValue) {
		return &(rapidjson::GenericPointer<typename rapidjson::Document::ValueType>(source, N - 1).GetWithDefault(*this, defaultValue, GetAllocator()));
	}

	//set("/tmp/1","v")
	template <typename CharType, size_t N, typename T2>
	typename rapidjson::Document::ValueType& set(const CharType(&source)[N], T2 value) {
		return rapidjson::GenericPointer<typename rapidjson::Document::ValueType>(source, N - 1).Set(*this, value);
	}

};

NS_END()