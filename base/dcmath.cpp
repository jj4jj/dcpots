
#include "stdinc.h"
#include "dcmath.hpp"
#include "dcbitset.hpp"

namespace dcs {

//////////////////////////////////////////////////////////////////////////////
size_t              prime_n(size_t n, std::unordered_set<size_t> & pn){
    //filter: 2->n;
    pn.clear();
    if (n < 2){
        return 0;
    }
    bits flip_prime_set(n);
    pn.insert(2);
    for (size_t i = 3; i <= n; ++i){
        if (!flip_prime_set.at(i)){
            flip_prime_set.set(i);
            pn.insert(i);
            for (size_t k = 2; k <= n/i; ++k){
                flip_prime_set.set(k*i);
            }
        }
    }
    return pn.size();
}
bool   is_a_prime(size_t n){
    if (n < 2){ return false; }
	size_t nq = sqrt(n);
    for (size_t i = 2; i < nq; ++i){
        if (n % i == 0){
            return false;
        }
    }
    return true;
}
size_t prime_prev(size_t n){
    --n;
    while (n >= 2){
        if (is_a_prime(n)){
            return n;
        }
        --n;
    }
    return 2;
}
size_t prime_next(size_t n){
    ++n;
    while (n != 0){
        if (is_a_prime(n)){
            return n;
        }
        ++n;
    }
    return 2;
}






}