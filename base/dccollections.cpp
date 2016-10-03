#include "logger.h"
#include "dcmath.hpp"
#include "dccollections.hpp"
///////////////////////////////////////////////////////////////////////////////////////////
#define MMPOOL_MAGIC	("mmpool-magic")
#define DIV_CEIL(s,n)		(((s)+(n-1))/n)
#define ALIGN_N(s,n)		(((s)+(n-1))/n*n)

///////////////////////////////////////////////////////////////////////////////////////////
namespace dcs {
struct mempool_impl_t {
	mempool_conf_t::strategy    stg;
	size_t						data_size;
	size_t						block_size;
	size_t						block_max;
	char						magic[16];
	size_t                      used;
	size_t						last_alloc_id;
	size_t						head_begin;
	size_t						data_begin;
	size_t						data_end;
};
union mempool_impl_head {
	///////////////////////////////////////////
	uint64_t				bmp[1];
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
mempool_conf_t::mempool_conf_t(){
    data = nullptr;
    data_size = block_size = block_max = 0;
    stg = MEMPOOL_STRATEGY_BITMAP;
    attach = false;
}
static inline int mmpool_bitmap_init(uint64_t bmp[], size_t blksz, size_t nblk, bool attach) {
	return 0;
}
static inline int mmpool_blklist_init(mempool_impl_head::block_list_t * bkl, size_t blksz, size_t nblk, bool attach) {
	if (!attach){
		memset(bkl, 0, sizeof(*bkl));
		bkl->free.front = bkl->free.rear = 0;
		bkl->busy.front = bkl->busy.rear = 0;
		//init construct a linked list free
		bkl->list[0].next = 1;
		bkl->list[0].prev = nblk-1;
		for (size_t i = 1; i < nblk; ++i) {
			bkl->list[i].prev = i-1;
			bkl->list[i].next = (i+1)%nblk;
		}
	}
	return 0;
}
int     mempool_t::init(const mempool_conf_t & conf){
	size_t total_size = size(conf.stg, conf.block_max, conf.block_size);
	if (total_size == 0 || conf.data_size < total_size) {
		GLOG_ERR("config data size :%zu need %zu error !", conf.data_size, total_size);
		return -1;
	}
	impl_ = (mempool_impl_t*)conf.data;
	mempool_impl_head * mphead = (mempool_impl_head*)&(impl_[1]);
	if (conf.attach) {
		if (!strcmp(MMPOOL_MAGIC, impl_->magic)) {
			GLOG_ERR("mmpool magic head error [%s] !", impl_->magic);
			return -1;
		}
		if (!strcmp(MMPOOL_MAGIC, (char *)impl_ + impl_->data_end)) {
			GLOG_ERR("mmpool magic tail error [%s] !", (char *)impl_ + impl_->data_end);
			return -1;
		}
	}
	else {
		memset(impl_, 0, sizeof(*impl_));
		impl_->block_max = conf.block_max;
		impl_->data_size = conf.data_size;
		impl_->block_size = conf.block_size;
		impl_->stg = conf.stg;
		impl_->head_begin = sizeof(mempool_impl_t);
		if (conf.stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
			impl_->data_begin = impl_->head_begin + sizeof(uint64_t)*DIV_CEIL(conf.block_max,64);
		}
		else if (conf.stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
			impl_->data_begin = impl_->head_begin + \
				sizeof(mempool_impl_head::block_list_t)+\
				sizeof(mempool_impl_head::block_list_t::entry_t)*(conf.block_max-1);
		}
		else {
			assert("not support now !" && false);
		}
		impl_->data_end = impl_->data_begin + conf.block_max*conf.block_size;
		strcpy(impl_->magic, MMPOOL_MAGIC);
		strcpy((char *)impl_ + impl_->data_end, MMPOOL_MAGIC);
	}
	if (conf.stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
		return mmpool_bitmap_init(mphead->bmp, conf.block_size, conf.block_max, conf.attach);
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
		size_t idx = impl_->last_alloc_id/64;
		uint64_t * bmp = ((mempool_impl_head*)(&impl_[1]))->bmp;
		size_t bitmap_count = impl_->block_max/64;
		//find first 0
		size_t ffo = 0;
		for (size_t i = 0; i < bitmap_count; ++i, idx = (idx+1)% bitmap_count) {			
			if ((ffo = __builtin_ffsll(~(bmp[idx])))) {
				break;
			}
		}
		assert(ffo);
		p = (char*)impl_ + impl_->data_begin + (idx*64+ffo)*impl_->block_size;
		bmp[idx] |= (1ULL << (ffo - 1));//set 1
	}
	else if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
		//free head alloc
		mempool_impl_head::block_list_t * bkl = &((mempool_impl_head*)(&impl_[1]))->bkl;
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
	}
	if (p) {
		++impl_->used;
		impl_->last_alloc_id = this->id(p);
	}
    return p;
}
void    mempool_t::free(void * p){
	int ret = -1;
	if (impl_->used == 0) {
		return;
	}
	size_t zid = this->id(p);
	if (zid == 0) {
		return;
	}
	size_t idx = zid - 1;
	if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
		uint64_t * bmp = ((mempool_impl_head*)(&impl_[1]))->bmp;
		uint64_t  mask = (1ULL << (idx % 64));
		if (bmp[idx / 64] & mask) {
			bmp[idx / 64] &= ~(mask);
			ret = 0;
		}
		else {
			ret = -1;
		}
	}
	else if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
		mempool_impl_head::block_list_t * bkl = &((mempool_impl_head*)(&impl_[1]))->bkl;
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
		}
		else {
			ret = -1;
		}
	}
	if (0 == ret) {
		--impl_->used;
	}
}
void *  mempool_t::ptr(size_t id){
	if (impl_->used == 0 || id == 0 || id > impl_->block_max) {
		return nullptr;
	}
	return (char*)impl_ + impl_->data_begin + (id-1)*impl_->block_size;
}
size_t  mempool_t::id(void * p) const {
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
	if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
		uint64_t * bmp = ((mempool_impl_head*)(&impl_[1]))->bmp;
		size_t blkid = this->id(blk);//+1-1 . NEXT
		size_t blkidx = blkid/64;
		size_t bitidx = blkid%64;
		size_t bitmap_count = impl_->block_max / 64;
		for (; blkidx < bitmap_count; ++blkidx) {
			for (int i = bitidx; i < 64; ++i) {
				if (bmp[blkid] & (1ULL << i)) {
					return this->ptr(blkidx*64+i+1);
				}
			}
		}
		return nullptr;
	}
	else if (impl_->stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
		mempool_impl_head::block_list_t * bkl = &((mempool_impl_head*)(&impl_[1]))->bkl;
		size_t blkidx = this->id(blk) - 1;
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
	if (stg == mempool_conf_t::MEMPOOL_STRATEGY_BITMAP) {
		return ALIGN_N(sizeof(mempool_impl_t) + nblk* blksz + 16 +\
			sizeof(uint64_t) * DIV_CEIL(nblk, 64), 16);
	}
	else if (stg == mempool_conf_t::MEMPOOL_STRATEGY_BLKLST) {
		return ALIGN_N(sizeof(mempool_impl_t) + nblk* blksz + 16 +\
			sizeof(mempool_impl_head::block_list_t) + \
			sizeof(mempool_impl_head::block_list_t::entry_t)*(nblk-1), 16);
	}
	else {		
		return 0;
	}
}
const char * mempool_t::stat(::std::string & str) const {
	str += "bytes size:" + ::std::to_string(impl_->data_size) +
		" used:" + ::std::to_string(this->used()) + "/" + ::std::to_string(this->capacity()) +
		" usage:" + ::std::to_string(this->used() * 100 / this->capacity());
	return str.c_str();
}

mempool_t::mempool_t(){
    impl_ = nullptr;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
hashmap_conf_t::hashmap_conf_t() {
	data = nullptr;
    init = nullptr;
    hash = nullptr;
    comp = nullptr;
	data_size = block_max = block_size = 0;
	attach = false;
	layer = 3;
}
#define  HASHMAP_MAGIC		("hashmap-magic")
#define HASHMAP_MAX_LAYER	(11)
//multi layer hash
//cmax(count)
struct hashmap_impl_t {
	hashmap_conf_t::block_init  init;
	hashmap_conf_t::block_hash  hash;
	hashmap_conf_t::block_comp  comp;
	mempool_t					mmpool;
	char						magic[16];
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
	};
	struct layout_desc {
		table_item_t			table[1];
		char					mmpool[1];
		char					magic[16];
	};
};
int         hashmap_t::init(const hashmap_conf_t & conf){
	size_t total_size = size(conf.layer, conf.block_max, conf.block_size);
	if (total_size == 0 || conf.data_size < total_size) {
		GLOG_ERR("total size:%zu data size:%zu", total_size, conf.data_size);
		return -1;
	}
	impl_ = (hashmap_impl_t *)conf.data;
	if (conf.attach) {
		impl_->comp = conf.comp;
		impl_->init = conf.init;
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
		impl_->init = conf.init;
		impl_->hash = conf.hash;
		impl_->total_size = total_size;
		impl_->block_count = conf.block_max;
		impl_->block_size = conf.block_size;
		impl_->stat_hit_read = impl_->stat_insert = impl_->stat_probe_insert = \
		impl_->stat_probe_read = 1;
		size_t prime_n = dcs::prime_next(2*conf.block_max);
		size_t table_size = prime_n;
		impl_->layer = conf.layer;
		impl_->layer_size[conf.layer - 1] = prime_n;
		for (int i = conf.layer-2;i >= 0; --i) {
			prime_n = dcs::prime_prev(prime_n);
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
	}
	mempool_conf_t mpc;
	mpc.stg = mempool_conf_t::MEMPOOL_STRATEGY_BITMAP;
	mpc.block_max = conf.block_max;
	mpc.block_size = conf.block_size;
	mpc.attach = conf.attach;
	mpc.data = (char*)impl_ + impl_->mmpool_start;
	mpc.data_size = conf.data_size - impl_->mmpool_start;
    return impl_->mmpool.init(mpc);
}
void *      hashmap_t::insert(const void * blk, bool unique){
	if (unique) {
		void * p = find(blk);
		if (p) {
			return nullptr;
		}
	}
	void * p = impl_->mmpool.alloc();
	if (!p) {
		return nullptr;
	}
	hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
	size_t id = impl_->mmpool.id(p);
	size_t code = impl_->hash(blk);
	size_t lidx = 0;
	bool inserted = false;
	for (int i = 0;i < impl_->layer; ++i) {
		lidx = impl_->layer_offset[i] + code % impl_->layer_size[i];
		++impl_->stat_probe_insert;
		if (table[lidx].id == 0) {
			table[lidx].id = id;
			table[lidx].code = code;
			inserted = true;
			break;
		}
	}
	if (!inserted) {
		//last layer
		size_t retry = capacity();
		while (table[lidx].id && retry-->0) {
			 ++lidx;
			 ++impl_->stat_probe_insert;
			 if (lidx >= impl_->layer_offset[impl_->layer - 1] + impl_->layer_size[impl_->layer - 1]) {
				 lidx = impl_->layer_offset[impl_->layer - 1];
			 }
		}
		if (table[lidx].id) {
			impl_->mmpool.free(p);
			p = nullptr;
			assert("no allocated space !" && false);
		}
		else {
			table[lidx].id = id;
			table[lidx].code = code;
		}
	}
	if (p) {
		memcpy(p, blk, impl_->block_size);
		++impl_->stat_insert;
		if (impl_->init) {
			impl_->init(p);
		}
	}
    return p;
}
static inline size_t _list_tail(hashmap_impl_t * impl_, size_t code) {
	size_t lidx = -1, plidx = -1;
	hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
	for (int i = 0; i < impl_->layer; ++i) {
		plidx = lidx;
		lidx = impl_->layer_offset[i] + code % impl_->layer_size[i];
		if (table[lidx].id == 0) {
			return plidx;
		}
	}
	//last layer
	size_t retry = impl_->mmpool.capacity();
	while (table[lidx].id && retry-->0) {
		plidx = lidx;
		++lidx;
		if (lidx >= impl_->layer_offset[impl_->layer - 1] + impl_->layer_size[impl_->layer - 1]) {
			lidx = impl_->layer_offset[impl_->layer - 1];
		}
	}
	if (table[lidx].id) {
		assert("no allocated space !" && false);
		return -1;
	}
	else {
		return plidx;
	}
}
static inline void * _list_find(size_t & lidx, hashmap_impl_t * impl_, size_t code, const void * blk) {
	size_t retry = impl_->mmpool.capacity();
	hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
	int last_layer_idx = impl_->layer - 1;
	lidx = impl_->layer_offset[last_layer_idx] + code % impl_->layer_size[impl_->layer-1];
	while (retry-- > 0 && table[lidx].id > 0) {
		++impl_->stat_probe_read;
		if (table[lidx].code == code) {
			void * p = impl_->mmpool.ptr(table[lidx].id);
			if (0 == impl_->comp(blk, p)) {
				++impl_->stat_hit_read;
				return p;
			}
		}
		++lidx;
		if (lidx >= impl_->layer_offset[last_layer_idx] + impl_->layer_size[last_layer_idx]) {
			lidx = impl_->layer_offset[last_layer_idx];
		}
	}
	return nullptr;
}
static inline void * _find(size_t & layer, size_t & lidx, hashmap_impl_t * impl_, const void * blk) {
	size_t code = impl_->hash(blk);
	hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
	lidx = 0;
	layer = 0;
	for (int i = 0;i < impl_->layer; ++i) {
		lidx = impl_->layer_offset[i] + code % impl_->layer_size[i];
		++impl_->stat_probe_read;
		if (table[lidx].id) {
			if (table[lidx].code == code) {
				void * p = impl_->mmpool.ptr(table[lidx].id);
				if (0 == impl_->comp(blk, p)) {
					layer = i;
					return p;
				}
			}
		}
		else {
			return nullptr;
		}
	}
	assert(lidx >= impl_->layer_offset[impl_->layer - 1]);
	layer = impl_->layer-1;
	return _list_find(lidx, impl_, code, blk);
}

void *      hashmap_t::find(const void * blk){
	size_t lidx, layer;
    return _find(layer, lidx, impl_, blk);
}
void        hashmap_t::remove(const void * blk){
	size_t lidx, layer;
	void * p = _find(layer, lidx, impl_, blk);
	if (!p) {
		return ;
	}
	hashmap_impl_t::table_item_t * table = (hashmap_impl_t::table_item_t*)((char*)impl_ + impl_->table_start);
	size_t code = impl_->hash(blk);
	size_t ltidx = _list_tail(impl_, code);
	assert(ltidx != (size_t)-1);
	if (ltidx != lidx) {
		table[lidx] = table[ltidx];
	}
	table[ltidx].id = 0;
	table[ltidx].code = 0;
	impl_->mmpool.free(p);
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
size_t      hashmap_t::size(int layer, size_t nblk, size_t blksz){
	if (layer < 1 || layer > HASHMAP_MAX_LAYER) {
		return 0;
	}
	size_t mpsz = mempool_t::size(mempool_conf_t::MEMPOOL_STRATEGY_BITMAP, nblk, blksz);
	if (mpsz == 0) {
		return 0;
	}
	size_t prime_n = dcs::prime_next(2*nblk);
	size_t table_size = prime_n;
	for (int i = 1; i < layer; ++i) {
		prime_n = dcs::prime_prev(prime_n);
		table_size += prime_n;
	}
	mpsz += table_size*sizeof(hashmap_impl_t::table_item_t);
	mpsz += sizeof(hashmap_impl_t);
    return mpsz;
}
int         hashmap_t::load(int rate) const {
	return used() * rate / buckets();
}
int         hashmap_t::factor() const {
	return capacity() * 100 / buckets();
}
int         hashmap_t::hit(int rate) const {
	return impl_->stat_hit_read * rate / impl_->stat_probe_read;
}
int         hashmap_t::collision() const {
	return impl_->stat_probe_insert / impl_->stat_insert;
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
	str += "mbytes size:" + ::std::to_string(impl_->total_size) +
		" mused:" + ::std::to_string(this->used()) + "/" + ::std::to_string(this->capacity()) +
		" musage:" + ::std::to_string(this->used() * 100 / this->capacity()) +
		" iload:" + ::std::to_string(this->load()) +
		" ihit:" + ::std::to_string(this->hit()) +
		" ifactor:" + ::std::to_string(this->factor()) +
		" icollision:" + ::std::to_string(this->collision()) +
		" ilayers:" + ::std::to_string(impl_->layer);
	return this->layers(str);
}

hashmap_t::hashmap_t() {
	impl_ = nullptr;
}






}
