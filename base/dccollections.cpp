#include "logger.h"
#include "dcmath.hpp"
#include "dccollections.h"
///////////////////////////////////////////////////////////////////////////////////////////
#define MMPOOL_HEAD_MAGIC	("mmpool-head")
#define MMPOOL_MAGIC_BEGIN	("mmpool-magic-begin")
#define MMPOOL_MAGIC_END	("mmpool-magic-end")
#define DIV_CEIL(s,n)		(((s)+(n-1))/n)
#define ALIGN_N(s,n)		(DIV_CEIL(s,n)*n)
#define MMPOOL_DATA_BEGIN_PADDING_BUFF_SIZE    (1024)
#define MMPOOL_PADDING_END_BLOCK_LIST    (1024)
#define MEMPOOL_MAGIC_BUFF_SIZE (32)
///////////////////////////////////////////////////////////////////////////////////////////
namespace dcs {
struct mempool_impl_t {
    mempool_conf_t::block_init  init;
	mempool_conf_t::strategy    stg;
	size_t						data_size;
	size_t						block_size;
	size_t						block_max;
	char						magic[MEMPOOL_MAGIC_BUFF_SIZE];
	size_t                      used;
    size_t                      capacity;
	size_t						last_alloc_id;
	size_t						data_begin;
	size_t						data_end;
};
union mempool_impl_head_t {
	///////////////////////////////////////////
    struct bmp_t {
        size_t                  bmp_count;
        uint64_t				bmp_block[1];
    } bmp;
	///////////////////////////////////////////
	struct block_list_t {
		struct queue_t {
			size_t			front;
			size_t			rear;
		};
		struct entry_t {
			uint32_t		busy:1;
			uint32_t		prev:31;
			uint32_t		next;
		};
		queue_t				free;
		queue_t				busy;
		entry_t				list[1];
	} bkl;
};
struct mempool_layout_desc {
    mempool_impl_t          impl;
    mempool_impl_head_t     head;
    mempool_impl_head_t     index[1];
    char    padding_data_begin[MMPOOL_DATA_BEGIN_PADDING_BUFF_SIZE];
    char    data[1];//data_begin,data_end
    char    padding_data_end[MMPOOL_PADDING_END_BLOCK_LIST];
};

mempool_conf_t::mempool_conf_t(){
    init = nullptr;
    data = nullptr;
    data_size = block_size = block_max = 0;
    stg = MEMPOOL_STRATEGY_BITMAP;
    attach = false;
}
static inline int mmpool_bitmap_init(mempool_impl_head_t::bmp_t * bmp, size_t blksz, size_t nblk, bool attach) {
    if(!attach){        
        memset(bmp->bmp_block, 0, bmp->bmp_count*sizeof(uint64_t));
    }
    else {
        if(bmp->bmp_count != DIV_CEIL(nblk, 64)){
            return -1;
        }
    }
	return 0;
}
static inline int mmpool_blklist_init(mempool_impl_head_t::block_list_t * bkl, size_t blksz, size_t nblk, bool attach) {
	if (!attach){
		memset(bkl, 0, sizeof(*bkl));
		bkl->free.front = bkl->free.rear = 0;
		bkl->busy.front = bkl->busy.rear = 0;
		//init construct a linked list free
        bkl->list[0].busy = 0;
		bkl->list[0].next = 1;
		bkl->list[0].prev = nblk-1;
		for (size_t i = 1; i < nblk; ++i) {
            bkl->list[i].busy = 0;
			bkl->list[i].prev = i-1;
			bkl->list[i].next = (i+1)%nblk;
		}
	}
	return 0;
}
int     mempool_t::init(const mempool_conf_t & conf){
	size_t total_size = size(conf.stg, conf.block_max, conf.block_size);
	if (total_size == 0 || conf.data_size < total_size) {
		GLOG_ERR("mempool config data size:%zu need size:%zu conf(stg:%d,nblk:%d,blksz:%d)error !", 
                conf.data_size, total_size,
                conf.stg, conf.block_max, conf.block_size);
		return -1;
	}
    GLOG_DBG("mempool init config data size:%zu need size:%zu conf(stg:%d,nblk:%d,blksz:%d) ...",
        conf.data_size, total_size,
        conf.stg, conf.block_max, conf.block_size);
    impl_ = (mempool_impl_t*)conf.data;
    //////////////////////////////////////////////////////////////////////////
	mempool_impl_head_t * mphead = (mempool_impl_head_t*)&(impl_[1]);
	if (conf.attach) {
		if (strcmp(MMPOOL_HEAD_MAGIC, impl_->magic)) {
			GLOG_ERR("mmpool magic head error [%s] !", impl_->magic);
			return -1;
		}
		if (strcmp(MMPOOL_MAGIC_BEGIN, (char *)impl_ + impl_->data_begin - MMPOOL_DATA_BEGIN_PADDING_BUFF_SIZE)) {
			GLOG_ERR("mmpool magic data begin error [%s] !", (char *)impl_ + impl_->data_end);
			return -1;
		}
        if (strcmp(MMPOOL_MAGIC_END, (char *)impl_ + impl_->data_end)) {
            GLOG_ERR("mmpool magic data end error [%s] !", (char *)impl_ + impl_->data_end);
            return -1;
        }
        impl_->init = conf.init;
	}
	else {
		memset(impl_, 0, sizeof(*impl_));
        impl_->init = conf.init;
		impl_->block_max = conf.block_max;
		impl_->data_size = conf.data_size;
		impl_->block_size = conf.block_size;
		impl_->stg = conf.stg;
		impl_->data_begin = sizeof(mempool_impl_t) + sizeof(mempool_impl_head_t);
		if (conf.stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
            mphead->bmp.bmp_count = DIV_CEIL(conf.block_max,64);
            impl_->capacity = mphead->bmp.bmp_count*64;
            impl_->data_begin += (sizeof(uint64_t)*mphead->bmp.bmp_count) ;
        }
		else if (conf.stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
            impl_->capacity = conf.block_max;
			impl_->data_begin += sizeof(mempool_impl_head_t::block_list_t::entry_t)*(impl_->capacity);
		}
		else {
			assert("not support now !" && false);
		}
        impl_->data_begin += MMPOOL_DATA_BEGIN_PADDING_BUFF_SIZE;
		impl_->data_end = impl_->data_begin + conf.block_size*impl_->capacity;
		strcpy(impl_->magic, MMPOOL_HEAD_MAGIC);
		strcpy((char *)impl_ + impl_->data_begin - MMPOOL_DATA_BEGIN_PADDING_BUFF_SIZE, MMPOOL_MAGIC_BEGIN);
        strcpy((char *)impl_ + impl_->data_end, MMPOOL_MAGIC_END);
	}
	if (conf.stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
		return mmpool_bitmap_init(&mphead->bmp, conf.block_size, conf.block_max, conf.attach);
	}
	else if (conf.stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
		return mmpool_blklist_init(&mphead->bkl, conf.block_size, conf.block_max, conf.attach);
	}
	else {
		GLOG_ERR("not support mempool strategy type:%d",conf.stg);
		return -1;
	}
    return 0;
}
void *  mempool_t::alloc(){
	void * p = nullptr;
	if (impl_->used >= impl_->block_max) {
		return nullptr;
	}
	if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
        mempool_impl_head_t::bmp_t * bmp = &((mempool_impl_head_t*)(&impl_[1]))->bmp;
        size_t bidx = impl_->last_alloc_id/64;
		//find first 0
		size_t ffo = 0;
		for (size_t i = 0; i < bmp->bmp_count; ++i, bidx = (bidx+1)% bmp->bmp_count) {			
			if ((ffo = __builtin_ffsll(~(bmp->bmp_block[bidx])))) {
				break;
			}
		}
		assert(ffo && (0 == (bmp->bmp_block[bidx]&(1ULL << (ffo-1)))));
		p = (char*)impl_ + impl_->data_begin + (bidx*64+ffo-1)*impl_->block_size;
		bmp->bmp_block[bidx] |= (1ULL << (ffo - 1));//set 1
		++impl_->used;
		impl_->last_alloc_id = bidx * 64 + ffo;
        if (impl_->last_alloc_id >= impl_->capacity) {
            impl_->last_alloc_id = 0;
        }
	}
	else if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
		//free head alloc
		mempool_impl_head_t::block_list_t * bkl = &((mempool_impl_head_t*)(&impl_[1]))->bkl;
		size_t idx = bkl->free.front;
		assert(0 == bkl->list[idx].busy);
		//erase from free
		bkl->list[bkl->list[idx].prev].next = bkl->list[idx].next;
		bkl->list[bkl->list[idx].next].prev = bkl->list[idx].prev;
		//append to busy
		bkl->list[idx].next = bkl->busy.front;
		bkl->list[idx].prev = bkl->busy.rear;
		bkl->list[bkl->busy.rear].next = idx;
		bkl->list[idx].busy = 1;
		p = (char*)impl_ + impl_->data_begin + idx*impl_->block_size;
		++impl_->used;
		impl_->last_alloc_id = idx + 1;
	}
    if(impl_->init){
        impl_->init(p);
    }
    return p;
}
int    mempool_t::free(void * p){
	int ret = -1;
	if (impl_->used == 0) {
		return -1;
	}
	size_t zid = this->id(p);
	if (zid == 0) {
		return -2;
	}
	size_t idx = zid - 1;
	if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
        mempool_impl_head_t::bmp_t * bmp = &((mempool_impl_head_t*)(&impl_[1]))->bmp;
		uint64_t  mask = (1ULL << (idx % 64));
		size_t bidx = idx / 64;
		if (bmp->bmp_block[bidx] & mask) {
			bmp->bmp_block[bidx] &= (~mask);
			ret = 0;
		}
		else {
			ret = -3;
		}
	}
	else if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
		mempool_impl_head_t::block_list_t * bkl = &((mempool_impl_head_t*)(&impl_[1]))->bkl;
		if (1 == bkl->list[idx].busy) {
			//erase from busy
			bkl->list[bkl->list[idx].prev].next = bkl->list[idx].next;
			bkl->list[bkl->list[idx].next].prev = bkl->list[idx].prev;
			//append into free
			bkl->list[idx].prev = bkl->free.rear;
			bkl->list[idx].next = bkl->free.front;
			bkl->list[idx].busy = 0;
			bkl->list[bkl->free.rear].next = idx;
			bkl->free.rear = idx;
			ret = 0;
		}
		else {
			ret = -4;
		}
	}
	if (0 == ret) {
		--impl_->used;
	}
    return ret;
}
void *  mempool_t::ptr(size_t id) const {
	if (impl_->used == 0 || id == 0 || id > impl_->capacity) {
		return nullptr;
	}
	return (char*)impl_ + impl_->data_begin + (id-1)*impl_->block_size;
}
size_t  mempool_t::id(const void * p) const {
	if (impl_->used == 0 || 
		p < (char*)impl_ + impl_->data_begin ||
		p >= (char*)impl_ + impl_->data_end) {
		return 0;
	}
	return ((char*)p - (char*)impl_ - impl_->data_begin) / impl_->block_size + 1;
}
size_t  mempool_t::used() const {
	return impl_->used;
}
size_t  mempool_t::capacity() const {
    return impl_->block_max;
}
void *  mempool_t::next(void * blk) {
    if(used() == 0){
        return nullptr;
    }
	if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
        mempool_impl_head_t::bmp_t * bmp = &((mempool_impl_head_t*)(&impl_[1]))->bmp;
        size_t blkid = 0;
        if(blk){
            blkid = this->id(blk);//idx = (id+1) -1 :NEXT
        }
		size_t blkidx = blkid/64;
		size_t bitidx = blkid%64;
        for (int i = bitidx; i < 64; ++i) {
            if (bmp->bmp_block[blkidx] & (1ULL << i)) {//busy
                return this->ptr(blkidx * 64 + i + 1);
            }
        }
        //next
        ++blkidx;
        for (; blkidx < bmp->bmp_count; ++blkidx) {
            bitidx = __builtin_ffsll(bmp->bmp_block[blkidx]);
            if(bitidx){
                return this->ptr(blkidx * 64 + bitidx);
            }
		}
		return nullptr;
	}
	else if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
        size_t blkidx = 0;
		mempool_impl_head_t::block_list_t * bkl = &((mempool_impl_head_t*)(&impl_[1]))->bkl;
        if(blk){
            blkidx = this->id(blk) - 1;
        }
		if (blkidx == bkl->busy.rear) {
			return nullptr;
		}
		assert(1 == bkl->list[blkidx].busy);
		blkidx = bkl->list[blkidx].next;
		return this->ptr(blkidx+1);
	}
	return nullptr;
}
size_t  mempool_t::size(mempool_conf_t::strategy stg, size_t nblk, size_t blksz){
	if (nblk == 0 || blksz == 0) {
		return 0;
	}
    size_t rnblk = nblk;
    size_t head_size = sizeof(mempool_impl_t) + sizeof(mempool_impl_head_t) + MMPOOL_DATA_BEGIN_PADDING_BUFF_SIZE + MMPOOL_PADDING_END_BLOCK_LIST;
	if (stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
        size_t  bmp_count = DIV_CEIL(nblk, 64);
        head_size += sizeof(uint64_t)* bmp_count;
        rnblk = bmp_count*64;
	}
	else if (stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
        head_size += sizeof(mempool_impl_head_t::block_list_t::entry_t)* nblk;
	}
	else {		
		return 0;
	}
    return ALIGN_N(head_size + rnblk*blksz, 16);
}
const char * mempool_t::stat(::std::string & str) const {
	str = "Total size:" + ::std::to_string(impl_->data_size) +
		" Bytes, Used:" + ::std::to_string(this->used()) + "/" + ::std::to_string(this->capacity()) +
		", Usage:" + ::std::to_string(this->used() * 100 / this->capacity()) + "%";
	return str.c_str();
}

mempool_t::mempool_t(){
    impl_ = nullptr;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
hashmap_conf_t::hashmap_conf_t() {
	data = nullptr;
    hash = nullptr;
    comp = nullptr;
	data_size = block_max = block_size = 0;
	attach = false;
	layer = 3;
}
#define  HASHMAP_MAGIC		("hashmap-magic")
#define HASHMAP_MAX_LAYER	(11)
//multi layer hash
#define HASH_MAP_LAYER_1ST_FACTOR   (117)
#define HASH_MAP_LAYER_OTHER_FACTOR   (107)
#define HASH_MAP_MAGIC_BUFF_SIZE    (32)
//cmax(count)
struct hashmap_impl_t {
	hashmap_conf_t::block_hash  hash;
	hashmap_conf_t::block_comp  comp;
	mempool_t					mmpool;
	char						magic[HASH_MAP_MAGIC_BUFF_SIZE];
	int							layer;
	size_t						layer_offset[HASHMAP_MAX_LAYER];
	size_t						layer_size[HASHMAP_MAX_LAYER];
	size_t						stat_probe_insert;
	size_t						stat_insert;
	size_t						stat_probe_read;
	size_t						stat_hit_read;
	size_t						total_size;
	size_t						block_size;
	size_t						block_count;
	size_t						table_start;
	size_t						table_size;
	size_t						mmpool_start;
	size_t						mmpool_end;
	//////////////////////////////////////////////////////////
	struct table_item_t {
		size_t		code;
		size_t		id;
        size_t      next;
	};
	struct layout_desc { //the hash map layout
        hashmap_impl_t        * head;
		table_item_t			table[1];//layers(multi level)
		char					mmpool[1];//mmpool
		char					magic[HASH_MAP_MAGIC_BUFF_SIZE];
	};
};
int         hashmap_t::init(const hashmap_conf_t & conf){
	size_t total_size = size(conf.layer, conf.block_max, conf.block_size);
	if (total_size == 0 || conf.data_size < total_size) {
		GLOG_ERR("total size:%zu data size:%zu block max:%u block size:%u ",
                 total_size, conf.data_size, conf.block_max, conf.block_size);
		return -1;
	}
	impl_ = (hashmap_impl_t *)conf.data;
	if (conf.attach) {
		impl_->comp = conf.comp;
		impl_->hash = conf.hash;
		//check
		if (strcmp(impl_->magic, HASHMAP_MAGIC)) {
			GLOG_ERR("attach check head magic error [%s]", impl_->magic);
			return -1;
		}
		if (strcmp((char*)impl_ + impl_->mmpool_end, HASHMAP_MAGIC)) {
			GLOG_ERR("attach check tail magic error [%s]", (char*)impl_ + impl_->mmpool_end);
			return -1;
		}
		if (impl_->layer != conf.layer || impl_->block_count != conf.block_max ||
			impl_->block_size != conf.block_size) {
			GLOG_ERR("attach check layer:%zu,%zu block count:%zu,%zu max block:%zu,%zu block size:%zu,%zu ", 
				impl_->layer , conf.layer , impl_->block_count , conf.block_max ,
				impl_->block_size , conf.block_size);
			return -1;
		}
	}
	else {
		memset(impl_, 0, sizeof(*impl_));
		impl_->comp = conf.comp;
		impl_->hash = conf.hash;
		impl_->total_size = total_size;
		impl_->block_count = conf.block_max;
		impl_->block_size = conf.block_size;
		impl_->stat_hit_read = impl_->stat_insert = impl_->stat_probe_insert = \
		impl_->stat_probe_read = 1;
        /////////////////////////////////////////////////////////////////////////
		size_t prime_n = dcs::prime_next(HASH_MAP_LAYER_1ST_FACTOR*(conf.block_max + 1)/100);
		size_t table_size = prime_n;
		impl_->layer = conf.layer;
        impl_->layer_offset[0] = 1; //pos start from 1
		impl_->layer_size[0] = prime_n;
		for (int i = 1;i < impl_->layer; ++i) {
			prime_n = dcs::prime_next(HASH_MAP_LAYER_OTHER_FACTOR*prime_n/100);
			impl_->layer_size[i] = prime_n;
			table_size += prime_n;
		}
		for (int i = 1; i < conf.layer; ++i) {
			impl_->layer_offset[i] = impl_->layer_offset[i-1] + impl_->layer_size[i-1];
		}
		//code -> id
		impl_->table_start = sizeof(hashmap_impl_t);
		impl_->table_size = table_size;
		impl_->mmpool_start = impl_->table_start + sizeof(hashmap_impl_t::table_item_t)*table_size;
		impl_->mmpool_end = impl_->mmpool_start + mempool_t::size(mempool_conf_t::MEMPOOL_STRATEGY_BITMAP, conf.block_max, conf.block_size);
		strcpy(impl_->magic, HASHMAP_MAGIC);
		strcpy((char*)impl_ + impl_->mmpool_end, HASHMAP_MAGIC);
		hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
		memset(table, 0 , sizeof(hashmap_impl_t::table_item_t)*table_size);
        GLOG_DBG("hashtable mem head table size:%zd mempool offset:%zd(hash map table entry list size) mempool size:%zd", 
                table_size, impl_->mmpool_start, impl_->mmpool_end - impl_->mmpool_start);
	}
	mempool_conf_t mpc;
	mpc.stg = mempool_conf_t::MEMPOOL_STRATEGY_BITMAP;
	mpc.block_max = conf.block_max;
	mpc.block_size = conf.block_size;
	mpc.attach = conf.attach;
	mpc.data = (char*)impl_ + impl_->mmpool_start;
	mpc.data_size = impl_->mmpool_end - impl_->mmpool_start;
    int ret = impl_->mmpool.init(mpc);
    if(ret){
        GLOG_ERR("hashmap init mempool error:%d total size:%zd offset:%zd mempool size:%zd need size:%zd conf.size:%zd total need:%zd", 
        conf.data_size, impl_->mmpool_start, mpc.data_size,
            mempool_t::size(mempool_conf_t::MEMPOOL_STRATEGY_BITMAP, conf.block_max, conf.block_size),
            conf.data_size, total_size);
        return -1;
    }
    return 0;
}
static inline size_t _list_tail(hashmap_impl_t * impl_, size_t & skip_idx, size_t code, bool for_insert) {
    hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
    size_t itr_idx = skip_idx;
    if(skip_idx == 0){
        itr_idx = impl_->layer_offset[impl_->layer-1] + code % impl_->layer_size[impl_->layer-1];
    }
    assert(itr_idx >= impl_->layer_offset[impl_->layer-1] + impl_->layer_size[impl_->layer-1]);
    while(table[itr_idx].next){
        if(for_insert){
            ++impl_->stat_probe_insert;
        }
        else {
            ++impl_->stat_probe_read;        
        }
        skip_idx = itr_idx;
        itr_idx = table[itr_idx].next;
    }
    return itr_idx;
}
static inline void 	_list_remove(hashmap_impl_t * impl_, size_t remove_idx, size_t code){
    hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
    size_t skip_prev_tail_idx = remove_idx;
    size_t tail_idx = _list_tail(impl_, skip_prev_tail_idx, code, false);
    //remove the last swap with remove idx
    if (tail_idx != remove_idx) {
        table[tail_idx].next = table[remove_idx].next;
        table[remove_idx] = table[tail_idx];
    }
    table[tail_idx].code = 0;
    table[tail_idx].next = 0;
    table[tail_idx].id = 0;    
}
static inline void * _list_find(hashmap_impl_t * impl_, size_t & itr_idx, size_t code, const void * blk) {
    hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
    itr_idx = impl_->layer_offset[impl_->layer-1] + code % impl_->layer_size[impl_->layer-1];
    void * p = nullptr;
    while (itr_idx && table[itr_idx].id) {
        ++impl_->stat_probe_read;
        if(table[itr_idx].code == code){
            p = impl_->mmpool.ptr(table[itr_idx].id);
            if (0 == impl_->comp(blk, p)) {
                ++impl_->stat_hit_read;
                return p;
            }        
        }
        itr_idx = table[itr_idx].next;
    }
    return nullptr;
}
static inline size_t _list_append(hashmap_impl_t * impl_, size_t code, size_t did){
    hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
    size_t tail_prev_idx = 0;
    size_t tail_idx = _list_tail(impl_, tail_prev_idx, code, true);
    assert(tail_idx >= impl_->layer_offset[impl_->layer - 1]);
    size_t max_try = impl_->block_count;
    size_t alloc_idx = tail_idx;
    while (max_try-- > 0 && table[alloc_idx].id > 0) {
        ++impl_->stat_probe_insert;
        alloc_idx = alloc_idx + 1;
        if (alloc_idx >= impl_->layer_offset[impl_->layer - 1] + impl_->layer_size[impl_->layer - 1]) {
            alloc_idx = impl_->layer_offset[impl_->layer - 1];
        }
    }
    assert(table[alloc_idx].id == 0 && table[alloc_idx].next == 0 && table[alloc_idx].code == 0);
    table[alloc_idx].code = code;
    table[alloc_idx].id = did;
    if(tail_idx != alloc_idx){
        table[tail_idx].next = alloc_idx;
    }
    return alloc_idx;
}
void *      hashmap_t::insert(const void * blk, bool unique){
    hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
    if (unique) {
		void * p = find(blk);
		if (p) {
            GLOG_ERR("insert blk repeat !");
			return nullptr;
		}
	}
	void * p = impl_->mmpool.alloc();
	if (!p) {
        GLOG_ERR("alloc blk fail count:%zd !", impl_->mmpool.used());
		return nullptr;
	}
    memcpy(p, blk, impl_->block_size);
    ++impl_->stat_insert;
	size_t id = impl_->mmpool.id(p);
	size_t code = impl_->hash(blk) + 1;
    for (int i = 0; i + 1 < impl_->layer; ++i) {
        size_t alloc_idx = impl_->layer_offset[i] + code%impl_->layer_size[i];
        ++impl_->stat_probe_insert;
        if (table[alloc_idx].id == 0) {
            assert(table[alloc_idx].code == 0);
            table[alloc_idx].code = code;
            table[alloc_idx].id = id;
            return p;
        }
    }
    _list_append(impl_, code, id);
    return p;
}
void *      hashmap_t::find(const void * blk){
    size_t code = impl_->hash(blk) + 1 , itr_idx = 0;
    hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
    for (int i = 0; (i+1) < impl_->layer; ++i) {
        itr_idx = impl_->layer_offset[i] + code % impl_->layer_size[i];
        ++impl_->stat_probe_read;
        if (table[itr_idx].code == code) {
            void * p = impl_->mmpool.ptr(table[itr_idx].id);
            if (0 == impl_->comp(blk, p)) {
                ++impl_->stat_hit_read;
                return p;
            }
        }
    }
    return _list_find(impl_, itr_idx,  code, blk);
}
int         hashmap_t::remove(const void * blk){
    size_t code = impl_->hash(blk) + 1, itr_idx = 0;
    hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
    void * p = nullptr;
    for (int i = 0; (i + 1) < impl_->layer; ++i) {
        itr_idx = impl_->layer_offset[i] + code % impl_->layer_size[i];
        ++impl_->stat_probe_read;
        if (table[itr_idx].code == code) {
            void * p = impl_->mmpool.ptr(table[itr_idx].id);
            if (0 == impl_->comp(blk, p)) {
                ++impl_->stat_hit_read;
                table[itr_idx].code = 0;
                table[itr_idx].id = 0;
                return impl_->mmpool.free(p);
            }
        }
    }
    p = _list_find(impl_, itr_idx, code, blk);
	if (!p) {
		return -1;
	}
	_list_remove(impl_, itr_idx, code);
	return impl_->mmpool.free(p);
}
void *      hashmap_t::next(void * blk){
    return impl_->mmpool.next(blk);
}
size_t      hashmap_t::capacity() const {
	return impl_->mmpool.capacity();
}
size_t		hashmap_t::buckets() const {
	return impl_->table_size;
}
size_t      hashmap_t::used() const {
    return impl_->mmpool.used();
}
void *      hashmap_t::ptr(size_t id) const {
    return impl_->mmpool.ptr(id);
}
size_t      hashmap_t::id(const void * p) const {
    return impl_->mmpool.id(p);
}
size_t      hashmap_t::size(int layer, size_t nblk, size_t blksz){
	if (layer < 1 || layer > HASHMAP_MAX_LAYER) {
		return 0;
	}
	size_t mpsz = mempool_t::size(mempool_conf_t::MEMPOOL_STRATEGY_BITMAP, nblk, blksz);
	if (mpsz == 0) {
		return 0;
	}
    /////////////////////////////////////////////////////////
    //refer to hashmap_layout
    //impl
    //item*layers_total
    //mmpool
    //magic
	size_t prime_n = dcs::prime_next(HASH_MAP_LAYER_1ST_FACTOR * (nblk + 1) / 100);
	size_t table_size = prime_n;
	for (int i = 1; i < layer; ++i) {
		prime_n = dcs::prime_next(HASH_MAP_LAYER_OTHER_FACTOR * prime_n / 100);
		table_size += prime_n;
	}

    return ALIGN_N(sizeof(hashmap_impl_t) + \
        (table_size * sizeof(hashmap_impl_t::table_item_t)) + \
          mpsz + \
        HASH_MAP_MAGIC_BUFF_SIZE, 16);
}
int         hashmap_t::load(int rate) const {
	return used() * rate / buckets();
}
int         hashmap_t::factor() const {
	return buckets() / capacity();
}
int         hashmap_t::hit(int rate) const {
	return impl_->stat_hit_read * rate / impl_->stat_probe_read;
}
int         hashmap_t::collision(int rate) const {
	return impl_->stat_probe_insert * rate / impl_->stat_insert;
}
const char * hashmap_t::layers(::std::string & str) const {
	for (int i = 0; i < impl_->layer; ++i) {
		str.append("[" +
			::std::to_string(impl_->layer_offset[i]) + "," +
			::std::to_string(impl_->layer_size[i]) + ")");
	}
	return str.c_str();
}
const char * hashmap_t::stat(::std::string & str) const {
	str = "memory total size:" + ::std::to_string(impl_->total_size) +
		" Bytes, mempool used:" + ::std::to_string(this->used()) + "/" + ::std::to_string(this->capacity()) +
		", mempool usage:" + ::std::to_string(this->used() * 100 / this->capacity()) +
		"%, load:" + ::std::to_string(this->load()) +
		"%, hit rate:" + ::std::to_string(this->hit()) +
		"%, bucket factor:" + ::std::to_string(this->factor()) +
		", collision:" + ::std::to_string(this->collision()) +
		"%, layers size:" + ::std::to_string(impl_->layer);
	return this->layers(str);
}

hashmap_t::hashmap_t() {
	impl_ = nullptr;
}






}
