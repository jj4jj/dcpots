#pragma once
#include "../../base/stdinc.h"
#include "../../base/msg_buffer.hpp"

NS_BEGIN(dcsutil)

struct xml_node_t;
struct xml_attribute_t;
struct xml_document_t;

class xml_doc_t {
	xml_document_t *				doc;
	msg_buffer_t					parse_file_buffer;
public:
	xml_doc_t();
	~xml_doc_t();
public:
	int					parse_file(const char * file);
	int					dump_file(const char * file, bool pretty = false);
	const	char *		pretty(std::string & str);
	int					loads(char * buffer);
	const	char *		dumps(std::string & str);
	enum sax_event_type {
		BEGIN_NODE,
		END_NODE
	};
	typedef void(*sax_event_cb_t)(xml_node_t * node, int lv, void * ud, sax_event_type ev);
	void				sax(sax_event_cb_t cb, void * cb_ud, xml_node_t * node, int lv = 0);
	///////////////////////////////////////////////////////
	//get and add
	xml_attribute_t *	get_attr(const char * key, xml_node_t * node = nullptr, const char * deafultvale = nullptr);
	xml_node_t *		get_node(const char* key, xml_node_t * parent = nullptr, int idx = 0, const char * deafultvale = nullptr);
	void				add_comment(const char * comments, xml_node_t * before = nullptr);
	void				add_cdata(const char * data, xml_node_t * before = nullptr);
	int					path_node_array_length(const char* path);
	//path : hello.ffx.fff#1:fff=v
	xml_attribute_t *	path_get_attr(const char* path, const char* defavalue = nullptr);
	xml_node_t *		path_get_node(const char* path, bool create_if_not_exist = false);
	void				path_set(const char * path, const char * val, bool create_if_not_exist = false);

	////////////////////////////////////////////////////////////////////////////
	static	const	char *		node_name(xml_node_t * node);
    static  int                 node_type(xml_node_t * node);
	static	const	char *		node_value(xml_node_t * node);
	static	size_t				node_value_size(xml_node_t * node);

};

NS_END()
