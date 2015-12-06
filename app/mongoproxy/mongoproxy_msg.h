#pragma once

#include "proto/mongo.pb.h"
#include "base/msg_proto.hpp"
typedef msgproto_t<dcorm::MongoORM>	mongo_msg_t;


#define MAX_MONGOPROXY_MSG_SIZE	(1024*1024*4)
