#pragma  once


namespace google {
	namespace protobuf {
		class Message;
	}
}

struct mongoproxy_result_t {
	int status;
	int count; //for count
	const google::protobuf::Message * msg; //result msg
	const char * error; //error
};
enum mongoproxy_cmd_t {
	MONGO_CMD = 0,
	MONGO_INSERT = 1,
	MONGO_REMOVE = 2,
	MONGO_FIND = 3,
	MONGO_UPDATE = 4,
	MONGO_COUNT = 5,
};

typedef	void(*mongoproxy_cmd_cb_t)(mongoproxy_cmd_t cmd, void * ud, const mongoproxy_result_t &);
int		mongoproxy_init(const char * proxyaddr);
void	mongoproxy_set_cmd_cb(mongoproxy_cmd_cb_t cb, void * cb_ud);
void	mongoproxy_destroy();
int		mongoproxy_poll( int timeout_ms = 1);
int		mongoproxy_insert( const google::protobuf::Message & msg);
int		mongoproxy_remove( const google::protobuf::Message & msg);
int		mongoproxy_find( const google::protobuf::Message & msg);
int		mongoproxy_update( const google::protobuf::Message & msg);
int		mongoproxy_count( const google::protobuf::Message & msg);

