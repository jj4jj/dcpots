
#include "dcnode/dcnode.h"
#include "base/logger.h"
#include "proto/dagent.pb.h"
#include "base/msg_proto.hpp"
#include "utility/script_vm.h"
#include "dagent.h"
extern "C" {
#include "python2.7/Python.h"
}
struct dagent_plugin_t {
	std::string				file;
};

typedef msgproto_t<dagent::MsgDAgent>	dagent_msg_t;


struct dagent_t
{
	dagent_config_t						conf;
	dcnode_t *							node;
	unordered_map<int, dagent_cb_t>		cbs;
	msg_buffer_t						send_msgbuffer;
	std::vector<dagent_plugin_t>		plugins;
	script_vm_t	*						vms[script_vm_enm_type::SCRIPT_VM_MAX];

	unordered_map<int, PyObject*>		py_cbs;
	script_vm_python_export_t			py_export;
	dagent_t(){
		bzero(vms,sizeof(vms));
		node = nullptr;
	}
};


static dagent_t AGENT;
////////////////////////////////////////////////////////////////////////////////////////////
bool	_cb_exists(int type){
	auto it = AGENT.cbs.find(type);
	if (it != AGENT.cbs.end()){
		return true;
	}
	auto pyit = AGENT.py_cbs.find(type);
	if (pyit != AGENT.py_cbs.end()){
		return true;
	}
	return false;
}


static int _py_cb_push(int type, PyObject * cb){
	if (_cb_exists(type)){
		GLOG_TRA("repeat cb push type:%d ", type);
		return -1;
	}
	Py_XINCREF(cb);
	AGENT.py_cbs[type] = cb;
	return 0;
}
static void _py_cb_pop(int type){
	auto it = AGENT.py_cbs.find(type);
	if (it != AGENT.py_cbs.end()){
		Py_XDECREF(it->second);
		AGENT.py_cbs.erase(it);
	}
}
//int push_cb(type,cb)
static PyObject * _py_push_cb(PyObject *, PyObject * type_cb){
	int ctype = 0;
	PyObject *cb = nullptr;
	if (PyArg_ParseTuple(type_cb, "iO:set_callback", &ctype, &cb)){
		if (!PyCallable_Check(cb)) {
			PyErr_SetString(PyExc_TypeError, "parameter 2 must be callable");
			return NULL;
		}
		//save the cb to the map > incea 
		int ret = _py_cb_push(ctype, cb);
		if (ret){
			PyErr_SetString(PyExc_TypeError, "py cb push error !");
			return NULL;
		}
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyErr_SetString(PyExc_TypeError, "parameter parse error");
	return NULL;
}
//pop cb
static PyObject * _py_pop_cb(PyObject *, PyObject * type){
	int ctype = 0;
	if (PyArg_ParseTuple(type, "i", &ctype)){
		//save the cb to the map > incea 
		GLOG_TRA("pop python cb :%d",ctype);
		_py_cb_pop(ctype);
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyErr_SetString(PyExc_TypeError, "parameter parse error");
	return NULL;
}
static PyObject * _py_send(PyObject *, PyObject * dst_type_msg_size){
	int type = 0 , size = 0;
	const char * dst = nullptr , * msg = nullptr;
	if (PyArg_ParseTuple(dst_type_msg_size, "sit#", &dst, &type, &msg, &size)){
		int ret = dagent_send(dst, type, msg_buffer_t(msg, size));
		if (ret){
			GLOG_TRA("dagent send error :%d", ret);
			PyErr_SetString(PyExc_TypeError, "dagent send error");
			return NULL;
		}
		/* Boilerplate to return "None" */
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyErr_SetString(PyExc_TypeError, "parameter parse error");
	return NULL;
}
static PyObject * _py_update(PyObject *, PyObject * timeoutms){
	int timeout_ms = 10;
	if (PyArg_ParseTuple(timeoutms, "i", &timeout_ms)){
		dagent_update(timeout_ms);
		/* Boilerplate to return "None" */
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyErr_SetString(PyExc_TypeError, "parameter parse error");
	return NULL;
}
static PyObject * _py_ready(PyObject *, PyObject *){
	int ret = dagent_ready();
	PyObject * rest = Py_BuildValue("i", ret);
	Py_INCREF(rest);
	return rest;
}

static PyObject * _py_ext_init(PyObject *, PyObject * args, PyObject *keywds){
	dagent_config_t dcf;
	const char * localkey = "";
	const char * parent = "";
	const char * listen = "";
	const char * name = "pynoname";
	int routermode = 0;
	int heartbeat = dcf.heartbeat;
	static const char *kwlist[] = {  "name", "routermode","localkey", "parent", "listen","heartbeat", NULL };
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "si|sssi", (char**)kwlist,
		&name, &routermode, &localkey, &parent, &listen, &heartbeat)){
		PyErr_SetString(PyExc_TypeError, "param parse error !");
		return NULL;
	}
	dcf.name = name;
	dcf.routermode = routermode != 0;
	dcf.localkey = localkey;
	dcf.parent = parent;
	dcf.listen = listen;
	dcf.heartbeat = heartbeat;
	dcf.extmode = true;
	int ret = dagent_init(dcf);
	if (ret){
		GLOG_TRA("dagent init error :%d", ret);
		PyErr_SetString(PyExc_TypeError, "dagent init error !");
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * _py_ext_destroy(PyObject *, PyObject *){
	dagent_destroy();
	Py_INCREF(Py_None);
	return Py_None;
}

static  int _init_py_vm(script_vm_t * vm){
	if (!vm){
		return -1;
	}	
	dagent_export_python(false);
	if (script_vm_run_file(vm, "init")){
		GLOG_TRA("init plugin start file error !");
		return -1;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int _dispatcher(void * ud, const char * src, const msg_buffer_t & msg){
	assert(ud == &AGENT);
	dagent_msg_t	dm;
	if (!dm.Unpack(msg.buffer, msg.valid_size)){
		GLOG_ERR("msg unpack error !");
        return -1;
	}
	auto it = AGENT.cbs.find(dm.type());
	if (it != AGENT.cbs.end()){
		return it->second(msg_buffer_t(dm.msg_data().data(),dm.msg_data().length()), src);
	}
	auto pyit = AGENT.py_cbs.find(dm.type());
	if(pyit != AGENT.py_cbs.end()){
		/* Time to call the callback */
		PyObject *arglist = Py_BuildValue("(ss#)", src, dm.msg_data().data(), dm.msg_data().length());
		PyObject *result = PyObject_CallObject(pyit->second, arglist);
		Py_XDECREF(arglist);
		if (result == NULL){
			PyErr_Print();
            GLOG_ERR("call python cb error !");
            return -1;
		}
		else{
			int ret = 0;
			PyArg_Parse(result,"i",&ret);
			Py_XDECREF(result);
			return ret;
		}
		GLOG_ERR("call python cb error result !");
		return -2;
	}
	//not found
	GLOG_ERR("not found handler !");
    return -3;
}

void dagent_export_python(bool for_ext){
	script_vm_python_export_t & vmexport = AGENT.py_export;
	if (!vmexport.entries.empty()){
		GLOG_ERR("repeat export python ...");
		return;
	}

	vmexport.module = "dagent";
	script_vm_python_export_t::export_entry_t	pyee;
	pyee.func = (void*)(_py_push_cb);
	pyee.name = "push_cb";
	pyee.desc = "void push_cb(int type, int(const char* src, const char* msg, int sz)) add a cb with type...";
	vmexport.entries.push_back(pyee);

	pyee.func = (void*)(_py_pop_cb);
	pyee.name = "pop_cb";
	pyee.desc = "void pop_cb(int type) remove the pushed cb with type...";
	vmexport.entries.push_back(pyee);

	pyee.func = (void*)(_py_send);
	pyee.name = "send";
	pyee.desc = "void send(const char* dst, int type, const char* msg, int msg_size) send msg to dst with type ...";
	vmexport.entries.push_back(pyee);

	pyee.func = (void*)(_py_update);
	pyee.name = "update";
	pyee.desc = "void update(int timeout_ms) update agent state ...";
	vmexport.entries.push_back(pyee);

	pyee.func = (void*)(_py_ready);
	pyee.name = "ready";
	pyee.desc = "int ready() query agent state ... [-1:abort,0:not ready:1:ready]";
	vmexport.entries.push_back(pyee);

	if (for_ext){
		pyee.func = (void*)(_py_ext_init);
		pyee.name = "init";
		pyee.desc = "void init() init :must be called by indenpendent extension";
		vmexport.entries.push_back(pyee);

		pyee.func = (void*)(_py_ext_destroy);
		pyee.name = "destroy";
		pyee.desc = "void destroy() destory :must be called by indenpendent extension";
		vmexport.entries.push_back(pyee);
	}

	script_vm_export(vmexport);
}
int     dagent_init(const dagent_config_t & conf){
    if (AGENT.node)
    {
        GLOG_ERR("node has been inited !");
        return -1;
    }
    if (AGENT.send_msgbuffer.create(conf.max_msg_size)){
        GLOG_ERR("create send msgbuffer error !");
        return -2;
    }
	
	dcnode_config_t	dcf;
	dcf.name = conf.name;
	dcf.parent_heart_beat_gap = conf.heartbeat;
	dcf.max_children_heart_beat_expired = 3 * conf.heartbeat;
	if (!conf.parent.empty()){
		dcf.addr.tcp_parent_addr = conf.parent;
	}
	if (!conf.listen.empty()){
		dcf.addr.tcp_listen_addr = conf.listen;
	}
	if (!conf.localkey.empty()){
		dcf.addr.msgq_sharekey = conf.localkey;
	}
	dcf.addr.msgq_push = true;
	if (conf.routermode){
		dcf.addr.msgq_push = false;
	}
	dcnode_t * node = dcnode_create(dcf);
	if (!node){
		GLOG_ERR("create dcnode error !");
        return -3;
	}
	dcnode_set_dispatcher(node, _dispatcher, &AGENT);
	AGENT.conf = conf;
	AGENT.node = node;
	for (int i = script_vm_enm_type::SCRIPT_VM_NONE; !conf.extmode && i < script_vm_enm_type::SCRIPT_VM_MAX; ++i){
		script_vm_config_t	vmc;
		vmc.type = (script_vm_enm_type)i;		
		vmc.path = conf.plugin_path.c_str();
		AGENT.vms[i] = script_vm_create(vmc);
		if (i == script_vm_enm_type::SCRIPT_VM_PYTHON){
			_init_py_vm(AGENT.vms[i]);
		}
	}
	return 0;
}

void    dagent_destroy(){
	if (AGENT.node){
		dcnode_destroy(AGENT.node);
		AGENT.node = nullptr;
	}
	AGENT.cbs.clear();
	AGENT.send_msgbuffer.destroy();
	for (int i = 0; i < script_vm_enm_type::SCRIPT_VM_MAX; ++i){
		if (AGENT.vms[i]){
			script_vm_destroy(AGENT.vms[i]);
			AGENT.vms[i] = nullptr;
		}
	}
}
void    dagent_update(int timeout_ms){
	dcnode_update(AGENT.node, timeout_ms*1000);	
}
int		dagent_ready() { //ready to write 
	return dcnode_ready(AGENT.node);
}

int     dagent_send(const char * dst, int type, const msg_buffer_t & msg){
	dagent_msg_t	dm;
	dm.set_type(type);
	dm.set_msg_data(msg.buffer,msg.valid_size);
	if (!dm.Pack(AGENT.send_msgbuffer)){
        GLOG_ERR("pack agent msg error !");
		return -1;
	}
	return dcnode_send(AGENT.node, dst, AGENT.send_msgbuffer.buffer, AGENT.send_msgbuffer.valid_size);
}
int     dagent_cb_push(int type, dagent_cb_t cb){
	if (_cb_exists(type)){
		GLOG_ERR("repeat cb push type:%d ", type);
		return -1;
	}
	AGENT.cbs[type] = cb;
	return 0;
}
int     dagent_cb_pop(int type){
	auto it = AGENT.cbs.find(type);
	if (it != AGENT.cbs.end()){
		AGENT.cbs.erase(it);
		return 0;
	}
    GLOG_ERR("pop a cb but not found the type:%d", type);
	return -1;
}
int     dagent_load_plugin(const char * file){
	script_vm_t * vm = nullptr;
	if (strstr(file, ".py") + 3 == file + strlen(file)){
		//python
		vm = AGENT.vms[script_vm_enm_type::SCRIPT_VM_PYTHON];
	}
	else if (strstr(file, ".lua") + 4 == file + strlen(file)){
		//lua
		vm = AGENT.vms[script_vm_enm_type::SCRIPT_VM_LUA];
	}
	else if (strstr(file, ".js") + 3 == file + strlen(file)){
		vm = AGENT.vms[script_vm_enm_type::SCRIPT_VM_JS];
	}
	if(!vm){
		GLOG_TRA("not found plugin vm file:%s or plugin vm not load", file);
		return -1;
	}
	return script_vm_run_file(vm, file);
}








