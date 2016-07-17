#pragma  once
#include <type_traits>
#include "dcbitset.hpp"
#include <atomic>
#include <cstdio>
namespace dcsutil {
    struct sequence_type {
    };
    template<int max_class_2e = 6, int max_hz_2e = 26, bool multi_thread = false, class SEQT = sequence_type>
	class sequence_number_t {
		enum {
			MAX_HZ_SEQN = (1 << max_hz_2e) - 1,
			MAX_CLASS_ID = (1 << max_class_2e) - 1,
		};
	public:
		static unsigned long long next(int id=0){
			typename std::conditional<multi_thread, int, char>::type val = 0;
			return next_dispatch(id, val);
		}
	private:
		//type dispatch (overload)
		static unsigned long long next_dispatch(int id, int){
			assert(sizeof(unsigned long long) >= 8);
			assert(id <= MAX_CLASS_ID);
			static std::atomic<int>	s_circle_seq(0);
			static std::atomic<uint32_t> s_sec_timestamp(0);
			int cmp_seq_value = MAX_HZ_SEQN;
			s_circle_seq.compare_exchange_strong(cmp_seq_value, 0);
			int seq = s_circle_seq.fetch_add(1);
			if (seq == 1 || s_sec_timestamp == 0){
				s_sec_timestamp = dcsutil::time_unixtime_s();
			}
			unsigned long long nseq = s_sec_timestamp;
			nseq <<= max_class_2e;
			nseq |= id;
			nseq <<= max_hz_2e;
			nseq |= seq;
			return nseq;
		}
		static unsigned long long next_dispatch(int id, char){
			assert(sizeof(unsigned long long) >= 8);
			assert(id <= MAX_CLASS_ID);
			static int	s_circle_seq{ 0 };
			static uint32_t s_sec_timestamp{ 0 };
			if (s_circle_seq == MAX_HZ_SEQN){
				s_circle_seq = 0;
			}
			++s_circle_seq;
			if (s_circle_seq == 1 || s_sec_timestamp == 0){
				s_sec_timestamp = dcsutil::time_unixtime_s();
			}
			unsigned long long nseq = s_sec_timestamp;
			nseq <<= max_class_2e;
			nseq |= id;
			nseq <<= max_hz_2e;
			nseq |= s_circle_seq;
			//printf("%u-%d\n", s_sec_timestamp, s_circle_seq);
			return nseq;
		}
	};

}
