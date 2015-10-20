#pragma once
#include "stdinc.h"

struct script_vm_t;
enum script_vm_enm_type {
	SCRIPT_VM_NONE = 0, //NO
	SCRIPT_VM_PYTHON = 1,
	SCRIPT_VM_LUA = 2,	//todo
	SCRIPT_VM_JS = 3,	//todo
	SCRIPT_VM_MAX, //script vm max
};

struct script_python_conf_t {
};

struct script_vm_config_t {
	script_vm_enm_type	type;
	const char *	path;
	union {
		script_python_conf_t python;
	} u;
	script_vm_config_t(){
		bzero(this, sizeof(*this));
	}
};

struct script_vm_python_export_t {
	std::string		module;
	struct export_entry_t {	//METH_VARARGS
		std::string		name;
		void*			func;	//PyCFunction
		std::string		desc;
	};
	std::vector<export_entry_t>		entries;
};

////////////////////////////////////////////////////////////////////////////
struct script_vm_import_t;

script_vm_t*	script_vm_create(const script_vm_config_t & conf);
script_vm_enm_type	script_vm_type(script_vm_t* vm);
void			script_vm_destroy(script_vm_t * vm);
int				script_vm_run_file(script_vm_t * vm, const char * file);
int				script_vm_run_string(script_vm_t * vm, const char * str);

//for python
void			script_vm_export(const script_vm_python_export_t & export_);


