#pragma once
#include <unordered_set>

namespace dcs {
size_t              prime_n(size_t n, std::unordered_set<size_t> & pn);
size_t              prime_next(size_t n);
size_t              prime_prev(size_t n);
bool                is_a_prime(size_t n);

///////////////////////////////////////////////////////////////////////////////////////
template<size_t n, size_t i>
struct is_prime_loop_i {
    enum {
        value = ( (n%i) == 0 )? 0 : is_prime_loop_i<n, i - 1>::value,
    };
};

template<size_t n>
struct is_prime_loop_i<n, 2> {
    enum { value = n % 2, };
};

template<size_t n>
struct is_prime {
    enum {
        value = is_prime_loop_i<n, n / 2>::value,
    };
};

template<size_t n, bool bigger = true>
struct next_prime {
    enum { value = is_prime<n + 1>::value ? n + 1 : next_prime<n + 1, bigger>::value };
};
template<size_t n>
struct next_prime<n, false> {
    enum { value = is_prime<n - 1>::value ? n - 1 : next_prime<n - 1, false>::value };
};

template<size_t n, size_t m, bool bigger=true>
struct n_next_prime_sum {
    typedef next_prime<n, bigger>   next_prime_v;
    enum {
        value = next_prime_v::value +
        n_next_prime_sum<next_prime_v::value, m - 1>::value,
    };
};

template<size_t n, bool bigger>
struct n_next_prime_sum<n, 0, bigger> {
    enum { value = 0 };
};


}