#ifndef __tm_scope_h
#define __tm_scope_h

#include <immintrin.h>
#include "atomics/tm_mutex.h"
#include "utils/tm_stats.h"

// Transaction Type
extern __thread		tran_type_t	__txn_type;

/**
 * Common functions
 */
#define	mgr_on_check()
#define	print_version( f_out )		fprintf( f_out, "Hybrid TM" )
#define	set_version( cd_v, cr_v )

/**
 * In transaction functions
 */
jmp_buf* 		mgr_on_begin();
ptr_t			mgr_on_rd( ptr_t r_addr, ver_t* tag );
ptr_t			mgr_on_wr( ptr_t w_addr, size_t sz, ver_t* tag );
void			mgr_on_commit();
void 			mgr_on_abort(int reason);

inline void txn_end(tm_mutex_t *lock) {
	if(_xtest()) {
		_xend();
		cstats_inc(p_trans.stats.n_commit_hw);
		return;
	} else if(mutex_is_locked_by_me(lock, __thread_id)) {
		mutex_unlock(lock);
		cstats_inc(p_trans.stats.n_commit_sw);

		return;
	}

	//should never be here
	exit(1);
}


/**
 * 1st layer, hardware transactions
 */
inline int begin_hw(tm_mutex_t * lock, unsigned int max_retries) {
  unsigned int nretries = 0;

  cstats_inc(p_trans.stats.n_start_hw);

  while(1) {
    unsigned int status = _xbegin();

    if(status == _XBEGIN_STARTED) {
    	if(mutex_is_free(lock)) {
    		return 1;
    	}
    	_xabort(0xff);
    }

    /**
	 * Fall-back right away for 1 of 2 reasons:
	 * 1. HW retry bit suggests not retrying
	 * 2. Abort due to capacity overflow
	 */
    if(!(status & _XABORT_RETRY) || (status & _XABORT_CAPACITY)) {
		cstats_inc(p_trans.stats.n_abort_other_hw);
		break;
    }

    // Handle explicit abort due to sw lock unavailable or conflict
    if((status & _XABORT_CONFLICT) || ((status & _XABORT_EXPLICIT) && _XABORT_CODE(status)==0xff)) {
    	// to eliminate lemming effect, wait for fallback lock to be free
    	while(mutex_is_locked_by_another(lock, __thread_id)) {
    		PAUSE();
    	}
    	//continue;
    }

    // 2nd criteria: retried too many times (> max_retries)
	if (nretries >= max_retries) {
		cstats_inc(p_trans.stats.n_abort_conflicts_hw);
		break;
	}

	++nretries;

    // unknown abort reasons: abort
    //break;
  }

  // Do not sw lock at this point because we have multiple layers of fall-back
  //mutex_lock( lock, __thread_id );
  return 0;
}

/**
 * 2nd/3rd layer, hybrid transactions
 */

inline void commit_begin_hybrid (tm_mutex_t * lock, unsigned int max_retries) {
  unsigned int nretries = 0;
  unsigned int status;

  cstats_inc(p_trans.stats.n_start_hybrid_commit_hw);

  while(1) {
    status = _xbegin();

    if(status == _XBEGIN_STARTED) {
    	if(mutex_is_free(lock)) {
    		return;
    	}
    	_xabort(0xff);
    }

    /**
	 * Fall-back right away for 1 of 2 reasons:
	 * 1. HW retry bit suggests not retrying
	 * 2. Abort due to capacity overflow
	 */
    if(!(status & _XABORT_RETRY) || (status & _XABORT_CAPACITY)) {
		cstats_inc(p_trans.stats.n_abort_other_hybrid_hw);
		break;
    }

    // Hybrid HW explicit abort due to validation failure
    if ((status & _XABORT_EXPLICIT) && _XABORT_CODE(status)==0xef) {
    	mgr_on_abort(1);
    	cstats_inc(p_trans.stats.n_abort_invalidations_hybrid_hw);
    	return;
    }

    // Fall-back criteria: retried too many times (> max_retries)
	if (nretries >= max_retries) {
		//printf("too many retries\n");
		cstats_inc(p_trans.stats.n_abort_conflicts_hybrid_hw);
		break;
	}

	++nretries;

    // Handle explicit abort due to sw lock unavailable or conflict
    if((status & _XABORT_CONFLICT) || ((status & _XABORT_EXPLICIT) && _XABORT_CODE(status)==0xff)) {
    	// to eliminate lemming effect, wait for fallback lock to be free
    	while(mutex_is_locked_by_another(lock, __thread_id)) {
    		PAUSE();
    	}

    	continue;
    }

    // unknown abort reasons: abort
    break;
  }

  //we commented this out when bypassing layer 3, to go from layer 2 straight to layer 4.
  //cstats_inc(p_trans.stats.n_start_hybrid_commit_sw);
  //bypassing layer 3 to go from layer 2 straight to layer 4: replace the
  //mutex_lock() with mgr_on_abort(), which will return to sigsetjmp in BEGIN_TRANSACTION,
  //which will then fall through the switch statement to the next "case SW", which is 
  //the pure software layer (layer 4).
  mgr_on_abort(1);
  //mutex_lock( lock, __thread_id );
  return;
}

inline void commit_end_hybrid(tm_mutex_t *lock) {
	if(_xtest()) {
		_xend();
		cstats_inc(p_trans.stats.n_commit_hybrid_hw);
		return;
	} else if(mutex_is_locked_by_me(lock, __thread_id)) {
		tm_assert(mutex_is_locked_by_me(lock, __thread_id));
		mutex_unlock(lock);
		cstats_inc(p_trans.stats.n_commit_hybrid_sw);
		return;
	}

	//should never be here
	exit(1);
}

/**
 * 4th layer, sw-only transactions
 */
inline void begin_sw(tm_mutex_t * lock) {
	mutex_lock( lock, __thread_id );
}

#endif // __tm_scope_h
