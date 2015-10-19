#include "script_vm.h"
#include "Python.h"


struct python_vm_t {
	bool	myown;
};

struct script_vm_t{
	script_vm_type	type;
	python_vm_t	 *	python;
};

static python_vm_t * _create_vm_python(const script_python_conf_t & conf){

	python_vm_t * vm = new python_vm_t();
	vm->myown = true;
	if (Py_IsInitialized()){
		vm->myown = false;
	}
	else {
		Py_InitializeEx(0);
	}
	return vm;
}
static void _destroy_vm_python(python_vm_t * vm){
	if (vm){
		if (vm->myown){
			Py_Finalize();
		}
		delete vm;
	}
}

script_vm_t*	script_vm_create(const script_vm_config_t & conf){
	auto vm = new script_vm_t();
	switch (conf.type){
	case SCRIPT_VM_PYTHON:
		vm->python = _create_vm_python(conf.u.python);
		break;
	default:
		return nullptr;
	}
}
void			script_vm_destroy(script_vm_t * vm){
	if (vm){
		switch (vm->type){
		case SCRIPT_VM_PYTHON:
			_destroy_vm_python(vm->python);
			vm->python = nullptr;
			break;
		default:break;
		}
		delete vm;
	}
}
int				script_vm_run(script_vm_t * vm, const char * file);


//for python
void					script_vm_export(script_vm_t * vm, const script_vm_python_export_t & export_){
	return;
}
script_vm_import_t *	script_vm_import(script_vm_t * vm, const char * filepath){
	return nullptr;
}
int						script_vm_run(script_vm_t * vm, script_vm_import_t *, const char * func){
	return 0;
}