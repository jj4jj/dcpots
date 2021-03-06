#pragma once
#include <bitset>
#include <vector>
#include <cmath>

namespace dcs {
    struct bits {
        void    set(size_t pos, bool bv = true);
        bool    at(size_t pos);
        std::vector<size_t> nvbits;
        bits(size_t n);
    };
    
    ///////////////////////////////////////////////////
    template <unsigned long long N>
	struct bit_count1 {
		enum {
			value = 1 + bit_count1<N&(N - 1)>::value
		};
	};
	template<>
	struct bit_count1<1> {
		enum {
			value = 1
		};
	};
	template<>
	struct bit_count1<0> {
		enum {
			value = 0
		};
	};

	////////////////////////////////////////////
	template<class type = int>
	static inline int bit_count(type n){
		return bit_count1<n>::value;
	}
	static inline int bit_ceil(unsigned long long n){
		double be = log2(n);
		if (be > int(be)){
			return int(be) + 1;
		}
		else {
			return int(be);
		}
	}
	static inline int bit_floor(unsigned long long n){
		return int(log2(n));
	}

}

