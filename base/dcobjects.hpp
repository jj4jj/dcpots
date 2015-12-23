#pragma once

NS_BEGIN(dcsutil)

//object pool

template<class T, int MAX_NUM = 1024>
class object_pool_t {
public:
	typedef	std::array<T, MAX_NUM>			pool_t;
	typedef typename pool_t::pointer		pointer;
	typedef std::unordered_set<uint64_t>	pool_hash_t;
private:
	pool_t             pool;
	pool_hash_t        free_pool;
	int                num;
public:
	object_pool_t(){
		num = 0;
		for (int i = 1; i < MAX_NUM; ++i){
			free_pool.insert(i);
		}
	}
	pointer   null(){
		return nullptr;
	}
	bool      is_null(pointer p){
		return p == nullptr;
	}
	size_t    used(){
		return num;
	}
	size_t    total(){
		return MAX_NUM;
	}
	size_t    free(){
		return free_pool.size();
	}
	size_t    alloc(std::mutex & lock){
		std::lock_guard<std::mutex> lock_gurad(lock);
		return genid();
	}
	size_t    alloc(){
		return genid();
	}
	int        free(size_t id){
		if (id > 0 &&
			id <= pool.size() &&
			free_pool.find(id) == free_pool.end()){
			num--;
			assert(num >= 0);
			free_pool.insert(id);
			return 0;
		}
		return -1;
	}
	size_t      id(pointer p){
		if (p == null()){
			return 0;
		}
		return p - &pool[0] + 1;
	}
	pointer     ptr(size_t id){
		if (id > 0 && id <= MAX_NUM &&
			free_pool.find(id) == free_pool.end()){
			return &pool[id - 1];
		}
		return null();
	}
private:
	size_t    genid(){
		if (free_pool.empty()){
			return 0;
		}
		else {
			size_t id = *free_pool.begin();
			free_pool.erase(free_pool.begin());
			++num;
			return id;
		}
	}
};
///============================================================
//object queue (cycle , ring)
template <typename T, size_t MAX>
class object_queue_t {
public:
	typedef typename object_pool_t<T, MAX>::pointer pointer;
private:
	size_t                front_, rear_;
	std::vector<pointer>   q;
	object_pool_t<T, MAX>    pool;
public:
	// [--- front xxxxx rear------]
	// [xxxx rear ---- front xxxxx]
	object_queue_t(){
		q.resize(MAX, null());
		front_ = rear_ = 0;
	}
	pointer null(){
		return pool.null();
	}
	size_t  size(){
		return pool.used();
	}
	pointer push(){
		if (rear_ + 1 == front_ || rear_ == front_ + MAX){//full 
			return pool.null();
		}
		//allocate and push it
		size_t id = pool.alloc();
		auto p = pool.ptr(id);
		if (p != null()){
			q[rear_] = p;
			rear_ = (rear_ + 1) % MAX;
		}
		return p;
	}
	pointer front(){
		if (front_ == rear_){
			return pool.null();
		}
		return q[front_];
	}
	bool empty(){
		return front_ == rear_;
	}
	void pop(){
		if (empty()){
			return;
		}
		auto p = q[front_];
		front_ = (front_ + 1) % MAX;
		pool.free(pool.id(p));
	}
};

NS_END()