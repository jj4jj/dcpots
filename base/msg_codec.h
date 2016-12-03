#pragma once
struct msg_buffer_t;
namespace dcs {
	//return >0: success, return the costed buffer msg length , = 0, need more, < 0 error.
	int codec_msg_size_encode(int szb, msg_buffer_t & buff, const msg_buffer_t & msg);
	int codec_msg_size_decode(int szb, msg_buffer_t & msg, const msg_buffer_t & buff);

	int codec_msg_token_encode(const char * token, int len, msg_buffer_t & buff, const msg_buffer_t & msg);
	int codec_msg_token_decode(const char * token, int len, msg_buffer_t & msg, const msg_buffer_t & buff);

}
