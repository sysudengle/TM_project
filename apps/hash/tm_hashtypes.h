#ifndef __TM_HASHTYPES_H__
#define __TM_HASHTYPES_H__

#include "../../src/tm.h"
#include "../../src/infra/tatas.h"

// The hash size
#define HASH_SIZE   1 //33107 //262144 //131072 //262144 //1031 //33107 //65147 //21991 // This is a prime number (from here http://alumnus.caltech.edu/~chamness/prime.html)

extern int PMODE;			// Protection Mode: 0 = transactions; 1 = coarse-grain locks; 2 = fine-grain locks

#define RAND()	((unsigned int)random())


class Hash_elem_t;
typedef Hash_elem_t * PHash_elem_t;

typedef tm_type< PHash_elem_t >		tm_PHash_elem_t;

/*
typedef PHash_elem_t	    tm_PHash_elem_t;
*/

// The chain structure
class Hash_elem_t : public tm_obj
{
public:
	tm_int				key;
	tm_PHash_elem_t		next;
	tatas_lock_t		e_lock;
};

// This will be shared by all nodes
typedef struct Hash_Table
{
	tm_PHash_elem_t buckets[ HASH_SIZE ];
	tatas_lock_t b_locks[ HASH_SIZE ];
} Hash_Table;



inline void Hash_init( Hash_Table* htable )
{
	int i;
	
	for( i = 0; i < HASH_SIZE; i++ )
	{
		Hash_elem_t* h = new Hash_elem_t;	
		h->key = -1;
		tatas_init( &h->e_lock, NULL );
		
		htable->buckets[i] = h;
		tatas_init( &htable->b_locks[i], NULL );	
	}
}

inline void Hash_insert( Hash_Table* htable, PHash_elem_t  h  )
{
	unsigned int bucket_nr = h->key % HASH_SIZE ;
	PHash_elem_t left;
	PHash_elem_t right;
	
	if( PMODE <= 1 )
	{
		if( PMODE == 1 )	tatas_lock( &htable->b_locks[ bucket_nr ] );

		left = htable->buckets[ bucket_nr ];
		right = left->next;

		while( right )
		{
			if( ((int)right->key) >= h->key )	break;
			left = right;
			right = right->next;
		}

		h->next = right;
		left->next = h;

		if( PMODE == 1 )	tatas_unlock( &htable->b_locks[ bucket_nr ] );
	}
	
	if( PMODE == 2 )
	{
		left = htable->buckets[ bucket_nr ];
		tatas_lock( &left->e_lock );
		right = left->next;
		while( right )
		{
			tatas_lock( &right->e_lock );
			if( ((int)right->key) >= h->key )
			break;
			tatas_unlock( &left->e_lock );
			left = right;
			right = right->next;
		}
			
		h->next = right;
		left->next = h;
		tatas_unlock( &left->e_lock );
		if( right )	tatas_unlock( &right->e_lock );
	}
}

inline int Hash_retrieve( Hash_Table * htable, int key)
{
	unsigned int bucket_nr = key % HASH_SIZE;
	PHash_elem_t left;
	PHash_elem_t right;
	int found = 0;
	
	if( PMODE <= 1 )
	{
		if( PMODE == 1 )	tatas_lock( &htable->b_locks[ bucket_nr ] );
		
		left = htable->buckets[ bucket_nr ];
		right = left->next;
		while( right )
		{
			if( ((int)right->key) >= key )
			break;
			left = right;
			right = right->next;
		}
		if( right && ((int)right->key) == key )
			found = 1;
			
		if( PMODE == 1 )	tatas_unlock( &htable->b_locks[ bucket_nr ] );
	}
	
	if( PMODE == 2 )
	{
		left = htable->buckets[ bucket_nr ];
		tatas_lock( &left->e_lock );
		right = left->next;
		while( right )
		{
			tatas_lock( &right->e_lock );
			if( ((int)right->key) >= key )
			break;
			tatas_unlock( &left->e_lock );
			left = right;
			right = right->next;
		}
		
		if( right && ((int)right->key) == key )
			found = 1;
		tatas_unlock( &left->e_lock );
		if( right )	tatas_unlock( &right->e_lock );
	}
	
	
	return found;
}

#endif	// __TM_HASHTYPES_H__

