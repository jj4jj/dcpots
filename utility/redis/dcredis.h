#pragma  once
#include <string>

struct redisReply;
namespace dcsutil{
struct RedisAsyncAgentImpl;
using std::string;
struct RedisAsyncAgent {
	int	init(const string & addrs);
	int update();
	int	destroy();
	typedef std::function<void(int error, redisReply* r)> CommandCallBack;
    bool ready();
	int command(CommandCallBack cb, const char * fmt, ...);
    int psubscribe(CommandCallBack cb, const char * channel);
    int subscribe(CommandCallBack cb, const char * channel);
public:
	RedisAsyncAgent();
	~RedisAsyncAgent();
private:
	RedisAsyncAgentImpl * impl{ nullptr };
};
}


