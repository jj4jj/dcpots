
#include "utility/utility_mongo.h"
#include "base/cmdline_opt.h"
#include "dcnode/dcnode.h"
#include "3rd/pbjson/src/pbjson.hpp"


int main(int argc, char * argv[]){

	cmdline_opt_t	cmdline(argc, argv);
	cmdline.parse("daemon:n:D:daemon mode;"
		"log-path:r::log path;"
		"listen:r:l:listen a dcnode address;"
		"mongo-uri:r::mongo uri address;"
		"workers:o::the number of workers thread (default = 2* <numbers core>)");
	//////////////////////////////////////////////////////////////////////////////////
	const char * logpath = cmdline.getoptstr("log");
	const char * listen = cmdline.getoptstr("listen");
	bool daemon = cmdline.getoptstr("daemon") ? true : false;
	//////////////////////////////////////////////////////////////////////////////////
	if (dcsutil::lockpidfile("./mongoproxy.pid")){
		return -2;
	}
	////////////////////////////////////////////////
	dcsutil::mongo_client_config_t	mconf;
	mconf.mongo_uri = cmdline.getoptstr("mongo-uri");
	mconf.multi_thread = cmdline.getoptint("workers");
	dcsutil::mongo_client_t mongo;
	if (mongo.init(mconf)){
		return -4;
	}
	//daemon, log, listen, mongo-uri, mongo-worker
	/////////////////////////////////////////////
	if (daemon){
		dcsutil::daemonlize();
	}
	//dcnode
	////////////////////////////////////////////////
	dcnode_config_t dconf;
	dconf.name = "mongoproxy";
	dconf.addr.listen_addr = listen;
	dcnode_t * dcn = dcnode_create(dconf);
	if (!dcn){
		return -3;
	}

	struct dcnode_forward {
		static int foward_cmd(void * ud, const char * src, const msg_buffer_t & msg){
			//
			//forward to ->
			return 0;
		}
	};
	typedef int(*dcnode_dispatcher_t)(void * ud, const char * src, const msg_buffer_t & msg);
	dcnode_set_dispatcher(dcn, dcnode_forward::foward_cmd, 0);

	while (true){
		dcnode_update(dcn, 1000);
		mongo.poll();
	}
	return 0;
}


