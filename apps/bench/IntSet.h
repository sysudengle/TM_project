#ifndef __INTSET_H__
#define __INTSET_H__

#include <iostream>
#include "Benchmark.h"

using std::cerr;
using std::endl;

// common interface for all benchmarks that support the insertion, removal, and
// lookup of a single integer value in a data structure
class IntSet
{
  public:
    virtual bool lookup(int val) const = 0;
    virtual void insert(int val) = 0;
    virtual void remove(int val) = 0;

    // perform a sanity check to ensure that the data structure integrity
    // hasn't been compromised
    virtual bool isSane(void) const = 0;
	virtual void print() const = 0;
    virtual ~IntSet() { }
};

// since we've got two trees, let's put the code here
enum Color { RED, BLACK };

class IntSetBench : public Benchmark
{
    IntSet* S;
    int M;
  public:
    IntSetBench(IntSet* s, int m) : S(s), M(m) { }
    void random_transaction(thread_args_t* args, unsigned int* seed,
                            unsigned int val,    unsigned int chance);
    bool sanity_check() const;
    // all of the intsets have the same verification code
    virtual bool verify(VerifyLevel_t v);
};

inline
void IntSetBench::random_transaction(thread_args_t* args, unsigned int* seed,
                                     unsigned int val,    unsigned int chance)
{
     unsigned int j = val % M;
    int action = chance;

    if (action < BMCONFIG.lThresh) {
        if (S->lookup(j)) {
            ++args->count[TXN_LOOKUP_TRUE];
        }
        else {
            ++args->count[TXN_LOOKUP_FALSE];
        }
    }
    else if (action < BMCONFIG.iThresh) {
        S->insert(j);
        ++args->count[TXN_INSERT];
    }
    else {
        S->remove(j);
        ++args->count[TXN_REMOVE];
    }
}

inline bool IntSetBench::sanity_check() const
{
	return S->isSane();
}

// verify() is a method for making sure that the data structure works as
// expected
inline bool IntSetBench::verify(VerifyLevel_t v)
{
    // always do the light verification:
	std::cerr << "Sanity check after stage 0!" << std::endl;
    int N = 256;
    for (int k = 1; k <= N; k++) {
        for (int i = k; i <= N; i += k) {
            if (S->lookup(i))
                S->remove(i);
            else
                S->insert(i);
            if (!S->isSane()) return false;
        }
    }
	std::cerr << "Sanity check after stage 1!" << std::endl;
	
    int j = 1;
    for (int k = 1; k <= N; k++) {
        if (k == j*j) {
            if (!S->lookup(k)) return false;
            j++;
        } else {
            if (S->lookup(k)) return false;
        }
    }
    // clean out the data structure
	std::cerr << "Sanity check after stage 2!" << std::endl;
    for (int k = 1; k <= N; k++) {
        S->remove(k);
    }

	std::cerr << "Sanity check after stage 3!" << std::endl;

    // maybe do the heavy verification (1M random ops):
    if (v == HEAVY) {
        bool val[256];
        for (int i = 0; i < 256; i++) {
            val[i] = false;
        }
        int op_count = 0;
        for (int i = 0; i < 1000000; i++) {
            if (++op_count % 10000 == 0) {
                cerr << ".";
                // op_count = 0;
            }
            int j = rand() % 256;
            int k = rand() % 3;
            if (k == 0) {
                val[j] = false;
                S->remove(j);
            }
            else if (k == 1) {
                val[j] = false;
                S->remove(j);
            }
            else {
                if (S->lookup(j) != val[j]) {
                    std::cerr << "Value error!" << std::endl;
                    return false;
                }
            }
            if (!S->isSane()) {
                std::cerr << "Sanity check error!" << std::endl;
                return false;
            }
        }
    }
    cerr << endl;

std::cerr << "Sanity check after stage 4!" << std::endl;
    return true;
}

#endif // __INTSET_H__
