#ifndef _tm_mgr_basics_d_H
#define	_tm_mgr_basics_d_H

#include "utils/tm_stats.h"
#include "atomics/bitset.h"

#include "infra/hashlib.h"
#include "infra/seqbuff.h"


/***********************************************************
		READS	INFRASTRUCTURE
***********************************************************/

typedef struct _read_entry_t
{
    ver_t *		tag_addr;
    unsigned long 	tag_value;
} read_entry_t;

#define read_entry_t_size 		16

typedef hshtbl				read_set_t;
#define read_set_init			hsh_init
#define read_set_free			hsh_kill
#define read_set_reset			hsh_reset


/***********************************************************
		WRITES	INFRASTRUCTURE
***********************************************************/

typedef struct _write_entry_t
{
    ptr_t           addr;
    ver_t *	    tag_addr;
    size_t          sz;
    byte_t          twin[0];
} write_entry_t;

typedef 	hshtbl		write_set_t;
#define 	write_set_init  hsh_init
#define		write_set_free	hsh_kill
#define 	write_set_reset	hsh_reset


/***********************************************************
 *
 *  	TRAN_T
 *
 **********************************************************/

typedef struct _tran_t
{
    read_set_t	read_set;  //  read owned tm_locks
    write_set_t	write_set; //  read & write owned tm_locks
    cstats_t 	stats;	   //  statistics for this thread

    jmp_buf	jbuf;
} tran_t;

extern __thread    	long 			__thread_id;
extern __thread 	tran_t  		p_trans;
extern __thread		int			__trap_access;
extern __thread		int			__first_run;
extern __thread		int			__hybrid_aborted;
extern __thread		int			__in_transact;
extern 			word_t 			__no_th;
extern 		 	tran_t* 		all_trans[MAX_NO_THREADS];


void 	mgr_on_init();
void 	mgr_on_end();

___always_inline static
void assign_thread_id() {
	if( __thread_id == 0 ) {
		__thread_id = atomic_add_return_prev( 1, &__no_th );
		all_trans[__thread_id] = &p_trans;
		cstats_init(&p_trans.stats);
	}
}


/**************************************************************************/
/*********************     Tag Operations       ***************************/
/**************************************************************************/
___always_inline static
ver_t * init_tag() {
	ver_t * tag = (ver_t*) malloc( sizeof( ver_t ) );
	tag->tag_version = 0;
	tag->conflict_counter = 0;
	//tag->tag_lock = 0;
	return tag;
}

___always_inline static
void reset_tag(ver_t * tag) {
	tag->tag_version = 0;
	tag->conflict_counter = 0;
	//tag->tag_lock = 0;
}

#endif	/* _tm_mgr_basics_H */

