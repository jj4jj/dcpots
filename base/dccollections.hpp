#pragma once

#include "dcarray.hpp"
#include "dcmmpool.hpp"
#include "dchashtable.hpp"

namespace dcs {
    //////////////////////dynamic/////////////////////////////////////////////
    struct mempool_impl_t;
    struct mempool_conf_t {
        enum strategy {
            MEMPOOL_STRATEGY_BITMAP = 0,
            MEMPOOL_STRATEGY_LIST = 1,
        };
        void *      data;
        size_t      data_size;
        size_t      block_size;
        size_t      block_max;
        bool        attach;
        strategy    stg;
        mempool_conf_t();
    };
    struct mempool_t {
        int     init(const mempool_conf_t & conf);
        void *  alloc();
        void    free(void *);
        void *  ptr(size_t id);
        size_t  id(void *) const;
        size_t  used() const;
        size_t  capacity() const;
        static size_t  size(const mempool_conf_t & conf);
    public:
        mempool_t();
        mempool_impl_t *   impl_;
    };

    ////////////////////////////////////////////////////////////////////
    struct hashmap_impl_t;
    struct hashmap_conf_t {
        mempool_conf_t mm;
        typedef void(*block_init)(void * blk);
        typedef size_t(*block_hash)(const void * blk);
        typedef int(*block_comp)(const void * blkl, const void * blkr);
        block_init  init;
        block_hash  hash;
        block_comp  comp;
        hashmap_conf_t();
    };

    struct hashmap_t {
        int         init(const mempool_conf_t & conf);
        void *      insert(const void * blk, bool unique = true);
        void *      find(const void * blk);
        void        remove(const void * blk);
        void *      next(void * blkprev);
        size_t      capacity() const;
        size_t      used() const;
        static size_t   size(const hashmap_conf_t & conf);
    public:
        hashmap_t();
        hashmap_impl_t  * impl_;
    };


};





