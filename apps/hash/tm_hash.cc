#include <stdio.h>
#include <sys/time.h>
#include "tm_hashtypes.h"

#define RUN_SIZE    5		// transaction size (# of operations)
#define TIMEOUT	    30		// seconds

int NTHREADS =		2;
int PMODE =			0;	// Protection Mode: 0 = transactions; 1 = coarse-grain locks; 2 = fine-grain locks



Hash_Table* htable;

float diff_tv(struct timeval start_tv, struct timeval end_tv)
{
	float diff_time = (((end_tv.tv_sec * 1000000.0) + end_tv.tv_usec) -
							((start_tv.tv_sec * 1000000.0) + start_tv.tv_usec)) / 1000000.0;
	return diff_time;				    
}

unsigned int* no_tx;
	
int run_work( void* arg, int id )
{
	long i;

	struct timeval tv;
	gettimeofday( &tv, NULL);
	srandom( 1031 + id*10 );    //((id + 1) * tv.tv_sec)  + tv.tv_usec
	
	volatile int num_aborts, total_aborts_r = 0, total_aborts_w = 0, no_r_tx = 0, no_w_tx = 0, op, opr = 0;
	float diff; 

	// start of experiment
	struct timeval start_tv, end_tv;
	gettimeofday(&start_tv, NULL);


	do {
		// Perform operations on the hash table
		num_aborts = -1;
		op = RAND()%100;
		if( op >= 50 )                           // read only transaction
		{
			if( PMODE == 0 )	BEGIN_TRANSACTION();
			
			num_aborts++;
			
			//if(num_aborts) 
			//    if(DBG){printf("%x - aborted read\n", mtid);}
			//if(DBG){printf("%x - started read\n", mtid);fflush(stdout);}
			
			for ( i = 0; i < RUN_SIZE; i++) 
			{
				Hash_retrieve( htable, (unsigned int)RAND() );
			}
		
			if( PMODE == 0 )	COMMIT_TRANSACTION();
			
			//if(DBG){printf("%x - commited read\n", mtid);fflush(stdout);}
			no_r_tx ++;
			total_aborts_r += num_aborts;
		}
		else                                             // write transaction
		{
			if( PMODE == 0 )	BEGIN_TRANSACTION();
		
			num_aborts ++;
			//if(num_aborts) 
			//    if(DBG){printf("%x - aborted write\n", mtid);}
			//if(DBG){printf("%x - started write\n", mtid);fflush(stdout);}
			
			for ( i = 0; i < RUN_SIZE; i++) 
			{
				Hash_elem_t* h = new Hash_elem_t;
				h->key = (int) RAND();
				tatas_init( &h->e_lock, NULL );
				Hash_insert( htable, h );
			}
			if( PMODE == 0 )	COMMIT_TRANSACTION();
			
			//if(DBG){printf("%x - commited write\n", mtid);fflush(stdout);}
			no_w_tx ++;
			total_aborts_w += num_aborts;
		}
	
		gettimeofday(&end_tv, NULL);
	} 
	while (diff_tv(start_tv, end_tv) < TIMEOUT); 
	
	// end of experiment
	
	// printf("Experiment time: %.2f\n", diff_tv(start_tv, end_tv));
	printf("%d - %d + %d = %d \n", id, no_r_tx, no_w_tx, (no_r_tx + no_w_tx) );
	printf("%d -r- %.2f\n", id, (float)(100*total_aborts_r)/(float)(no_r_tx + total_aborts_r));
	printf("%d -w- %.2f\n\n", id, (float)(100*total_aborts_w)/(float)(no_w_tx + total_aborts_w));
	no_tx[id] = no_r_tx + no_w_tx;
	return 0;
}


	
int main( int argc, char ** argv)
{
	int i;
	unsigned int n_tx = 0;
	
	if( argc < 5 )
	{
		printf("usage: %s CD_VERSION(1:4) CR_VERSION(1:2) N_TH ProtMODE(0:2)\n", argv[0]);
		return 0;
	}
	
	set_version( atoi( argv[1] ), atoi( argv[2] ) );
	print_version(stdout); printf("\n");
	
	NTHREADS = atoi( argv[3] );
	PMODE = atoi( argv[4] );
	
	no_tx = new unsigned int[NTHREADS];
	
	htable = new Hash_Table;
	Hash_init( htable );
	
	CREATE_TM_THREADS( NTHREADS );
	PARALLEL_EXECUTE( NTHREADS, run_work, NULL );
	DESTROY_TM_THREADS( NTHREADS );

	for( i = 0; i < NTHREADS; i++ )
		n_tx += no_tx[i];
	
	delete htable;
	delete[] no_tx;
	
	printf( "-- %u --\n\n", n_tx );
	return 0;
}
