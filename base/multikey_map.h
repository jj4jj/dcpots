#pragma  once
#include <tuple>
#include <unordered_map>
#include <map>
template<typename KT1, typename KT2,  typename VT,
		 template<typename KT, typename MVT=VT> 
		 class container_map=std::unordered_map >
class multikey_map { //share value
	typedef std::tuple<KT1, KT2, VT>		value_type;
	typedef std::tuple<KT1, KT2, VT>	*	value_pointer;
	container_map<KT1>	k1mp;
	container_map<KT2>	k2mp;
	int insert(const KT1 & k1, const KT2 & k2, const VT & v){
		if (k1mp.find(k1) != k1mp.end()){
			assert(k2mp.find(k2) != k2mp.end());
			return -1;
		}
		assert(k2mp.find(k2) == k2mp.end());
		value_pointer vp = new value_type();
		std::get<0>(*vp) = k1;
		std::get<1>(*vp) = k2;
		std::get<2>(*vp) = v;
		return 0;
	}
	value_pointer find_1st(const KT1 & k1){
		auto it = k1mp.find(k1);
		if (it != k1mp.end()){
			return it->second;
		}
		return nullptr;
	}
	value_pointer find_2nd(const KT2 & k2){
		auto it = k2mp.find(k2);
		if (it != k2mp.end()){
			return it->second;
		}
		return nullptr;
	}
	void	erase_1st(const KT1 & k1){
		auto it = k1mp.find(k1);
		if (it != k1mp.end()){
			auto vp = it->second;
			k2mp.erase(std::get<1>(*vp));
			k1mp.erase(it);
			delete vp;
		}
	}
	void	erase_2nd(const KT2 & k2){
		auto it = k2mp.find(k2);
		if (it != k2mp.end()){
			auto vp = it->second;
			k1mp.erase(std::get<0>(*vp));
			k2mp.erase(it);
			delete vp;
		}
	}
};

