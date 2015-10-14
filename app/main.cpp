#include "dagent/dagent.h"

int main(int argc, char* argv[])
{
	dagent_config_t	conf;

	//an agent
	auto & ncf = conf.node_conf;
	conf.max_msg_size = 1024 * 1024;
	ncf.addr.listen_addr = "127.0.0.1:8888";
	ncf.heart_beat_gap = 60;
	ncf.max_channel_buff_size = 1024576;
	ncf.max_register_children = 10;
	ncf.name = "test1";
	ncf.addr.msgq_key = "./dcagent";

	if (argc == 2)
	{
		//client
		ncf.addr.listen_addr = "";
		ncf.name = "test2";
	}

	int ret = dagent_init(conf);
	if (ret)
	{
		puts("error init!");
		puts(DAGENT_ERRMSG(" < "));
		perror("system error ");
		return -1;
	}
	while (true)
	{
		dagent_update();
		usleep(10000);//10ms
	}
	dagent_destroy();
	return 0;
}
