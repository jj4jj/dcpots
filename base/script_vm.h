#pragma once
#include "stdinc.h"

struct script_vm_t;
enum script_vm_type {
	SCRIPT_VM_NONE = 0, //NO
	SCRIPT_VM_PYTHON = 1,
	SCRIPT_VM_LUA = 2,	//todo
	SCRIPT_VM_JS = 3,	//todo
	SCRIPT_VM_MAX, //script vm max
};
struct script_python_conf_t {
	const char * path; //dummy
};

struct script_vm_config_t {
	script_vm_type	type;
	union {
		script_python_conf_t python;
	} u;
	script_vm_config_t(){
		bzero(this, sizeof(*this));
	}
};

struct PyObject;

struct script_vm_python_export_t {
	std::string		module;
	struct export_entry_t {	//METH_VARARGS
		std::string		name;
		std::function<PyObject*(PyObject*, PyObject*)>	func;
	};
	std::vector<export_entry_t>		entries;
};

////////////////////////////////////////////////////////////////////////////
struct script_vm_import_t;

script_vm_t*	script_vm_create(const script_vm_config_t & conf);
void			script_vm_destroy(script_vm_t * vm);
int				script_vm_run(script_vm_t * vm, const char * file);


//for python
void					script_vm_export(script_vm_t * vm, const script_vm_python_export_t & export_);
script_vm_import_t *	script_vm_import(script_vm_t * vm, const char * filepath);
int						script_vm_run(script_vm_t * vm, script_vm_import_t *, const char * func);