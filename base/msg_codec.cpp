#include "stdinc.h"
#include "logger.h"
#include "msg_buffer.hpp"
#include "msg_codec.h"
namespace dcs {
	int codec_msg_size_encode(int szb, msg_buffer_t & buff, const msg_buffer_t & msg) {
		assert(szb == 1 || szb == 2 || szb == 4);
		if (msg.valid_size > (1LL << (szb * 8)) || buff.max_size < szb + msg.valid_size) {
			GLOG_ERR("msg codec error size:%d  is too much for head size width buffer sz:%d!", msg.valid_size, buff.max_size);
			return -1;
		}
		buff.valid_size = szb + msg.valid_size;
		switch (szb) {
			case 1:
			*(uint8_t*)buff.buffer = buff.valid_size;
			break;
			case 2:
			*(uint16_t*)buff.buffer = htons(buff.valid_size);
			break;
			case 4:
			*(uint32_t*)buff.buffer = htonl(buff.valid_size);
			break;
		}
		return 0;
	}
	int codec_msg_size_decode(int szb, msg_buffer_t & msg, const msg_buffer_t & buff) {
		assert(szb == 1 || szb == 2 || szb == 4);
		if (buff.valid_size <= szb) {
			return 0; //need more
		}
		int msgsz = 0;
		switch (szb) {
		case 1:
			msgsz = *(uint8_t*)buff.buffer;
			break;
		case 2:
			msgsz = ntohs(*(uint16_t*)buff.buffer);
			break;
		case 4:
			msgsz = ntohl(*(uint32_t*)buff.buffer);
			break;
		}
		if (msgsz <= 0 || msgsz > (1LL << (szb * 8))) {
			return -1;//msg error format (size is invalid)
		}
		if (msgsz > buff.max_size) {
			return -2;//msg too much for recv buffer
		}
		if (msgsz > buff.valid_size) {
			return 0;//need more buffer
		}
		if (msgsz > msg.max_size) {
			return -3;//buffer not enough for decoding
		}
		/////////////////////////////////////////////////
		memcpy(msg.buffer, buff.buffer + szb, msgsz - szb);
		msg.valid_size = msgsz - szb;
		return msgsz;
	}

	int codec_msg_token_encode(const char * token, int len, msg_buffer_t & buff, const msg_buffer_t & msg) {
		assert(token && *token);
		if (msg.valid_size + len > buff.max_size) {
			return -1;
		}//check the token in msg ?
		memcpy(buff.buffer, msg.buffer, msg.valid_size);
		memcpy(buff.buffer + msg.valid_size, token, len);
		buff.valid_size = msg.valid_size + len;
		return 0;
	}
	int codec_msg_token_decode(const char * token, int len, msg_buffer_t & msg, const msg_buffer_t & buff) {
		assert(token && *token);
		if (buff.valid_size < len) {
			return 0;
		}
		//brute force , opt to kmp ? bm ?
		for (int i = 0;i <= buff.valid_size - len; ++i) {
			if (0 == memcmp(token, buff.buffer + i, len)) {
				if (i > 0) {
					if (i > msg.max_size) {
						return -1;//msg buff too small
					}
					memcpy(msg.buffer, buff.buffer, i);
					msg.valid_size = i;
					return i+len;
				}
				else {
					msg.valid_size = 0;
					return len;
				}
			}
		}
		//not found => need more
		return 0;
	}
}