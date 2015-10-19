
#include "dcnode/dcnode.h"
#include "base/logger.h"
#include "proto/dagent.pb.h"
#include "base/msg_proto.hpp"
#include "base/script_vm.h"
#include "dagent.h"

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
	script_vm_t	*						vms[script_vm_type::SCRIPT_VM_MAX];
	dagent_t(){
		bzero(vms,sizeof(vms));
		node = nullptr;
	}
};


static dagent_t AGENT;
static inline int	_error(const char * msg, int err = -1, logger_t * trace = nullptr){
	//LOG_MSG(ERROR_LVL_DEBUG, AGENT.error_msg, trace, err, msg);
	LOGP("%s",msg);
	return err;
}
static int _dispatcher(void * ud, const char * src, const msg_buffer_t & msg){
	assert(ud == &AGENT);
	dagent_msg_t	dm;
	if (!dm.Unpack(msg.buffer, msg.valid_size))
	{
		//error pack
		return _error("msg unpack error !");
	}
	auto it = AGENT.cbs.find(dm.type());
	if (it != AGENT.cbs.end())
	{
		return it->second(msg_buffer_t(dm.msg_data().data(),dm.msg_data().length()), src);
	}
	//not found
	return _error("not found handler !");
}
int     dagent_init(const dagent_config_t & conf){
	if (AGENT.node)
	{
		return _error("node has been inited !");
	}
	if (AGENT.send_msgbuffer.create(conf.max_msg_size))
		return _error("create send msgbuffer error !");
	
	dcnode_config_t	dcf;
	dcf.name = conf.name;
	dcf.heart_beat_gap = conf.hearbeat;
	dcf.max_live_heart_beat_gap = 3 * conf.hearbeat;
	if (!conf.parent.empty()){
		dcf.addr.parent_addr = conf.parent;
	}
	if (!conf.listen.empty()){
		dcf.addr.listen_addr = conf.listen;
	}
	if (!conf.localkey.empty()){
		dcf.addr.msgq_path = conf.localkey;
	}
	dcf.addr.msgq_push = true;
	if (conf.routermode){
		dcf.addr.msgq_push = false;
	}
	dcnode_t * node = dcnode_create(dcf);
	if (!node){
		return _error("create dcnode error !");
	}
	dcnode_set_dispatcher(node, _dispatcher, &AGENT);
	AGENT.conf = conf;
	AGENT.node = node;
	for (int i = script_vm_type::SCRIPT_VM_JS; i < script_vm_type::SCRIPT_VM_MAX; ++i){
		script_vm_config_t	vmc;
		vmc.type = (script_vm_type)i;
		AGENT.vms[i] = script_vm_create(vmc);
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
	for (int i = 0; i < script_vm_type::SCRIPT_VM_MAX; ++i){
		if (AGENT.vms[i]){
			script_vm_destroy(AGENT.vms[i]);
			AGENT.vms[i] = nullptr;
		}
	}
}
void    dagent_update(int timeout_ms){
	dcnode_update(AGENT.node, timeout_ms*1000);	
}
bool	dagent_ready() { //ready to write 
	return dcnode_ready(AGENT.node);
}

int     dagent_send(const char * dst, int type, const msg_buffer_t & msg){
	dagent_msg_t	dm;
	dm.set_type(type);
	dm.set_msg_data(msg.buffer,msg.valid_size);
	if (!dm.Pack(AGENT.send_msgbuffer))
	{
		//serialize error
		return -1;
	}
	return dcnode_send(AGENT.node, dst, AGENT.send_msgbuffer.buffer, AGENT.send_msgbuffer.valid_size);
}
int     dagent_cb_push(int type, dagent_cb_t cb){
	auto it = AGENT.cbs.find(type);
	if (it != AGENT.cbs.end())
	{
		//repeat
		return -1;
	}
	AGENT.cbs[type] = cb;
	return 0;
}
int     dagent_cb_pop(int type){
	auto it = AGENT.cbs.find(type);
	if (it != AGENT.cbs.end())
	{
		AGENT.cbs.erase(it);
		return 0;
	}
	//not found
	return -1;
}
int     dagent_load_plugin(const char * file){
	script_vm_t * vm = nullptr;
	if (strstr(file, ".py") + 3 == file + strlen(file)){
		//python
		vm = AGENT.vms[script_vm_type::SCRIPT_VM_PYTHON];
	}
	else if (strstr(file, ".lua") + 4 == file + strlen(file)){
		//lua
		vm = AGENT.vms[script_vm_type::SCRIPT_VM_LUA];
	}
	else if (strstr(file, ".js") + 3 == file + strlen(file)){
		vm = AGENT.vms[script_vm_type::SCRIPT_VM_JS];
	}

	if(!vm){
		LOGP("not found plugin vm file:%s or plugin vm not load", file);
		return -1;
	}
	return script_vm_run(vm, file);
}
int     dagent_unload_plugin(const char * file){
	return -1;
}








