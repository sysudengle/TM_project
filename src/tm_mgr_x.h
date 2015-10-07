#ifndef TM_MGR_X_H_
#define TM_MGR_X_H_

#include "tm_mgr.h"
#include "tm_scope.h"


#define get_rs_addr(entry_t)	(res_addr += sizeof( entry_t ))

/***********************************************************
		READS	WRAPPERS
***********************************************************/

// TODO: optimize here, no need to do sizeof all the time
#define read_set_put( r_tag, r_read_set )				hsh_insert_r2( r_read_set, (int_pointer_t)(r_tag), sizeof(read_entry_t) )

#define read_set_for_each( r_rd, r_read_set )			hsh_for_each_r( w_rd, r_read_set, read_entry_t, 0 )
#define read_set_pass_by( r_rd, r_read_set )			hsh_pass_by_r( r_rd, r_read_set, read_entry_t, 0 )

#define rd_has( r_tag )									(tag_record = (read_entry_t *)hsh_find_r2( &p_trans.read_set, (int_pointer_t)(r_tag) ))
#define rd_has_entry( r_tag )							rd_has( r_tag )

// TODO: is the tag value always bigger than the recorded tag value?
#define validate_r_tag_addr(tag_entry)						(tag_entry->tag_addr == tag_entry->tag_addr)

#define validate_r_tag(tag_entry)						(tag_entry->tag_addr->tag_version == tag_entry->tag_value)

/***********************************************************
		WRITES	WRAPPERS
***********************************************************/

# define write_set_put( w_addr, w_sz, w_write_set ) 	hsh_insert_r2( w_write_set, (int_pointer_t)(w_addr), sizeof(write_entry_t) + (w_sz) )
# define write_set_for_each( w_wr, w_write_set )		hsh_for_each_r( w_wr, w_write_set, write_entry_t, (w_wr)->sz )
# define write_set_pass_by( w_wp, w_write_set )			hsh_pass_by_r( w_wp, w_write_set, write_entry_t, (w_wp)->sz )


# define wr_has( w_addr )								(res_addr = (ptr_t)hsh_find_r2( &p_trans.write_set, (int_pointer_t)(w_addr) ))

# define wr_has_wb_entry( w_addr )						wr_has( w_addr )

# define get_wr_res_addr()					get_rs_addr( write_entry_t )
# define set_res_addr()						(wr->twin)

___always_inline  static
void rd_record( ver_t * r_tag )
{
	read_entry_t* rd = (read_entry_t*)read_set_put( r_tag, &p_trans.read_set );
	rd->tag_addr = r_tag;
	rd->tag_value = r_tag->tag_version;

	//printf("tag_addr: %ld, tag_value: %ld\n", rd->tag_addr, rd->tag_value);
	//tm_memcpy( rd->twin, r_addr, r_sz );
}


___always_inline  static
ptr_t wr_record( ptr_t w_addr, size_t w_sz, ver_t* w_tag )
{
	write_entry_t* wr = (write_entry_t*)write_set_put( w_addr, w_sz, &p_trans.write_set );
	wr->addr = w_addr;
	wr->sz = w_sz;
	wr->tag_addr = w_tag;

#ifdef TM_DEBUG
	//printf("ws item: %X, size: %lu, actual addr: %X, entry addr: %X\n", wr, wr->sz, wr->addr, wr->twin);
#endif

	return set_res_addr();
}

/**********************************************************************
		ON_RD
**********************************************************************/

___always_inline static
ptr_t _mgr_on_rd( ptr_t addr, ver_t* tag )
{
	ptr_t res_addr = addr;
	read_entry_t* tag_record;

	// All writes involve a read, so no need to record the read
	if( wr_has_wb_entry( addr ) ) {
		return get_wr_res_addr();
	}

	if( tag != NULL && rd_has_entry( tag ) ) {

#ifdef EAGER_ABORT
		if (!validate_r_tag(tag_record)) {
		        tag->conflict_counter ++;
			mgr_on_abort(1);
		}
#endif

		return addr;
	}

	rd_record( tag );

	return addr;
}


/**********************************************************************
		ON_WR
**********************************************************************/

/* Return the address of the data so it can be written */
___always_inline static
ptr_t _mgr_on_wr( ptr_t addr, size_t sz, ver_t* tag_addr )
{
	ptr_t res_addr = addr;

	if( wr_has_wb_entry( addr ) ) {
		return get_wr_res_addr();
	}

	return wr_record( addr, sz, tag_addr );
}


/**********************************************************************
		ON_COMMIT
**********************************************************************/

___always_inline static
void _mgr_on_commit( tran_t* l_tran, tm_mutex_t *lock)
{
	write_set_t* write_set = &l_tran->write_set;
	read_set_t*  read_set  = &l_tran->read_set;

	// begin hybrid commit process
	commit_begin_hybrid(lock, 0);

	// Validate the reads
	read_set_pass_by( rd, read_set ) {
		if (!validate_r_tag(rd)) {
		        //I think we should do this regardless of whether we're in a hw txn or not
		        rd->tag_addr->conflict_counter ++;
			if (_xtest()) {
				_xabort(0xef);
			} else {
				tm_assert(mutex_is_locked_by_me(lock, __thread_id));

				mutex_unlock(lock);
				cstats_inc(p_trans.stats.n_abort_invalidations_hybrid_sw);
				mgr_on_abort(2);
			}
		}
	}

	// Flush the writes
	write_set_pass_by( wr, write_set ) {
		tm_memcpy( wr->addr, wr->twin, wr->sz );
		++(wr->tag_addr->tag_version);
	}

	// end hybrid commit process
	commit_end_hybrid(lock);

	// Committed changes, deallocate current thread's read/write sets
	read_set_reset( &p_trans.read_set );
	write_set_reset( &p_trans.write_set );
}


/**************************************************************************/
/*********************     TM_MGR WRAPPERS      ***************************/
/**************************************************************************/

/**
 * Do read
 */
inline ptr_t mgr_on_rd( ptr_t addr, ver_t* tag )
{
	/**
	 * Perform direct reads if doing
	 * HW only or SW only transactions
	 */
	if( unlikely( !__trap_access ) )
	    return addr;

	return _mgr_on_rd( addr, tag );
}

/**
 * Do write
 */
inline ptr_t mgr_on_wr( ptr_t addr, size_t sz, ver_t* tag ) {
	return _mgr_on_wr( addr, sz, tag );
}


inline void mgr_on_commit(tm_mutex_t *lock)
{
	tm_assert(__trap_access);
	
	_mgr_on_commit( &p_trans, lock );

	__trap_access = 0;
}

#endif  // TM_MGR_X_H_

