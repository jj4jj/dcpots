#pragma once

struct msg_buffer_t {
	char *	buffer;
	int		valid_size;
	int		max_size;
	msg_buffer_t(const char * csp = nullptr, int sz = 0) :buffer((char*)csp), valid_size(sz), max_size(0){
		if (csp != nullptr && sz == 0){
			valid_size = strlen(csp) + 1;
		}
	}
	msg_buffer_t(const string & str) :msg_buffer_t(str.c_str(), str.length() + 1){}
	int create(int max_sz, bool init = false){
        destroy();
        char * p = (char*)malloc(max_sz);
        if (!p) return -1;
        buffer = p;
        max_size = max_sz;
        if (init){ memset(buffer, 0, max_sz); }
		valid_size = 0;
		return 0;
	}
    int  reserve(int max_sz, bool init = false){
        if (max_size < max_sz ){            
            if (max_size == 0){
                buffer = nullptr; //str
            }
            char * p = (char*)realloc(buffer, max_sz);
            if (!p) {
                destroy();
                return -1;
            }
            buffer = p;
            max_size = max_sz;
            //expanding
            if (init){
                memset(buffer + max_size, 0, max_sz - max_size);
            }
        }
        else { //cut
            if (init){
                memset(buffer + max_sz, 0, max_size - max_sz);
            }
        }
        valid_size = 0;
        return 0;
    }
    int  copy(const char * buf, int sz){
        destroy();
        if (create(sz)){
            return -1;
        }
        memcpy(buffer, buf, sz);
        valid_size = sz;
        return 0;
    }
	void destroy()
	{
		if (buffer && max_size > 0) {
			free(buffer);
		}
        buffer = nullptr;
        valid_size = max_size = 0;
	}
};
