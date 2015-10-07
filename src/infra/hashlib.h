/* -------------- File hashlib.h ------------------ */
#ifndef hashlib_h
#define hashlib_h

#include <stdio.h>
#include "seqbuff.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * __builtin_expect()
 * Help out with branching by eliminating highly unlikely cases, allows CPU
 * instruction pipelines to be flushed less. Used in in Linux kernel code.
 *
 * !!(x) is a trick to convert x to boolean. if x = 0, it will remain 0, for
 * all other values, x will become 1
 *
 */
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)


typedef unsigned long int_pointer_t;

typedef struct hshstats
{
   unsigned long hentries, hdeleted;
   int   herror;
} hshstats;

struct hshtag
{
	void*          *htbl;
	void*          *htbl_end;
	unsigned long   currentsz;
	seqbuff_t	   *seqb;

	unsigned long	bloom_filter;

	int_pointer_t	last_found_key;
	unsigned long	last_found_ind;
	hshstats        hstatus;
};
typedef struct hshtag hshtbl;
typedef struct hshtag* hshtblptr;

/* Possible error returns, powers of 2 */
#define hshOK 0
#define hshNOMEM 1
#define hshTBLFULL 2
#define hshINTERR 4

/* Threshold above which reorganization is desirable */
#define TTHRESH(sz) ((sz) - ((sz) >> 3))
/* Space available before reaching threshold */
/* Ensure this can return a negative value   */
#define TSPACE(m)  (   ((long)TTHRESH( (m)->currentsz )) - ((long)m->hstatus.hentries) )


/*	BLOOM FILTER OPERATIONS	*/
#define FILTERHASH(a)   ( ( ((int_pointer_t)a) >> 2 ) ^ ( ((int_pointer_t)a) >> 5 ) )
#define FILTERBITS(a)   ( 1 << ( FILTERHASH(a) & 0x1F ) )


void hsh_init( hshtbl* master );
void hsh_kill( hshtbl* master );
void hsh_reset(hshtbl* master);

void hsh_reorganize( hshtbl* master );
void hsh_reorganize_r( hshtbl* master );




// hk == hash hey
// hi == hash index
// ho == hash object

#define _hget_key( ho )		((int_pointer_t)ho)
#define _hget_key_r( ho )	(*((int_pointer_t*)ho))

#define hsh_lookup( hsh_master, hk, hi, ho, hget_key )		\
{                                                               \
    unsigned long h2;                                           \
    hi = ( hk >> 2 ) % hsh_master->currentsz;                   \
    ho = hsh_master->htbl[ hi ];                                \
    if( ho && hget_key( ho ) != hk )                          	\
    {                                                           \
        h2 = ((hk >> 5) % (hsh_master->currentsz >> 3)) + 1;    \
        do{                                                     \
            hi = (hi + h2) % hsh_master->currentsz;             \
            ho = hsh_master->htbl[ hi ];                        \
        }while( ho && hget_key( ho ) != hk );              	\
    }                                                           \
}

#define hsh_add( hsh_master, hsh_item, hsh_key, hi )		\
{								\
	hsh_master->htbl[ hi ] = hsh_item;			\
	hsh_master->hstatus.hentries++;				\
	hsh_master->bloom_filter |= FILTERBITS( hsh_key );	\
    if( unlikely( TSPACE(hsh_master) <= 0 ) )			\
    	hsh_reorganize( hsh_master );				\
}

#define hsh_add_r( hsh_master, hsh_item, hsh_key, hi )		\
{								\
	hsh_master->htbl[ hi ] = hsh_item;			\
	hsh_master->hstatus.hentries++;				\
	hsh_master->bloom_filter |= FILTERBITS( hsh_key );	\
    if( unlikely( TSPACE(hsh_master) <= 0 ) )			\
    	hsh_reorganize_r( hsh_master );				\
}



inline __attribute__((always_inline))
void hsh_insert( hshtbl* hsh_master, void* hsh_item)
{
    unsigned long hi;
    void         *ho;

    int_pointer_t hsh_key = _hget_key( hsh_item );
    hsh_lookup( hsh_master, hsh_key, hi, ho, _hget_key );	
    tm_assert( ho == NULL );
    hsh_add( hsh_master, hsh_item, hsh_key, hi );
}

inline __attribute__((always_inline))
void* hsh_insert_r( hshtbl* hsh_master, int_pointer_t hsh_key, size_t sz )
{
	unsigned long hi;
	void   *ho = NULL;
	void   *hsh_item = NULL;

	hsh_lookup( hsh_master, hsh_key, hi, ho, _hget_key_r );	tm_assert( ho == NULL );

	hsh_item = seqbuff_put_ptr( hsh_master->seqb, sz );
	*((int_pointer_t*)hsh_item) = hsh_key;

	hsh_add_r( hsh_master, hsh_item, hsh_key, hi );
	return hsh_item;
}

inline __attribute__((always_inline))
void* hsh_insert_r2( hshtbl* hsh_master, int_pointer_t hsh_key, size_t sz )
{
	unsigned long hi;
	void   *ho = NULL;
	void   *hsh_item = NULL;

	if( hsh_master->last_found_key != hsh_key )
	{
		hsh_lookup( hsh_master, hsh_key, hi, ho, _hget_key_r );
		tm_assert( ho == NULL );//assert( 0 && ho == NULL );
	}
	else	
	    hi = hsh_master->last_found_ind;

	hsh_item = seqbuff_put_ptr( hsh_master->seqb, sz );
	// hsh_key is the address of the variable being written to

	//TODO: removed this, it is set outside anyways
	*((int_pointer_t*)hsh_item) = hsh_key;

	hsh_add_r( hsh_master, hsh_item, hsh_key, hi );
	return hsh_item;
}


inline __attribute__((always_inline))
void* hsh_find( hshtbl* hsh_master, int_pointer_t hsh_key )
{
    unsigned long hi;
    void* ho = NULL;

    if( hsh_master->bloom_filter & FILTERBITS( hsh_key ) )
   	hsh_lookup( hsh_master, hsh_key, hi, ho, _hget_key );

    return ho;
}

inline __attribute__((always_inline))
void* hsh_find_r( hshtbl* hsh_master, int_pointer_t hsh_key )
{
    unsigned long hi;
    void* ho = NULL;

    if( hsh_master->bloom_filter & FILTERBITS( hsh_key ) )
	hsh_lookup( hsh_master, hsh_key, hi, ho, _hget_key_r );

    return ho;
}




inline __attribute__((always_inline))
void* hsh_find_r2( hshtbl* hsh_master, int_pointer_t hsh_key )
{
    unsigned long hi;
    void*   ho = NULL;

    if( hsh_master->bloom_filter & FILTERBITS( hsh_key ) )
	{
		hsh_lookup( hsh_master, hsh_key, hi, ho, _hget_key_r );
		hsh_master->last_found_key = hsh_key;
		hsh_master->last_found_ind = hi;
	}

    return ho;
}


inline __attribute__((always_inline))
int hsh_contains( hshtbl* hsh_master, int_pointer_t hsh_key )
{
    if( hsh_master->bloom_filter & FILTERBITS( hsh_key )) {
		return 1;
	}

    return 0;
}


#define _hsh_delete( hsh_master, hsh_key, hget_key )		\
{                                                           	\
    unsigned long hi;                                       	\
    void         *ho;                                       	\
    hsh_lookup( hsh_master, hsh_key, hi, ho, hget_key );	\
    if( ho )                                                	\
    {                                                       	\
        hsh_master->htbl[ hi ] = hsh_master;                	\
        hsh_master->hstatus.hdeleted++;                     	\
    }                                                       	\
}
#define hsh_delete( hsh_master, hsh_key)    _hsh_delete( hsh_master, hsh_key, _hget_key )
#define hsh_delete_r( hsh_master, hsh_key)  _hsh_delete( hsh_master, hsh_key, _hget_key_r )



#define hsh_for_each( ho, hsh_master, ho_t, p_ho )												\
	ho_t*	ho;																					\
	void*	*p_ho;																				\
	(hsh_master)->hstatus.hentries = 0;															\
	(hsh_master)->bloom_filter = 0;																\
	(hsh_master)->last_found_key = 0;															\
	for( p_ho = (hsh_master)->htbl ; p_ho < (hsh_master)->htbl_end ; *p_ho = NULL, p_ho++ )		\
        if( ( ho = (ho_t*)(*p_ho) ) )

#define hsh_for_each_r( ho, hsh_master, ho_t, ho_sz )											\
	(hsh_master)->hstatus.hentries = 0;															\
	(hsh_master)->bloom_filter = 0;																\
	(hsh_master)->last_found_key = 0;															\
	memset( (hsh_master)->htbl, 0, sizeof(void*) * (hsh_master)->currentsz );					\
	seqbuff_for_each( ho, hsh_master->seqb, ho_t, ho_sz )

#define hsh_pass_by_r( ho, hsh_master, ho_t, ho_sz )											\
	seqbuff_pass_by( ho, hsh_master->seqb, ho_t, ho_sz )

#ifdef __cplusplus
}
#endif

#endif

