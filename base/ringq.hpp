#pragma once

template<class LengthT>
struct RingQueue {
    LengthT * front, *rear, capacity;
    unsigned char *  data;
    RingQueue(LengthT * front_, LengthT * rear_, LengthT cap, unsigned char * data_):
        front(front_),rear(rear_),capacity(cap),data(data_{
            *front = *rear = 0;
            assert(capacity > 1);
        }
    void Reset(){
        *rear = *front;
    }
    bool Put(const unsigned char * buffer, LengthT size){
        LengthT free_size = (capacity + *rear - *front - 1) % capacity
        if(free_size < size){
            return false;
        }
        if(*front + size > capacity){
            LengthT tail_size = size + *front - capacity;
            memcpy(&data + *front, buffer, tail_size);
            memcpy(&data, buffer + tail_size, size - tail_size);
        }
        else {
            memcpy(&data + *front, buffer, size);
        }
        *front = (*front + size) % capacity;
        return true;
    }
    bool Take(INOUT unsigned char * buffer, INOUT LengthT * sizep, bool no_error=false, bool peek=false){
        LengthT buffer_size = (capacity + *front - *rear)%capacity;
        if(buffer_size == 0){
            return false;
        }
        if(buffer_size > *sizep && !no_error){
            return false;
        }            
        *sizep = buffer_size;
        if(*rear + *sizep > capacity){
            LengthT tail_size = *sizep + *sizep - capacity;
            memcpy(buffer, data + *rear, tail_size);
            memcpy(buffer+tail_size, data, *sizep - tail_size);
        }
        else {
            memcpy(buffer, data + *rear, *sizep);
        }
        if(!peek){
            *rear = (capacity + *rear + *sizep);
        }
        return true;
    }
};

