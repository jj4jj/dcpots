#pragma once

#include "dcarray.hpp"

template<class T, size_t cmax>
struct mmpool_t {
    typedef uint64_t mmpool_bit_block_t;
    #define mmpool_bit_block_byte_sz  (sizeof(mmpool_bit_block_t))
    #define mmpool_bit_block_bit_sz  (8*mmpool_bit_block_byte_sz)
    #define mmpool_bit_block_count ((cmax+mmpool_bit_block_bit_sz-1)/mmpool_bit_block_bit_sz)
    typedef array_t<T, mmpool_bit_block_count*mmpool_bit_block_bit_sz>    allocator_t;
    allocator_t                 allocator_;
    mmpool_bit_block_t          bmp_[mmpool_bit_block_count];
    size_t                      used_;
    /////////////////////////////////////////////////////////////////////////////////////////
    void   construct(){
        memset(this, 0, sizeof(*this));
        allocator_.count = cmax;
        used_ = 0;
    }
    const allocator_t & allocator() {
        return allocator_;
    }
    size_t alloc(){
        if (used() >= capacity()){
            return 0; //full
        }
        //1.find first 0 set 1
        size_t x, i;
        for (i = 0, x = 0; i < mmpool_bit_block_count; ++i){
            if ((x = __builtin_ffsll(~(bmp_[i])))){
                break;
            }
        }
        if (x != 0){
            bmp_[i] |= (1ULL << (x - 1));//set 1
            size_t id = i*mmpool_bit_block_bit_sz + x;
            ++used_;
            new (&(allocator_.list[id - 1]))T();
            return id;
        }
        else {
            return 0;
        }
    }
    size_t id(const T * p) const {
        assert(p >= allocator_.list && p < allocator_.list + cmax);
        return 1 + (p - allocator_.list);
    }
    T * ptr(size_t id) {
        if (id > 0 && id <= capacity() && isbusy(id)){
            return &(allocator_.list[id - 1]);
        }
        else {
            return NULL;
        }
    }
    bool isbusy(size_t id){
        assert(id > 0);
        size_t idx = id - 1;
        return bmp_[idx / mmpool_bit_block_bit_sz] & (1ULL << (idx%mmpool_bit_block_bit_sz));
    }
    size_t capacity() const {
        return cmax;
    }
    bool   empty() const {
        return used() == 0;
    }
    size_t used() const {
        return used_;
    }
    size_t next(size_t it) const {
        if (used_ == 0){
            return 0;
        }
        bool head = true; //(it+1)-1 = idx
        uint32_t pos_bit_ffs = 0;
        for (size_t idx = it / mmpool_bit_block_bit_sz, pos_bit_offset = it % mmpool_bit_block_bit_sz;
            idx < sizeof(bmp_) / sizeof(bmp_[0]); ++idx) {
            if (!head) {
                pos_bit_offset = 0;
            }
            else {
                head = false;
            }
            if (bmp_[idx] != 0ULL) {
                pos_bit_ffs = __builtin_ffsll(bmp_[idx] >> pos_bit_offset);
                if (pos_bit_ffs > 0) {
                    return idx*mmpool_bit_block_bit_sz + pos_bit_offset + pos_bit_ffs;
                }
            }
        }
        return 0;
    }
    void   free(size_t id){
        assert(id > 0);
        size_t idx = id - 1;
        if (isbusy(id)){ //set 0
            bmp_[idx / mmpool_bit_block_bit_sz] &= (~(1ULL << (idx%mmpool_bit_block_bit_sz)));
            --used_;
            allocator_.list[idx].~T();
        }
    }
};
