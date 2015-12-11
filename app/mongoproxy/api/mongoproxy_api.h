#pragma  once
#include "base/stdinc.h"

namespace google {
	namespace protobuf {
		class Message;
	}
}

struct mongoproxy_result_t {
    int status; //for call status
    int ok; //.ok for status 1:true;0:false
    int n; //n
    int modified;
    const  char * cb_data;
    size_t cb_size;
    //id, msg
    typedef std::pair<string, google::protobuf::Message * >  mongo_record_t;
    std::vector<mongo_record_t> results; //result msg
    const char * error; //error
    mongoproxy_result_t(){
        status = 0;
        ok = 0;
        n = 0;
        modified = 0;
        error = nullptr;
        cb_data = nullptr;
        cb_size = 0;
    }
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
int		mongoproxy_insert(const google::protobuf::Message & msg, const char * cb_data = nullptr, int cb_size = 0);
int		mongoproxy_remove(const google::protobuf::Message & msg, int limit = 0, const char * cb_data = nullptr, int cb_size = 0);
int		mongoproxy_find(const google::protobuf::Message & msg, const char * fields = nullptr,
    int skip = 0, int limit = 0, const char * sort = nullptr, const char * cb_data = nullptr, int cb_size = 0);
int		mongoproxy_update(const google::protobuf::Message & msg, const std::string & fields, const char * cb_data = nullptr, int cb_size = 0);
int		mongoproxy_count(const google::protobuf::Message & msg, const char * cb_data = nullptr, int cb_size = 0);

