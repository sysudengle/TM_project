/**
 * If RAW = Pes, WAR = Pes, and WAW = Pes or
 * RAW = Opt, WAR = Opt, and WAW = Opt
 *
 * Use code in this file
 */

#ifndef TM_LOCK_X_H_
#define TM_LOCK_X_H_

#include "tm_lock.h"


#define GOT_READ_MASK( tran_id )     (BITMASK << (tran_id))
#define GOT_WRITE_MASK( tran_id )    ((unsigned long)((BITMASK << (tran_id)) | (BITMASK << WRITE_BIT)))

#define tm_lock_has_rd( _tmlock, _tran_id )	is_set( (_tmlock)->rlock, (_tran_id) )
#define tm_lock_has_wr( _tmlock, _tran_id )	( (_tmlock)->rlock == GOT_WRITE_MASK( _tran_id ) )



#ifndef DISABLE_LOCKS


/*		Check for read-write and write-write conflicts		*/

static
uint_t is_conflict_rw( uint_t c_id, uint_t tran_id, uint_t trail )
{
    uint_t i, tmp;
    uint_t deadlock_trail;
	
	tm_lock_t* _lock = deadlock_table[ c_id*16 ];
	if( !_lock )							
	    return 0;
	tmp = _lock->rlock;
	if( !is_set( tmp, WRITE_BIT ) )			
	    return 0;
	clear_mask( tmp, (BITMASK << WRITE_BIT) | (BITMASK << c_id) );
	if( !tmp )								
	    return 0;
    
    //if( is_set( tmp, tran_id ) && (trail >> tran_id) == 1 )	return trail; // c_id is waiting for me
	if( is_set( tmp, tran_id ) )									
	    return trail;
	
	clear_mask( tmp, trail );
	if( !tmp )								
	    return 0;
    for( i = 0; i < MAX_NO_THREADS; i++ )
    {
        if( is_set( tmp, i ) && deadlock_table[ i*16 ] && ( deadlock_trail = is_conflict_rw( i, tran_id, trail | (BITMASK << i) ) ) )
                return deadlock_trail;
    }
    return 0;
}

___always_inline  static
uint_t is_deadlock_rw( tm_lock_t* _lock, uint_t tran_id )
{
	if( !deadlock_table[tran_id*16] )
	{
		atomic_set_mask( _lock, &deadlock_table[tran_id*16] );
		return is_conflict_rw( tran_id, tran_id, BITMASK << tran_id);
	}
	else return 0;
	
	//set_deadlock_table( tran_id, _lock );		
	//return is_conflict_rw( tran_id, tran_id, BITMASK << tran_id);
}


___always_inline  static
void tm_lock_waitfor_readers( tm_lock_t* _lock, uint_t tran_id )
{
    int b = 64;

	uint_t readers = (_lock->rlock & (~(GOT_WRITE_MASK( tran_id ))));
	if( readers )	
	   tm_log2( "%x - lock_wait_for_readers %x  %x\n", (uint_t)_lock, readers );

	while( _lock->rlock != GOT_WRITE_MASK( tran_id ) )
	{
		if( is_deadlock_rw( _lock, tran_id ) )
		{
			atomic_clear_mask( GOT_WRITE_MASK( tran_id ), &(_lock->rlock) );
			ABORT_TRANSACTION(1);
		}
		
		p_trans[ tran_id ].stats.wt_war += b;
		backoff( b );
	}
	clear_deadlock_table( tran_id );
}



/*		Check for write-write conflicts		*/

static
uint_t is_conflict_w( uint_t c_id, uint_t tran_id, uint_t trail )
{
    uint_t tmp, i;
	uint_t deadlock_trail;

	tm_lock_t* _lock = deadlock_table[ c_id*16 ];
	if( !_lock )							
	    return 0;
	tmp = _lock->rlock;
        if( !is_set( tmp, WRITE_BIT ) )			
            return 0;
	clear_mask( tmp, (BITMASK << WRITE_BIT) | (BITMASK << c_id) );
	if( !tmp )								
	    return 0;

	//if( is_set( tmp, tran_id ) && (trail >> tran_id) == 1 )		return trail;		// c_id is waiting for me
	if( is_set( tmp, tran_id ) )									
	   return trail;
	
	if( tmp & trail )					
	    return 0;
	for( i = (uint_t)-1; tmp; tmp = tmp >> 1, i++ );
	if( deadlock_table[ i*16 ] && ( deadlock_trail = is_conflict_w( i, tran_id, trail | (BITMASK << i) ) ) )
		return deadlock_trail;

    return 0;
}


___always_inline  static
uint_t is_deadlock_w( tm_lock_t* _lock, uint_t tran_id )
{
	if( !deadlock_table[tran_id*16] )
	{
		atomic_set_mask( _lock, &deadlock_table[tran_id*16] );
		return is_conflict_w( tran_id, tran_id, BITMASK << tran_id);
	}
	else return 0;
	
	//set_deadlock_table( tran_id, _lock );		
	//return is_conflict_w( tran_id, tran_id, BITMASK << tran_id);
}


___always_inline  static
void tm_lock_invalidate_readers( uint_t readers, uint_t tran_id )
{
	uint_t i;
	clear_mask( readers, (BITMASK << WRITE_BIT) | (BITMASK << tran_id) );

	if( readers )
	{
		tm_log1( "%x - invalidate_readers %x\n", readers );

		for( i = 0; i < MAX_NO_THREADS; i++ )
			if( is_set( readers, i ) && !aborted_table[ i*16 ] )
				atomic_set_mask( BITMASK, (&aborted_table[ i*16 ]) );
	}
}



#ifdef WAITFOR_READERS
#define is_deadlock		is_deadlock_rw
#define resolve_readers()	tm_lock_waitfor_readers( _lock, tran_id )
#define NEW_WRITE_LOCK()	tmp | GOT_WRITE_MASK( tran_id )
#endif

#ifdef ABORT_READERS
#define is_deadlock		is_deadlock_w
#define resolve_readers()	tm_lock_invalidate_readers( tmp, tran_id )
#define NEW_WRITE_LOCK()	GOT_WRITE_MASK( tran_id )
#endif




___always_inline  static
void tm_lock_rdlock( tm_lock_t* _lock, uint_t tran_id )
{
	int b = 64;
	uint_t tmp;

	tm_log1( "%x - rd_lock %x\n", (uint_t)_lock );
	
	while( 1 )
	{
		while( is_clear( (tmp  = _lock->rlock), WRITE_BIT ) )
		{
			if( unlikely( aborted_check2( tran_id ) ) )			
			    ABORT_TRANSACTION(0);

			if( cas( &_lock->rlock, tmp, tmp | GOT_READ_MASK( tran_id ) ) == tmp )
			{
				clear_deadlock_table( tran_id );
				return;
			}
		}

		if( is_deadlock( _lock, tran_id ) || aborted_check2( tran_id ) )     
			ABORT_TRANSACTION(0);

		p_trans[ tran_id ].stats.wt_raw += b;
		backoff( b );
	}
}



___always_inline  static
void tm_lock_wrlock( tm_lock_t* _lock, uint_t tran_id )
{
	int b = 64;
	uint_t tmp;

	tm_log1( "%x - wr_lock %x\n", (uint_t)_lock );

	while( 1 )
	{
		//  check if rlock is clear
		while( is_clear( (tmp  = _lock->rlock), WRITE_BIT ) )
		{
			if( unlikely( aborted_check2( tran_id ) ) )			
			    ABORT_TRANSACTION(2);

			/**
			 * Attempt to grab the lock
			 * NEW_WRITE_LOCK() will produce a bit mask with the write and the
			 * transaction id bits set.
			 */
			if( cas( &_lock->rlock, tmp, NEW_WRITE_LOCK() ) == tmp )
			{
				clear_deadlock_table( tran_id );
				resolve_readers();
				tm_assert( _lock->rlock == GOT_WRITE_MASK( tran_id ) );
				return;
			}
		}

		if( is_deadlock( _lock, tran_id ) || aborted_check2( tran_id ) )     
			ABORT_TRANSACTION(2);
		
		//  Track waiting stats for lock grabbing
		p_trans[ tran_id ].stats.wt_waw += b;

		//  Backoff for trying to grab the lock again
		backoff( b );
	}
}




___always_inline  static
void tm_lock_rdunlock( tm_lock_t* _lock, uint_t tran_id )
{
	if( is_clear( _lock->rlock, tran_id ) )				
	    return;
	tm_log1( "%x - rd_unlock %x\n", (uint_t)_lock );
	atomic_clear_mask( GOT_READ_MASK( tran_id ), &_lock->rlock );
}

___always_inline  static
void tm_lock_wrunlock( tm_lock_t* _lock, uint_t tran_id )
{
	tm_log1( "%x - wr_unlock %x\n", (uint_t)_lock );
	
	#ifdef WAW_Opt
	if( _lock->rlock != GOT_WRITE_MASK( tran_id ) )		
	    return;
	#endif
	
	_lock->rlock = 0;
}




#else	// DISABLE_LOCKS



___always_inline static void tm_lock_rdlock( tm_lock_t* _lock, uint_t tran_id )
{
	tm_log1( "%x - rd_lock_fake\n", (uint_t)_lock );
	_lock->rlock = GOT_READ_MASK( tran_id );
}
___always_inline static void tm_lock_wrlock( tm_lock_t* _lock, uint_t tran_id )
{
	tm_log1( "%x - wr_lock_fake\n", (uint_t)_lock );
	_lock->rlock = GOT_WRITE_MASK( tran_id );
}

___always_inline static void tm_lock_rdunlock( tm_lock_t* _lock, uint_t tran_id )
{
	tm_log1( "%x - rd_unlock_fake\n", (uint_t)_lock );
	_lock->rlock = 0;
}
___always_inline static void tm_lock_wrunlock( tm_lock_t* _lock, uint_t tran_id )
{
	tm_log1( "%x - wr_unlock_fake\n", (uint_t)_lock );
	_lock->rlock = 0;
}



#endif	// DISABLE_LOCKS





#endif /*TM_LOCK_X_H_*/
