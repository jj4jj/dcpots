#pragma once
struct msg_buffer_t {
	char *	buffer;
	int		valid_size;
	int		max_size;
	msg_buffer_t(const char * csp = nullptr, int sz = 0) :buffer((char*)csp), valid_size(sz), max_size(0){}
	int create(int max_sz){
		destroy();
		char * p = (char*)malloc(max_sz);
		if (!p) return -1;
		bzero(p, max_sz);
		buffer = p;
		max_size = max_sz;
		valid_size = 0;
		return 0;
	}
	void destroy()
	{
		if (buffer && max_size > 0) { 
			free(buffer); buffer = nullptr;
			valid_size = max_size = 0; 
		}
	}
};


/////////////////////////////////////////////////////////////////////////

template<class T>
struct msgproto_t : public T {
	const char * Debug() const {
		return T::ShortDebugString().c_str();
	}
	bool Pack(msg_buffer_t & msgbuf) const {
		int sz = msgbuf.max_size;
		bool ret = Pack(msgbuf.buffer, sz);
		if (!ret) return ret;
		msgbuf.valid_size = sz;
		return true;
	}
	bool Unpack(const msg_buffer_t & msgbuf) {
		return Unpack(msgbuf.buffer, msgbuf.valid_size);
	}
	bool Pack(char * buffer, int & sz) const {
		bool ret = T::SerializeToArray(buffer, sz);
		if (ret) sz = T::ByteSize();
		return ret;
	}
	bool Unpack(const char * buffer, int sz){
		return T::ParseFromArray(buffer, sz);
	}
	int	PackSize() const {
		return T::ByteSize();
	}
};