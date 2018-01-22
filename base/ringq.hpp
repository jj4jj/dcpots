#pragma once

//push tail and pop head
template<class LengthT>
struct RingQueue {
    LengthT * tail, *head, capacity;
    unsigned char *  data;
    RingQueue(LengthT * tail_, LengthT * head_, LengthT cap, unsigned char * data_):
        tail(tail_),head(head_),capacity(cap),data(data_{
            *tail = *head = 0;
            assert(capacity > 1);
        }
    void Reset(){
        *head = *tail;
    }
    bool Put(const unsigned char * buffer, LengthT size){
        LengthT free_size = (capacity + *head - *tail - 1) % capacity
        if(free_size < size){
            return false;
        }
        if(*tail + size > capacity){
            LengthT tail_size = size + *tail - capacity;
            memcpy(&data + *tail, buffer, tail_size);
            memcpy(&data, buffer + tail_size, size - tail_size);
        }
        else {
            memcpy(&data + *tail, buffer, size);
        }
        *tail = (*tail + size) % capacity;
        return true;
    }
    bool Take(INOUT unsigned char * buffer, INOUT LengthT * sizep, bool no_error=false, bool peek=false){
        LengthT buffer_size = (capacity + *tail - *head)%capacity;
        if(buffer_size == 0){
            return false;
        }
        if(buffer_size > *sizep && !no_error){
            return false;
        }            
        *sizep = buffer_size;
        if(*head + *sizep > capacity){
            LengthT tail_size = *sizep + *sizep - capacity;
            memcpy(buffer, data + *head, tail_size);
            memcpy(buffer+tail_size, data, *sizep - tail_size);
        }
        else {
            memcpy(buffer, data + *head, *sizep);
        }
        if(!peek){
            *head = (capacity + *head + *sizep);
        }
        return true;
    }
};

