#include "dagent/dagent.h"

int main()
{
	dagent_config_t	conf;

	//an agent
	auto & ncf = conf.node_conf;
	ncf.addr.listen_addr = "127.0.0.1:8888";
	ncf.heart_beat_gap = 60;
	ncf.max_channel_buff_size = 1024576;
	ncf.max_register_children = 10;
	ncf.name = "test1";
	ncf.addr.msgq_key = "/tmp/dagent";

	int ret = dagent_init(conf);
	if (ret)
	{
		puts("error init!");
		puts(dagent_errmsg());
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
