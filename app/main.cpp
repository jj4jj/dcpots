#include "dagent/dagent.h"

int main()
{
	dagent_config_t	conf;
	int ret = dagent_init(conf);
	if (ret)
	{
		puts("error init!");
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
