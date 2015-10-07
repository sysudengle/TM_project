#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

#include <iostream>
#include <string>

#include "../../src/tm.h"
#include "../../src/utils/hrtime.h"


inline void argError(std::string reason)
{
    std::cerr << "Argument Error: " << reason << std::endl;
    exit(-1);
}

struct BenchmarkConfig
{
    int duration;                       // in seconds
    int datasetsize;                    // number of items
    int threads;
    int verbosity;                      // in {0, 1, 2}, lower => less output
    bool verify;
    std::string bm_name;
    // these three are for getting various lookup/insert/remove ratios
    const int NUM_OUTCOMES;
    int lThresh;
    int iThresh;

    // warmup and execute are used when we aren't using a duration but instead
    // are testing the code for a fixed number of transactions
    int warmup;
    int execute;
    char unit_testing;

    BenchmarkConfig()
        : duration(5), datasetsize(256), threads(2), verbosity(1),
          verify(true), bm_name("Counter"),
          NUM_OUTCOMES(30), lThresh(10), iThresh(20),
          warmup(0), execute(0), unit_testing(' ') { }

    void verifyParameters();
    void printConfig();
} __attribute__ ((aligned(64)));

// BMCONFIG is declared in BenchMain.cpp
extern BenchmarkConfig BMCONFIG;

enum TxnOps_t { TXN_ID,          TXN_GENERIC,      TXN_INSERT, TXN_REMOVE,
                TXN_LOOKUP_TRUE, TXN_LOOKUP_FALSE, TXN_NUM_OPS};
enum VerifyLevel_t { LIGHT, HEAVY };

class Benchmark;
struct thread_args_t
{
    int id;
    Benchmark* b;
    int duration;
    int threads;
    long count[TXN_NUM_OPS];
    int warmup;
    int execute;
} __attribute__ ((aligned(64)));

// all benchmarks have the same interface
class Benchmark
{
  public:
    virtual void random_transaction(thread_args_t* args, unsigned int* seed,
                                    unsigned int val, unsigned int chance) = 0;
    virtual bool sanity_check() const = 0;
    void measure_speed();
    virtual bool verify(VerifyLevel_t v) = 0;
    virtual ~Benchmark() { }
};

#endif // __BENCHMARK_H__
