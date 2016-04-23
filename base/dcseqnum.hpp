#pragma  once
#include "bit_eval.hpp"
#include <atomic>
namespace dcsutil {
	template<int max_class_2e = 12, int max_hz_2e = 20>
	class sequence_number_t {
		enum {
			MAX_HZ_SEQN = (1 << max_hz_2e) - 1,
			MAX_CLASS_ID = (1 << max_class_2e) - 1,
		};
	public:
		static unsigned long long next(int id = 0){
			assert(sizeof(unsigned long long) >= 8);
			assert(id <= MAX_CLASS_ID);
			static std::atomic<int>	s_circle_seq(0);
			int max_seq_value = MAX_HZ_SEQN;
			s_circle_seq.compare_exchange_strong(max_seq_value, 0);
			int seq = s_circle_seq.fetch_add(1);
			unsigned long long nseq = dcsutil::time_unixtime_s();
			nseq <<= max_class_2e;
			nseq |= id;
			nseq <<= max_hz_2e;
			nseq |= seq;
			return nseq;
		}
	};

}
