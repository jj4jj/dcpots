#include "xml_doc.h"
#include "logger.h"
#include "utility.hpp"
#include "3rd/rapidxml/rapidxml.hpp"
#include "3rd/rapidxml/rapidxml_print.hpp"


struct xml_node_t : public rapidxml::xml_node<>{};
struct xml_attribute_t : public  rapidxml::xml_attribute<>{};
struct xml_document_t : public rapidxml::xml_document<>{};

#define XML_PATH_NODE_SEP	('.')
#define XML_PATH_NODE_ARRAY_SEP		('#')
#define XML_PATH_NODE_ATTRIBUTE_SEP	(':')

#define MAX_XML_FILE_BUFF_SIZE	(1024*1024)

xml_doc_t::xml_doc_t(){
	doc = new xml_document_t;
}
xml_doc_t::~xml_doc_t(){
	if (doc){
		delete doc;
		doc = nullptr;
	}
}
int					xml_doc_t::parse_file(const char * file){
	if (parse_file_buffer.buffer == nullptr &&
		parse_file_buffer.create(MAX_XML_FILE_BUFF_SIZE)){
		LOGP("create file buffer error !");
		return -1;
	}
	size_t readn = dcsutil::readfile(file, parse_file_buffer.buffer, parse_file_buffer.max_size);
	if (readn <= 0){
		LOGP("read file :%s error ! ", file);
		return -2;
	}
	return loads(parse_file_buffer.buffer);
}
int					xml_doc_t::dump_file(const char * file, bool pretty ){
	string s;
	return dcsutil::writefile(file, dumps(s));
}
const	char *		xml_doc_t::pretty(std::string & str){
	std::ostringstream 	oss;
	std::cout << (*doc);
	str = oss.str();
	return str.c_str();
}
int					xml_doc_t::loads(char * buffer){
	try {
		doc->parse<0>(buffer);
	}
	catch (rapidxml::parse_error e){
		LOGP("parse buffer error info:%s ",
			e.what());
		return -1;
	}
	return 0;
}
const	char *		xml_doc_t::dumps(std::string & str){
	return pretty(str);
}
xml_attribute_t *		xml_doc_t::get_attr(const char * key, xml_node_t * node , const char * deafultvale ){
	if (node == nullptr){
		node = reinterpret_cast<xml_node_t*>(doc);
	}
	auto it = node->first_attribute();
	while (it){
		if (strcmp(it->name(), key) == 0){
			return reinterpret_cast<xml_attribute_t *>(it);
		}
		it = it->next_attribute();
	}
	if (deafultvale){
		xml_attribute_t * attr = reinterpret_cast<xml_attribute_t *>(doc->allocate_attribute(key, deafultvale));
		node->append_attribute(attr);
		return attr;
	}
	return nullptr;
}
void				xml_doc_t::add_cdata(const char * data, xml_node_t * before){
	if (!before){
		before = reinterpret_cast<xml_node_t*>(doc);
	}
	xml_node_t * node = reinterpret_cast<xml_node_t *>(doc->allocate_node(rapidxml::node_cdata, nullptr, data));
	if (!node){
		LOGP("alloc data node error !");
		return;
	}
	if (before->parent()){
		before->parent()->insert_node(before, node);
	}
	else {
		before->append_node(node);
	}
}
void				xml_doc_t::add_comment(const char * comments, xml_node_t * before){
	if (!before){
		before = reinterpret_cast<xml_node_t*>(doc);
	}
	xml_node_t * node = reinterpret_cast<xml_node_t *>(doc->allocate_node(rapidxml::node_comment, nullptr, comments));
	if (!node){
		LOGP("alloc comment node error !");
		return;
	}
	if (before->parent()){
		before->parent()->insert_node(before, node);
	}
	else {
		before->append_node(node);
	}
}

xml_node_t *		xml_doc_t::get_node(const char* key, xml_node_t * parent, int idx, const char * defaultvalue){
	if (!parent){
		parent = reinterpret_cast<xml_node_t*>(doc);
	}
	auto it = parent->first_node();
	int found = 0;
	while (it){
		if (strcmp(it->name(), key) == 0){
			if (found >= idx){
				return reinterpret_cast<xml_node_t *>(it);
			}
			else {
				found++;
			}
		}
		it = it->next_sibling();
	}
	if (defaultvalue){
		xml_node_t * new_node = nullptr;
		for (int i = found; i <= idx; ++i){
			new_node = reinterpret_cast<xml_node_t *>(doc->allocate_node(rapidxml::node_element, key, defaultvalue));  // Set node name to node_name
			if (new_node){
				parent->append_node(new_node);
			}
		}
		return new_node;
	}
	return nullptr;
}
int					xml_doc_t::path_node_array_length(const char* path){
	auto it = path_get_node(path);
	int num = 0;
	const char * name = nullptr;
	while (it){
		if (name == nullptr){
			++num;
			name = it->name();
		}
		else if (strcmp(it->name(), name) == 0){
			++num;
		}
		it = reinterpret_cast<xml_node_t *>(it->next_sibling());
	}
	return num;
}
//path : hello.ffx.fff#1:fff=v
xml_attribute_t *	 xml_doc_t::path_get_attr(const char* path, const char* defavalue ){
	const char * attr_key = strchr(path, XML_PATH_NODE_ATTRIBUTE_SEP);
	if (attr_key){
		attr_key++;
	}
	else {
		return nullptr;
	}
	xml_node_t * node = path_get_node(path, defavalue != nullptr);
	if (node){
		return get_attr(attr_key, node, defavalue);
	}
	else{
		return nullptr;
	}
}
xml_node_t *		xml_doc_t::path_get_node(const char* path, bool create_if_not_exist){
	std::string	spath = path;
	char * pbeg = (char*)spath.data();
	char * end = strchr(pbeg, XML_PATH_NODE_ATTRIBUTE_SEP);
	if (end){
		*end = 0;
	}
	char * pfind = strchr(pbeg, XML_PATH_NODE_SEP);
	xml_node_t * node = nullptr;
	do{
		if (pfind){
			*pfind = 0;
		}
		int idx = 0; //.xxff#1
		char * ps_array_idx = strchr(pbeg, XML_PATH_NODE_ARRAY_SEP);
		if (ps_array_idx){
			idx = std::atoi(ps_array_idx + 1);
			*ps_array_idx = 0;
		}
		node = get_node(pbeg, node, idx, create_if_not_exist ? "" : nullptr);
		if (!node){
			return nullptr;
		}
		if (pfind){
			pbeg = pfind + 1;
		}
		else {
			break;
		}
		pfind = strchr(pbeg, XML_PATH_NODE_SEP);
	} while (true);
	return node;
}
void				xml_doc_t::path_set(const char * path, const char * val){
	if (strchr(path, XML_PATH_NODE_ATTRIBUTE_SEP)){
		auto it = path_get_attr(path, val);
		if (it){
			auto str = doc->allocate_string(val);
			it->value(str);
		}
	}
	else{
		LOGP("path:%s is not valid format , not found attribute name !", path);
	}
}