#ifndef TM_STATS_H_
#define TM_STATS_H_

#include "../tm_general.h"

//#define TIMINGS

typedef struct _cstats_t
{
	// transaction start counters
	volatile unsigned long long n_start_total;

	volatile unsigned long long n_start_hw;
	volatile unsigned long long n_start_hybrid;
	volatile unsigned long long n_start_hybrid_commit_hw;
	volatile unsigned long long n_start_hybrid_commit_sw;
	volatile unsigned long long n_start_sw;

	// transaction commit counters
	volatile unsigned long long n_commit_hw;
	volatile unsigned long long n_commit_hybrid_hw;
	volatile unsigned long long n_commit_hybrid_sw;
	volatile unsigned long long n_commit_sw;

	// abort counters
	volatile unsigned long long n_abort_conflicts_hw;
	volatile unsigned long long n_abort_other_hw;
	volatile unsigned long long n_abort_conflicts_hybrid_hw;
	volatile unsigned long long n_abort_other_hybrid_hw;
	volatile unsigned long long n_abort_invalidations_hybrid_hw;
	volatile unsigned long long n_abort_invalidations_hybrid_sw;

	// access counters
	// TODO: might need to put this off for now since we don't want to impact
	// the HW transactions
	volatile unsigned long long	n_reads;
	volatile unsigned long long	n_writes;

	// timers
	volatile unsigned long long begin;
} cstats_t;

#ifdef TM_STATS
# define 	cstats_inc(counter)					++counter
#else
# define	cstats_inc(counter)
#endif

typedef struct _tstats_t
{
	volatile unsigned long long	n_total;
	volatile unsigned long long	n_hw;
	volatile unsigned long long	n_hybrid; // no. of aborts caused by invalidation = no of aborts - no of aborts caused by deadlocks
	volatile unsigned long long n_hybrid_hw;
	volatile unsigned long long n_hybrid_sw;
	volatile unsigned long long	n_sw;
	volatile unsigned long long n_start_hybrid_hw;
	volatile unsigned long long n_start_hybrid_sw;
	volatile unsigned long long n_start_sw;

	volatile unsigned long long n_abort_hw_conflicts;
	volatile unsigned long long n_abort_hw_other;

	volatile unsigned long long n_abort_hybrid_hw_conflict;
	volatile unsigned long long n_abort_hybrid_hw_invalidation;
	volatile unsigned long long n_abort_hybrid_hw_other;

	volatile unsigned long long n_abort_hybrid_sw_invalidation;

	volatile unsigned long long	n_reads; // no of commited reads
	volatile unsigned long long	n_writes; // no of commited writes
} tstats_t;


void cstats_init( cstats_t* csts );
void cstats_begin( cstats_t* csts );

tstats_t stats_get( int _tid );
tstats_t stats_get_total(FILE* f);
void stats_print( FILE* f, tstats_t sts );

#endif /*TM_STATS_H_*/
