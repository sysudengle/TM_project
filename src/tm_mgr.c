#include "tm_mgr.h"
#include "tm_scope.h"

			word_t				__no_th = 1;
			tran_t*				all_trans[MAX_NO_THREADS] = {0};
__thread    long 				__thread_id = 0;
__thread 	tran_t  			p_trans = {0};
__thread	int					__trap_access = 0;
__thread	int					__first_run = 0;
__thread	int					__hybrid_aborted = 0;
__thread	int					__in_transact = 0;

/**********************************************************************
		ON_BEGIN
**********************************************************************/

jmp_buf* mgr_on_begin()
{
	tm_assert(__trap_access);

	read_set_init( &p_trans.read_set );
	write_set_init( &p_trans.write_set );

	return &(p_trans.jbuf);
}

/**
 * mgr_on_abort comes here
 *
 * reason codes:
 * 0: memory conflict
 * 1: validation failure in hybrid hardware commit phase
 * 2: validation failure in hybrid software commit phase
 */
void mgr_on_abort( int reason )
{
	tm_assert(__trap_access);
	read_set_reset( &p_trans.read_set );
	write_set_reset( &p_trans.write_set );
	__hybrid_aborted = 1;
	// TODO: reason 1: hw validation abort, 2. sw validation abort
	longjmp( p_trans.jbuf, 1 );
}


void mgr_on_init() {
	read_set_init( &p_trans.read_set );
	write_set_init( &p_trans.write_set );
}

void mgr_on_end() {
	read_set_free( &p_trans.read_set );
	write_set_free( &p_trans.write_set );

#ifdef TM_STATS
	stats_print( stdout, stats_get_total(stdout) );
	fflush(stdout);
#endif
}
