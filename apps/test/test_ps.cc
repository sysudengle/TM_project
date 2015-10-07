#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>


#include "../../src/tm.h"
#include "../../src/utils/hrtime.h"

//#define CORRECT
//#define TIMEOUT

//#define DYNAMIC


//#define SINGLE_OWNER
//#define MULTIPLE_OWNERS




int N_TH;

int N_ELEMS;
int N_ELEMS_PER_TX;

int N_ELEMS_POOL;

unsigned int _WR_QUOTA = 0;
unsigned int _DELAY = 0;

int _ITERATE = 0;
unsigned long long _TIMEOUT = 0;



#ifdef DYNAMIC
int N_RUNS = 2;
unsigned int RUN_WQ[2] = {30,100};
unsigned int RUN_DELAYS[2] = {1000,0};
#else
int N_RUNS = 1;
#endif


#define RAND()	((unsigned int)random())

tm_int* cnt;

volatile int* to_acc[MAX_NO_THREADS];
volatile int* read_parts[MAX_NO_THREADS];
double durations[MAX_NO_THREADS] = {0.0};


#ifdef CORRECT

#define reset_read_parts()								\
{											\
	for( i = 0, j = to_acc[id][base+i]; i < k; j = to_acc[id][base+(++i)] )		\
		if( j > 0 )	 	read_parts[id][j] = 0;				\
}

#define tx_access()																						\
{																										\
	if( j > 0 ) 	read_parts[id][j] = cnt[j];															\
	else			cnt[-j] = ( read_parts[id][-j] > 0 ) ? (++(read_parts[id][-j])) : (cnt[-j]+1);		\
}

#else

#define reset_read_parts()

#ifdef DISABLE_ALL
#define tx_access()								\
{												\
	if( j > 0 ) 	read_x = cnt[j]._t+j;		\
	else			cnt[-j]._t = read_x-j;		\
}
#else
#define tx_access()								\
{												\
	if( j > 0 ) 	read_x = cnt[j]+j;			\
	else			cnt[-j] = read_x-j;			\
}
#endif

#endif


#define check_break_per_attempt()																	\
	if( (_TIMEOUT && !((l++)%100) && get_c() > _TIMEOUT) || ( _ITERATE && l++ >= _ITERATE ) )		\
	{	COMMIT_TRANSACTION();	break;		}



int run_work( void* arg, int id )
{
	volatile int i, j, k, l, base;
	volatile unsigned int n;
	volatile int read_x;
	read_x = 0;
	i = 0; l = 0; n = 0;

	volatile unsigned long long _start = get_c();

	while( 1 )
	{
		base =  RAND() % (N_ELEMS_POOL-N_ELEMS_PER_TX);
		
		k = 0;
		
		BEGIN_TRANSACTION();
			
			check_break_per_attempt();
			reset_read_parts();
	
			for( k = 0, j = to_acc[id][base+k]; k < N_ELEMS_PER_TX; j = to_acc[id][base+(++k)] )
			{
				for( n = 0; n < _DELAY; n++ )    asm volatile("nop");
				tx_access();
			}

		COMMIT_TRANSACTION();
		
		reset_read_parts();
	}

	durations[id] = c_to_t( get_c() - _start );

    return 0;
}


int run_assign_ownership( void* arg, int id )
{
	int i;i = 0;
#ifdef MULTIPLE_OWNERS
	for( i = N_ELEMS * id + 1; i < N_ELEMS * (id + 1) + 1; i++ )
		cnt[i].assign_owner();
#endif

#ifdef SINGLE_OWNER
	for( i = 0; i < N_ELEMS * (N_TH + 1) + 1; i++ )
		cnt[i].assign_owner();
#endif

   return 0;
}



int main(int argc, char** argv)
{
	int i, j, k, timeout, iterate, i_runs;
	timeout = -1; iterate = -1;
	
	if( argc < 9 )
	{
		printf("usage: %s CD_VERSION CR_VERSION N_TH N_ELEMS N_ELEMS_PER_TX WR_QUOTA DELAY [ITERATE | TIMEOUT]\n", argv[0]);
		return 0;
	}
	
	set_version( atoi(argv[1]), atoi(argv[2]) );

	N_TH = atoi( argv[3] );
	N_ELEMS = atoi( argv[4] );
	N_ELEMS_PER_TX = atoi( argv[5] );
	
	N_ELEMS_POOL = N_ELEMS * (N_TH);

	_WR_QUOTA = atoi( argv[6] );
	_DELAY = atoi( argv[7] );
	
	FILE* out_f = NULL;
	if( argc == 10 )    out_f = fopen( argv[9], "a" );

	#ifdef TIMEOUT
	timeout = atoi(argv[8]);
	printf("TIMEOUT %d ", timeout );
	if( out_f )		fprintf( out_f, "TIME %d ", timeout );
	#else
	iterate = atoi( argv[8] );
	printf("ITERATE %d ", iterate );
	if( out_f )		fprintf( out_f, "ITERATE %d ", iterate );
	#endif

	printf( "N_TH %d N_ELEMS %d N_PER_TX %d ", N_TH, N_ELEMS, N_ELEMS_PER_TX );fflush(stdout);
	if( out_f ){	fprintf( out_f, "N_TH %d N_ELEMS %d N_PER_TX %d ", N_TH, N_ELEMS, N_ELEMS_PER_TX );fflush(out_f);	}




	struct timeval tv;	gettimeofday( &tv, NULL);	srandom( tv.tv_usec );
	
	//avem chunks de cate n_elems, 1 chunk per thread, and a final shared chunk.
	cnt = new tm_int[(N_TH + 1) * N_ELEMS+1];
	int* marks = new int[(N_TH + 1 ) * N_ELEMS]; //this prevents the same element being accessed twice in a transaction
	for( i = 0; i < (N_TH + 1) * N_ELEMS+1; i++)		cnt[i] = 0;
	for( i = 0; i < N_TH; i++ )
	{
		read_parts[i] = new int[(N_TH + 1) * N_ELEMS+1]; //testeaza correctness
		for( j = 0; j < (N_TH + 1) * N_ELEMS+1; j++ )		read_parts[i][j] = 0;
		
		to_acc[i] = new int[N_ELEMS_POOL];
	}
	
	CREATE_TM_THREADS( N_TH );
	
	for( i_runs = 0 ; i_runs < N_RUNS; i_runs++  )
	{
		// generate random accesses

		#ifdef DYNAMIC
		_WR_QUOTA = RUN_WQ[ i_runs ];
		_DELAY = RUN_DELAYS[ i_runs ];

		printf("\n");	if( out_f )		fprintf( out_f, "\n" );
		#endif

		printf( "WR_Q %d DELAY %d ", _WR_QUOTA, _DELAY );
		if( out_f )		fprintf( out_f, "WR_Q %d DELAY %d ", _WR_QUOTA, _DELAY );

		
		for( i = 0; i < N_TH; i++ )
		{
			for( j = 0; j < N_ELEMS * (N_TH + 1); j++ )		marks[j] = 0;
			for( j = 0; j < N_ELEMS_POOL; j++ )
			{
				do{
					k = (RAND()%(N_ELEMS * (N_TH)));
				}while( marks[k] && (N_ELEMS * (N_TH)) >= N_ELEMS_POOL );
				marks[k] = 1;
	
				if( (RAND()%100) < _WR_QUOTA )		to_acc[i][j] = -(k+1);
				else								to_acc[i][j] =  (k+1);
			}
		}
		
		print_version( stdout );
		if( out_f ){	print_version( out_f );	}
	
		#ifdef TIMEOUT
		_TIMEOUT = get_c() + t_to_c( timeout );
		#else
		_ITERATE = iterate;
		#endif
		
		PARALLEL_EXECUTE (N_TH, run_assign_ownership, NULL);
		
		PARALLEL_EXECUTE( N_TH, run_work, NULL );
	}

	#ifdef DYNAMIC
	printf("\n");	if( out_f )		fprintf( out_f, "\n" );
	#endif
	
	
	DESTROY_TM_THREADS( N_TH );

	printf(" - ");	if( out_f )		fprintf( out_f, " - " );
	for( i = 0; i < N_TH; i++ )
	{
		printf( "%.4lf ", durations[i] );
		if( out_f )		fprintf( out_f, "%.4lf ", durations[i] );
	}
	printf(" - ");	if( out_f )		fprintf( out_f, " - " );

	
	tstats_t tot_stats = stats_get_total();

	#ifdef CORRECT
	unsigned long long t_cnt = 0;
	for( i = 1; i < N_ELEMS+1; i++)		t_cnt += cnt[i];
	if( t_cnt != tot_stats.n_writes )
		printf( "\nIncorrect %llu != %llu  \n\n", t_cnt, tot_stats.n_writes );
	#endif

	stats_print( stdout, tot_stats );	fflush(stdout);
	if( out_f ){	stats_print( out_f, tot_stats );	fflush(out_f);	}

	return (EXIT_SUCCESS);
}

