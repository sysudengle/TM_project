#include <stdlib.h>
#include <assert.h>


#ifdef TM_DEBUG
#define tm_assert( expr )  assert( expr )
#else
#define tm_assert( expr )
#endif


#include "hashlib.h"

#define INITSZ 17   /* small prime, for easy testing */

/* WARN   WARN   WARN   WARN  WARN */
/* Peculiar definition, never to escape this module and NEVER */
/* NEVER to be dereferenced.   This is the equivalent of NULL */
/* NEVER EVER change the name of 'master' anywhere.           */
#define _DELETED (void*)master



/* table of k where 2**n - k is prime, for n=8 up. 0 ends   */
/* These numbers are chosen so that memory allocation will  */
/* usually allow space for system overhead in a 2**n block  */
/*http://www.utm.edu/research/primes/lists/2small/0bit.html */
#define FIRSTN 8
static int primetbl[] = {45, 45, 41, 45, 45, 45, 45, 49, 57, 49, 41, 45, 59, 55, 57, 61, 63, 61, 45, 79, 0};
/* So the prime of interest, vs index i into above table,   */
/* is    ( 2**(FIRSTN + i) ) - primetbl[i]                  */
/* The above table suffices for about 48,000,000 entries.   */

/* 1------------------1 */

/* return a prime slightly less than 2**(FIRSTN + i) -1 */
/* return 0 for i value out of range                    */
static unsigned long ithprime(size_t i)
{
	tm_assert( (i < (sizeof primetbl / sizeof (int))) && primetbl[i] );
	return ((1 << (FIRSTN + i)) - primetbl[i]);
}



/* Create, allocate and initialize an empty hash table      */
/* This is always doubling the table size, and the size of  */
/* all the old tables together won't hold it.  So any       */
/* freed old table space is effectively useless for this    */
/* because of fragmentation. Changing the ratio won't help. */
static void* *maketbl(unsigned long newsize)
{
	unsigned long  i;
	void*   *newtbl;
	tm_assert( newsize > 0 );

	newtbl = (void**)malloc( newsize * sizeof(void*) );	
	tm_assert( newtbl );
	for (i = 0; i < newsize; i++)		
	    newtbl[i] = NULL;

	return newtbl;
}



/* initialize and return a pointer to the data base */
void hsh_init( struct hshtag *master )
{
	if( master )
	{
		if (master->htbl) return;
		master->htbl = maketbl(INITSZ);
		master->currentsz = INITSZ;
		master->htbl_end = master->htbl + master->currentsz;
		master->seqb = seqbuff_init();
		master->bloom_filter = 0;
		master->last_found_key = 0;

		master->hstatus.hentries = 0;
		master->hstatus.hdeleted = 0;
		master->hstatus.herror = hshOK;
	}
	return;
} /* hshinit */

/* destroy the data base */
void hsh_kill(hshtblptr master)
{
   if (master)
   {
	  free(master->seqb);
      free(master->htbl);
   }
} /* hshkill */


void hsh_reset(struct hshtag *master)
{
   if (master)
   {
	   if (!master->htbl) {
		   return hsh_init(master);
	   }

	   unsigned long i;

	   for (i = 0; i < master->currentsz; i++)
		   master->htbl[i] = NULL;

	   reset_seqbuff(master->seqb);

	   master->bloom_filter = 0;
	   master->last_found_key = 0;
	   master->hstatus.hentries = 0;
	   master->hstatus.hdeleted = 0;
	   master->hstatus.herror = hshOK;
   }
}


void hsh_reorganize( hshtblptr master )
{
	void*         *newtbl = NULL;
	void*         *oldtbl;
	unsigned long  newsize, oldsize;
	unsigned long  oldentries, j;
	unsigned int   i;

	oldsize = master->currentsz;
	oldtbl =  master->htbl;
	oldentries = 0;

	if (master->hstatus.hdeleted > (master->hstatus.hentries / 4))
		newsize = oldsize;
	else
	{
		newsize = ithprime(0);
		for (i = 1; newsize <= oldsize; i+=1)		
		    newsize = ithprime(i);
	}
	newtbl = maketbl(newsize);

	master->currentsz = newsize;
	master->htbl = newtbl;

	unsigned long hi;
	void         *ho;
        int_pointer_t hsh_key;

	for (j = 0; j < oldsize; j++)
	    if (oldtbl[j] && (oldtbl[j] != _DELETED))
		{
			hsh_key = _hget_key( oldtbl[j] );
			hsh_lookup( master, hsh_key, hi, ho, _hget_key);	
			tm_assert( ho == NULL );
			master->htbl[ hi ] = oldtbl[j];
			
			oldentries++;
		}
  
	/* Sanity check */
	tm_assert( oldentries == master->hstatus.hentries - master->hstatus.hdeleted );
	
	master->htbl_end = master->htbl + master->currentsz;
	master->last_found_key = 0;
	master->hstatus.hentries = oldentries;
	master->hstatus.hdeleted = 0;
	free(oldtbl);
}

void hsh_reorganize_r( hshtblptr master )
{
	void*         *newtbl = NULL;
	void*         *oldtbl;
	unsigned long  newsize, oldsize;
	unsigned long  oldentries, j;
	unsigned long   i;

	oldsize = master->currentsz;
	oldtbl =  master->htbl;
	oldentries = 0;

	if (master->hstatus.hdeleted > (master->hstatus.hentries / 4))
		newsize = oldsize;
	else
	{
		newsize = ithprime(0);
		for (i = 1; newsize <= oldsize; i+=1)		
		    newsize = ithprime(i);
	}
	newtbl = maketbl(newsize);

	master->currentsz = newsize;
	master->htbl = newtbl;

	unsigned long hi;
        void         *ho;
        int_pointer_t hsh_key;

	for (j = 0; j < oldsize; j++)
		if (oldtbl[j] && (oldtbl[j] != _DELETED))
		{
			hsh_key = _hget_key_r( oldtbl[j] );
    		        hsh_lookup( master, hsh_key, hi, ho, _hget_key_r);	
    		        tm_assert( ho == NULL );
			master->htbl[ hi ] = oldtbl[j];
			
			oldentries++;
		}
  
	/* Sanity check */
	tm_assert( oldentries == master->hstatus.hentries - master->hstatus.hdeleted );
	
	master->htbl_end = master->htbl + master->currentsz;
	master->last_found_key = 0;
	master->hstatus.hentries = oldentries;
	master->hstatus.hdeleted = 0;
	free(oldtbl);
}


