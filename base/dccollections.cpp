

#include "dccollections.hpp"

namespace dcsutil {
struct mempool_impl_t {
    mempool_conf_t  conf;    
};
mempool_conf_t::mempool_conf_t(){
    data = nullptr;
    data_size = block_size = block_max = 0;
    stg = MEMPOOL_STRATEGY_BITMAP;
    attach = false;
}
int     mempool_t::init(const mempool_conf_t & conf){
    //1. check size match ?
    //2. check attach ?
    //3. dispatch strategy 
    //4. impl
    return 0;
}
void *  mempool_t::alloc(){
    //todo
    return nullptr;
}
void    mempool_t::free(void *){
    //todo
}
void *  mempool_t::ptr(size_t id){
    //todo
    return nullptr;
}
size_t  mempool_t::id(void *){
    //todo
    return 0;
}
size_t  mempool_t::used() const {
    //todo
    return 0;
}
size_t  mempool_t::capacity() const {
    //todo
    return 0;
}
size_t  mempool_t::size(const mempool_conf_t & conf){
    //todo
    return 0;
}
mempool_t::mempool_t(){
    impl_ = nullptr;
}


/////////////////////////////////////////////////////////////////////////////////////


hashmap_conf_t::hashmap_conf_t() {
    init = nullptr;
    hash = nullptr;
    comp = nullptr;
}
int         hashmap_t::init(const mempool_conf_t & conf){
    //todo
    return 0;
}
void *      hashmap_t::insert(const void * blk, bool unique = true){
    //todo
    return nullptr;
}
void *      hashmap_t::find(const void * blk){
    //todo
    return nullptr;
}
void        hashmap_t::remove(const void * blk){
}
void *      hashmap_t::next(void * blkprev){
    //todo
    return nullptr;
}
size_t      hashmap_t::capacity() const {
    //todo
    return 0;
}
size_t      hashmap_t::used() const {
    //todo
    return 0;
}
size_t      hashmap_t::size(const hashmap_conf_t & conf){
    //todo
    return 0;
}








}