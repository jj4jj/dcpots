#pragma once
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cassert>

template<class T, size_t cmax, class LengthT = uint32_t>
struct array_t {
    T           list[cmax];
    LengthT     count;

    /////////////////////////////////        
    void construct(){
        count = 0;
    }
    size_t capacity() const {
        return cmax;
    }
    bool   full() const {
        return this->count >= cmax;
    }
    bool   empty() const {
        return this->count == 0;
    }
    void   clear() {
        this->count = 0;
    }
    bool operator == (const array_t & rhs) const {
        return compare(rhs) == 0;
    }
    bool operator < (const array_t & rhs) const {
        return compare(rhs) < 0;
    }
    bool operator > (const array_t & rhs) const {
        return compare(rhs) > 0;
    }
    int  compare(const array_t & rhs) const {
        if (count < rhs.count){
            for (LengthT i = 0; i < count; ++i){
                if (list[i] < rhs.list[i]){
                    return -1;
                }
                else if (!(list[i] == rhs.list[i])){
                    return 1;
                }
            }
        }
        else {
            for (LengthT i = 0; i < rhs.count; ++i){
                if (list[i] < rhs.list[i]){
                    return -1;
                }
                else if (!(list[i] == rhs.list[i])){
                    return 1;
                }
            }
        }
        return (int)(count - rhs.count);
    }
    ////////////////////////////////////////////////////
    //linear list///////////////////////////////////////
    int lfind(const T & tk) const {
        for (LengthT i = 0; i < count && i < cmax; ++i){
            if (list[i] == tk){
                return i;
            }
        }
        return -1;
    }
    int lappend(const T & td, bool shift_overflow = false){
        if (count >= cmax && !shift_overflow){
            return -1;
        }
        if (count < cmax){
            list[count] = td;
            ++count;
            return 0;
        }
        else {
            if (cmax > 0){
                memmove(list, list + 1, (cmax - 1)*sizeof(T));
                list[cmax - 1] = td;
            }
        }
        return 0;
    }
    int lremove(int idx, bool swap_remove = false){
        if (idx < 0 || (LengthT)idx >= count){
            return -1;
        }
        if (swap_remove){
            list[idx] = list[cmax - 1];
            //list[cmax - 1].construct();
        }
        else {
            memmove(list + idx, list + idx + 1, (count - idx - 1)*sizeof(T));
        }
        --count;
        return 0;
    }
    int linsert(int idx, const T & td, bool overflow_shift = false){
        if (count >= cmax && !overflow_shift){
            return -1;
        }
        if ((LengthT)idx >= count){
            idx = count;
        }
        else if (idx < 0){
            idx = 0;
        }
        if ((LengthT)idx == count){
            return lappend(td, overflow_shift);
        }
        //--overlay------idx------>-----idx+1------------------------------
        if (count >= cmax){
            memmove(list + idx + 1, list + idx, (cmax - 1 - idx)*sizeof(T));
            list[idx] = td;
        }
        else {
            memmove(list + idx + 1, list + idx, (count - idx)*sizeof(T));
            list[idx] = td;
            ++count;
        }
        return 0;
    }
    void lsort(array_t & out) const {
        memcpy(&out, this, sizeof(*this));
        ::std::sort(out.list, out.list + out.count);
    }
    //////////////////////////////////////////////////////////
    /////////////////////binary-seaching//////////////////////
    int bfind(const T & tk) const {
        int idx1st = lower_bound(tk);
        int idx2nd = upper_bound(tk);
        if (idx1st == idx2nd){
            return -1;
        }
        while (idx1st < idx2nd){
            if (tk == list[idx1st]){
                return idx1st;
            }
            ++idx1st;
        }
        return -1;
    }
    int binsert(const T & td, bool overflow_shift = false, bool uniq = false) {
        LengthT idx = lower_bound(td);
        if (uniq && idx < count) {
            if (list[idx] == td) {
                return -1;
            }
        }
        return linsert(idx, td, overflow_shift);
    }
    int bremove(const T & tk){
        int idx = bfind(tk);
        if (idx < 0){ return -1; }
        return lremove(idx, false);
    }
    LengthT lower_bound(const T & tk) const {
        const T * p = ::std::lower_bound((T*)list, (T*)list + count, (T&)tk);
        return (p - list);
    }
    LengthT upper_bound(const T & tk) const {
        const T * p = ::std::upper_bound((T*)list, (T*)list + count, (T&)tk);
        return (p - list);
    }
    T & operator [](size_t idx){
        assert(idx < count);
        return list[idx];
    }
    const T & operator [](size_t idx) const {
        assert(idx < count);
        return list[idx];
    }
};

