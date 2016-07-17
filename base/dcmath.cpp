
#include "stdinc.h"
#include "dcmath.hpp"
namespace dcsutil {

//////////////////////////////////////////////////////////////////////////////
size_t              prime_n(std::vector<size_t> & pn){
    return pn.size();
}
bool   is_a_prime(size_t n){
    if (n < 2){ return false; }
    for (size_t i = 2; i < n / 2; ++i){
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