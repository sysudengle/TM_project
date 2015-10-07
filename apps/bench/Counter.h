// POSSIBILITY OF SUCH DAMAGE.

#ifndef __BENCH_COUNTER_H__
#define __BENCH_COUNTER_H__

#include "Benchmark.h"

namespace bench
{
    class CounterBench : public Benchmark
    {
      private:

        tm_int m_counter;

      public:

        CounterBench() : m_counter(0){ }


        void random_transaction(thread_args_t* args,
                                unsigned int* seed,
                                unsigned int val,
                                unsigned int chance)
        {
			int _val;
            BEGIN_TRANSACTION();
            //m_counter = m_counter+1;
			m_counter += 1;
			_val = m_counter;
            COMMIT_TRANSACTION();
        }


        bool sanity_check() const
        {
            // not as useful as it could be...
            int val = 0;
            BEGIN_TRANSACTION();
            val = m_counter;
            COMMIT_TRANSACTION();
			std::cout << "val: " << val << std::endl;
            return (val >= 0);
        }


        // no data structure verification is implemented for the Counter...
        // yet
        virtual bool verify(VerifyLevel_t v) {
            return true;
        }
    };

}   // namespace bench

#endif  // __BENCH_COUNTER_H__
