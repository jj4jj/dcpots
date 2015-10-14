#pragma once
#include "utility.hpp"

//nocopy
struct msg_buffer_t : public noncopyable {
	char *	buffer;
	int		max_size;
	int		txd_size;
	int		current_msg_size;
	msg_buffer_t() :buffer(nullptr), max_size(0), txd_size(0), current_msg_size(0)
	{
	}
	int create(int max_sz)
	{
		destroy();
		char * p = (char*)malloc(max_size);
		if (!p) return -1;
		max_size = max_sz;
		current_msg_size = txd_size = 0;
		buffer = p;
		return 0;
	}
	void destroy()
	{
		if (buffer) { free(buffer); buffer = nullptr; max_size = 0; }
	}
	~msg_buffer_t()
	{
		destroy();
	}
};





template<class T>
struct msgproto_t : public T {
	bool pack(msg_buffer_t & msgbuf)
	{
		int sz = msgbuf.max_size;
		bool ret = pack(msgbuf.buffer, sz);
		if (!ret) return ret;
		msgbuf.txd_size = sz;
		return true;
	}
	bool unpack(const msg_buffer_t & msgbuf)
	{
		return unpack(msgbuf.buffer, msgbuf.txd_size);
	}
	bool pack(char * buffer, int & sz) const
	{
		bool ret = T::SerializeToArray(buffer, sz);
		if (ret) sz = T::ByteSize();
		return ret;
	}
	bool unpack(const char * buffer, int sz)
	{
		return T::ParseFromArray(buffer, sz);
	}
};