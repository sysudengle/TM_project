#include "hrtime.h"

#include "tm_stats.h"
#include "../tm_mgr.h"


void cstats_init( cstats_t* csts )
{
	// transaction start counters
	csts->n_start_total= 0;

	csts->n_start_hw= 0;
	csts->n_start_hybrid= 0;
	csts->n_start_hybrid_commit_hw= 0;
	csts->n_start_hybrid_commit_sw= 0;
	csts->n_start_sw= 0;

	// transaction commit counters
	csts->n_commit_hw= 0;
	csts->n_commit_hybrid_hw= 0;
	csts->n_commit_hybrid_sw= 0;
	csts->n_commit_sw= 0;

	// abort counters
	csts->n_abort_conflicts_hw= 0;
	csts->n_abort_other_hw= 0;
	csts->n_abort_conflicts_hybrid_hw= 0;
	csts->n_abort_other_hybrid_hw= 0;
	csts->n_abort_invalidations_hybrid_hw= 0;
	csts->n_abort_invalidations_hybrid_sw= 0;

	// access counters
	csts->n_reads= 0;
	csts->n_writes= 0;

	csts->begin = 0;
}

void cstats_begin( cstats_t* csts )
{
	#ifdef TIMINGS
	csts->begin = get_c();
	#endif
}

tstats_t stats_get_total(FILE* f)
{
	unsigned int i;
	tstats_t tot;

	tot.n_total = 0;
	tot.n_hw = 0;
	tot.n_hybrid = 0;
	tot.n_hybrid_hw = 0;
	tot.n_hybrid_sw = 0;
	tot.n_sw = 0;
	tot.n_start_hybrid_hw = 0;
	tot.n_start_hybrid_sw = 0;
	tot.n_start_sw = 0;

	tot.n_abort_hw_conflicts = 0;
	tot.n_abort_hw_other = 0;

	tot.n_abort_hybrid_hw_conflict = 0;
	tot.n_abort_hybrid_hw_invalidation = 0;
	tot.n_abort_hybrid_hw_other = 0;

	tot.n_abort_hybrid_sw_invalidation = 0;

	tot.n_reads = 0;
	tot.n_writes = 0;

	fprintf( f, "\n\n\n============= TM STATS ===============\n");
	fprintf( f, "\n------------- Transactions ---------------\n");
	fprintf( f, "thread #,started,hw,hybrid_hw,hybrid_sw,sw\n");

	for( i = 1; i < __no_th; i++ )
	{
		// transaction start counters
		tot.n_total += all_trans[ i ]->stats.n_start_total;

		tot.n_hw += all_trans[ i ]->stats.n_commit_hw;
		tot.n_hybrid_hw += all_trans[ i ]->stats.n_commit_hybrid_hw;
		tot.n_hybrid_sw += all_trans[ i ]->stats.n_commit_hybrid_sw;
		tot.n_hybrid = tot.n_hybrid_hw + tot.n_hybrid_sw;
		tot.n_sw += all_trans[ i ]->stats.n_commit_sw;
		tot.n_start_sw += all_trans[ i ]->stats.n_start_sw;

		tot.n_start_hybrid_hw += all_trans[ i ]->stats.n_start_hybrid_commit_hw;
		tot.n_start_hybrid_sw += all_trans[ i ]->stats.n_start_hybrid_commit_sw;

		tot.n_abort_hw_conflicts += all_trans [ i ]->stats.n_abort_conflicts_hw;
		tot.n_abort_hw_other += all_trans [ i ]->stats.n_abort_other_hw;

		tot.n_abort_hybrid_hw_conflict += all_trans [ i ]->stats.n_abort_conflicts_hybrid_hw;
		tot.n_abort_hybrid_hw_invalidation += all_trans [ i ]->stats.n_abort_invalidations_hybrid_hw;
		tot.n_abort_hybrid_hw_other += all_trans [ i ]->stats.n_abort_other_hybrid_hw;

		tot.n_abort_hybrid_sw_invalidation += all_trans [ i ]->stats.n_abort_invalidations_hybrid_sw;

		// access counters
		tot.n_reads += all_trans[ i ]->stats.n_reads;
		tot.n_writes += all_trans[ i ]->stats.n_writes;

		fprintf( f, "%u,%llu,%llu,%llu,%llu,%llu\n", i-1, all_trans[ i ]->stats.n_start_total,
				all_trans[ i ]->stats.n_commit_hw,all_trans[ i ]->stats.n_commit_hybrid_hw,
				all_trans[ i ]->stats.n_commit_hybrid_sw, all_trans[ i ]->stats.n_commit_sw);
	}

	return tot;
}

void stats_print( FILE* f, tstats_t sts )
{
	fprintf( f, "total,%llu,%llu,%llu,%llu,%llu\n", sts.n_total,
			sts.n_hw,sts.n_hybrid_hw, sts.n_hybrid_sw, sts.n_sw);

	fprintf( f, "\n\n------------- HW Transactions ---------------\n");
	fprintf( f, "total_hw_attempt,commit,abort_conflicts,abort_others\n");
	fprintf( f, "%llu,%llu,%llu,%llu\n", sts.n_total, sts.n_hw, sts.n_abort_hw_conflicts, sts.n_abort_hw_other);

	fprintf( f, "\n\n------------- Hybrid_HW Transactions ---------------\n");
	fprintf( f, "total_hybrid_hw_attempt,commit,abort_conflicts,abort_invalidations,abort_others\n");
	fprintf( f, "%llu,%llu,%llu,%llu,%llu\n", sts.n_start_hybrid_hw, sts.n_hybrid_hw,
						sts.n_abort_hybrid_hw_conflict, sts.n_abort_hybrid_hw_invalidation
						, sts.n_abort_hybrid_hw_other);

	fprintf( f, "\n\n------------- Hybrid_SW Transactions ---------------\n");
	fprintf( f, "total_hybrid_sw_attempt,commit,abort_invalidation\n\n");
	fprintf( f, "%llu,%llu,%llu\n", sts.n_start_hybrid_sw,
			sts.n_hybrid_sw, sts.n_abort_hybrid_sw_invalidation);

	fprintf( f, "\n\n------------- SW Serialized ---------------\n");
	fprintf( f, "total_sw_attempt,commit\n\n");
	fprintf( f, "%llu,%llu\n", sts.n_start_sw, sts.n_sw);

}

void stats_print_v( FILE* f, tstats_t sts )
{
}
