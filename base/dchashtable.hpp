#pragma once
#include "dcmmpool.hpp"
#include "dcmath.hpp"
//multi layer hash table implementation
//---------------------------
//-----------------------------
//---------------------------------------
//collision strategy : 
//1.create a link list in multiple layer
//2.in last max layer linear probe solving
namespace dcs {
template<class T, size_t cmax, size_t layer = 3, class HcfT = ::std::hash<T>>
struct hashtable_t {
    struct hashmap_entry_t {
        size_t  id;
        size_t  next;
        size_t  hco;
    };
    struct {
        size_t  offset;
        size_t  count;
    } hash_layer_segment[layer];
    enum {
        hash_entry_index_1st_layer_size = next_prime<cmax, true>::value,
        hash_entry_index_size = (layer*hash_entry_index_1st_layer_size),
    };
    typedef mmpool_t<T, cmax>                                   pool_t;
    typedef array_t<hashmap_entry_t, hash_entry_index_size>     index_t;
    index_t     index_;
    pool_t      mmpool_;
    size_t      stat_probe_insert;
    size_t      stat_insert;
    size_t      stat_probe_read;
    size_t      stat_hit_read;
    HcfT        hcf;
    ////////////////////////////////////////////////////////////
    void        construct(){
        memset(&index_, 0, sizeof(index_));
        index_.count = hash_entry_index_size;
        mmpool_.construct();
        stat_probe_insert = stat_insert =
            stat_hit_read = stat_probe_read = 1;//for div 0 error
        //bigger and bigger but max is limit
        hash_layer_segment[0].offset = 1;
        size_t hash_layer_max_size = hash_entry_index_1st_layer_size;
        hash_layer_segment[layer - 1].count = hash_layer_max_size;
        for (int i = layer - 2; i >= 0; --i){
            hash_layer_segment[i].count = prime_prev(hash_layer_segment[i + 1].count);
        }
        for (size_t i = 1; i < layer; ++i){
            hash_layer_segment[i].offset = hash_layer_segment[i - 1].offset + hash_layer_segment[i - 1].count;
        }
        assert(hash_entry_index_size >= hash_layer_segment[layer - 1].count + hash_layer_segment[layer - 1].offset);
    }
    const pool_t &  mmpool() const { return mmpool_; }
    int         load(int rate = 100) const {
        return mmpool_.used() * rate / index_.capacity();
    }
    int         factor(){
        return cmax * 100 / index_.capacity();
    }
    int         hit(int rate = 100) const {
        return stat_hit_read * rate / stat_probe_read;
    }
    int         collision() const {
        return stat_probe_insert / stat_insert;
    }
    const char * layers(::std::string & str) const {
        for (int i = 0; i < layer; ++i){
            str.append("[" +
                ::std::to_string(this->hash_layer_segment[i].offset) + "," +
                ::std::to_string(this->hash_layer_segment[i].count) + ")");
        }
        return str.c_str();
    }
    const char * stat(::std::string & str){
        str += "mbytes size:" + ::std::to_string(sizeof(*this)) +
            " mused:" + ::std::to_string(this->mmpool().used()) + "/" + ::std::to_string(cmax) +
            " musage:" + ::std::to_string(this->mmpool().used() / cmax) +
            " iload:" + ::std::to_string(this->load()) +
            " ihit:" + ::std::to_string(this->hit()) +
            " ifactor:" + ::std::to_string(this->factor()) +
            " icollision:" + ::std::to_string(this->collision()) +
            " ilayers:" + ::std::to_string(layer);
        return this->layers(str);
    }
    bool        empty() const {
        return mmpool_.empty();
    }
    bool        full() const {
        return mmpool_.used() >= cmax;
    }
    size_t      next_slot(size_t hc, size_t ref){
        assert(ref > 0);
        assert(index_.list[ref].next == 0);
        size_t find_slot = 0;
        if (ref < hash_layer_segment[layer - 1].offset){
            for (size_t i = 0; i < layer - 1; ++i){
                ++stat_probe_insert;
                find_slot = hash_layer_segment[i].offset + hc % hash_layer_segment[i].count;
                if (0 == index_.list[find_slot].id){
                    return find_slot;
                }
            }
        }
        //next one idle
        find_slot = ref;
        while (true){
            ++stat_probe_insert;
            find_slot = hash_layer_segment[layer - 1].offset + (find_slot + 1) % hash_layer_segment[layer - 1].count;
            if (0 == index_.list[find_slot].id){
                return find_slot;
            }
        }
        assert(false);
        return 0;
    }
    T *         insert(const T & k){
        if (full()){
            return NULL; //full
        }
        //need to optimalization
        size_t hco = hcf(k);
        size_t idx = 1 + hco % hash_layer_segment[0].count;
        T * pv = NULL;
        while (idx && index_.list[idx].id){//find collision queue available
            ++stat_probe_insert;
            pv = mmpool_.ptr(index_.list[idx].id);
            if (index_.list[idx].hco == hco && *pv == k){
                return NULL;
            }
            if (index_.list[idx].next == 0){
                break;
            }
            idx = index_.list[idx].next;
        }
        if (idx > 0){ //valid idx
            if (index_.list[idx].id > 0){ //link list tail
                assert(index_.list[idx].next == 0);
                size_t npos = next_slot(hco, idx);
                assert(npos > 0 && index_.list[npos].id == 0);
                size_t hid = mmpool_.alloc();
                if (hid == 0){
                    return NULL;
                }
                //#list append collision queue
                index_.list[idx].next = npos;
                index_.list[npos].id = hid;
                index_.list[npos].hco = hco;
                index_.list[npos].next = 0;
                ++stat_insert;
                pv = mmpool_.ptr(hid);
                *pv = k;
                return pv;
            }
            else { //head pos or in layer link list node
                size_t hid = mmpool_.alloc();
                if (hid == 0){
                    return NULL;
                }
                hashmap_entry_t & he = index_.list[idx];
                he.id = hid;
                he.hco = hco;
                ++stat_insert;
                pv = mmpool_.ptr(hid);
                *pv = k;
                return pv;
            }
        }
        return NULL;
    }
    int         remove(const T & k){
        //need to optimalization
        size_t hco = hcf(k);
        size_t idx = 1 + hco % hash_layer_segment[0].count;
        size_t pidx = 0;
        size_t ltidx = hash_layer_segment[layer - 1].offset + hco % hash_layer_segment[layer - 1].count;
        while (idx){
            if (index_.list[idx].id){
                T * p = mmpool_.ptr(index_.list[idx].id);
                if (index_.list[idx].hco == hco && *p == k){
                    mmpool_.free(index_.list[idx].id);
                    if (idx < hash_layer_segment[layer - 1].offset || idx == ltidx){ //middle layer
                        index_.list[idx].id = 0; //just erase it (no change list)
                        index_.list[idx].hco = 0; //just erase it (no change list)
                        return 0;
                    }
                    assert(pidx > 0);
                    index_.list[idx].id = 0;
                    index_.list[idx].hco = 0;
                    index_.list[pidx].next = index_.list[idx].next;
                    index_.list[idx].next = 0;
                    return 0;
                }
            }
            pidx = idx;
            idx = index_.list[idx].next;
        }
        return -1;
    }
    T *         find(const T & k){
        //need to optimalization
        size_t hco = hcf(k);
        size_t idx = 1 + hco % hash_layer_segment[0].count;
        while (idx){
            ++stat_probe_read;
            if (index_.list[idx].id){
                T * pv = mmpool_.ptr(index_.list[idx].id);
                if (index_.list[idx].hco == hco && *pv == k){
                    ++stat_hit_read;
                    return pv;
                }
            }
            idx = index_.list[idx].next;
        }
        return NULL;
    }
};




}