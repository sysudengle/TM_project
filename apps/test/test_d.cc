#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>


#include "../../src/tm.h"
#include "../../src/utils/hrtime.h"

#define TIMEOUT

//#define DYNAMIC


int N_TH;

int N_ELEMS;
int N_ELEMS_PER_TX;

unsigned int _WR_QUOTA = 0;
unsigned int _DELAY = 0;
unsigned int _ORDER = 0;

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

int N_ELEMS_POOL[2];
volatile int* to_acc[MAX_NO_THREADS][2];
volatile int rw_order[MAX_NO_THREADS];

#ifdef DISABLE_ALL
#define tx_access()								\
{												\
	if( o == 0 ) 	read_x = cnt[j]._t+j;		\
	else			cnt[j]._t = read_x-j;		\
}
#else
#define tx_access()								\
{												\
	if( o == 0 ) 	read_x = cnt[j]+j;			\
	else			cnt[j] = read_x-j;			\
}
#endif



#define check_break_per_attempt()																	\
	if( (_TIMEOUT && !((l++)%100) && get_c() > _TIMEOUT) || ( _ITERATE && l++ >= _ITERATE ) )		\
	{	COMMIT_TRANSACTION();	break;		}

#define private_work()																				\
	for( n1 = 0; n1 < _DELAY/500; n1++ )															\
	{																								\
		for( n2 = 0; n2 < 500; n2++ )    asm volatile("nop");										\
		CHECK_TRANSACTION();																		\
	}



int run_work( void* arg, int id )
{
	volatile int i, j, k, l, _base, base[2], n_acc[2], o;
	volatile unsigned int n1, n2;
	volatile int read_x;
	read_x = 0;
	i = 0; l = 0; n1 = 0; n2 = 0;
	
	n_acc[0] = N_ELEMS_PER_TX * (100 - _WR_QUOTA) / 100;
	n_acc[1] = N_ELEMS_PER_TX * 	   _WR_QUOTA  / 100;
	
	while( 1 )
	{
		_base = RAND();
		base[0] = _base % (N_ELEMS_POOL[0]-n_acc[0]);
		base[1] = _base % (N_ELEMS_POOL[1]-n_acc[1]);
		
		
		BEGIN_TRANSACTION();
			
			check_break_per_attempt();
			
			private_work();
			
			o = rw_order[id];
			for( k = 0, j = to_acc[id][o][base[o]+k]; k < n_acc[o]; j = to_acc[id][o][base[o]+(++k)] )
			{				
				tx_access();
			}
			
			private_work();
			
			o = 1 - o;			
			for( k = 0, j = to_acc[id][o][base[o]+k]; k < n_acc[o]; j = to_acc[id][o][base[o]+(++k)] )
			{
				tx_access();
			}
			
			private_work();
			
		COMMIT_TRANSACTION();
	}

    return 0;
}

int main(int argc, char** argv)
{
	int i, j, k, timeout, iterate, i_runs;
	timeout = -1; iterate = -1;
	
	if( argc < 9 )
	{
		printf("usage: %s CD_VERSION CR_VERSION N_TH N_ELEMS N_ELEMS_PER_TX WR_QUOTA DELAY ORDER [ITERATE | TIMEOUT]\n", argv[0]);
		return 0;
	}
	
	set_version( atoi(argv[1]), atoi(argv[2]) );

	N_TH = atoi( argv[3] );
	N_ELEMS = atoi( argv[4] );
	N_ELEMS_PER_TX = atoi( argv[5] );
	
	_WR_QUOTA = atoi( argv[6] );
	_DELAY = atoi( argv[7] );
	_ORDER = atoi( argv[8] );
	
	if( _ORDER < 0 || _ORDER > 2 )
	{	printf("Incorrect parameters\n");		return 0;	}
	
	FILE* out_f = NULL;
	if( argc == 11 )    out_f = fopen( argv[10], "a" );

	#ifdef TIMEOUT
	timeout = atoi(argv[9]);
	printf("TIMEOUT %d ", timeout );
	if( out_f )		fprintf( out_f, "TIME %d ", timeout );
	#else
	iterate = atoi( argv[9] );
	printf("ITERATE %d ", iterate );
	if( out_f )		fprintf( out_f, "ITERATE %d ", iterate );
	#endif

	printf( "N_TH %d N_ELEMS %d N_PER_TX %d ", N_TH, N_ELEMS, N_ELEMS_PER_TX );fflush(stdout);
	if( out_f ){	fprintf( out_f, "N_TH %d N_ELEMS %d N_PER_TX %d ", N_TH, N_ELEMS, N_ELEMS_PER_TX );fflush(out_f);	}




	struct timeval tv;	gettimeofday( &tv, NULL);	srandom( tv.tv_usec );
	
	cnt = new tm_int[N_ELEMS];
	int* marks = new int[N_ELEMS];
	for( i = 0; i < N_ELEMS; i++)		cnt[i] = 0;
	
	int chunk = (N_TH > 1) ? (N_ELEMS/N_TH) : (N_ELEMS/2); 
	N_ELEMS_POOL[0] = N_ELEMS-chunk;
	N_ELEMS_POOL[1] = chunk;
	for( i = 0; i < N_TH; i++ )			to_acc[i][0] = new int[ N_ELEMS_POOL[0] ];
	for( i = 0; i < N_TH; i++ )			to_acc[i][1] = new int[ N_ELEMS_POOL[1] ];
		
	CREATE_TM_THREADS( N_TH );
	
	for( i_runs = 0 ; i_runs < N_RUNS; i_runs++  )
	{
		// generate random accesses

		#ifdef DYNAMIC
		_WR_QUOTA = RUN_WQ[ i_runs ];
		_DELAY = RUN_DELAYS[ i_runs ];

		printf("\n");	if( out_f )		fprintf( out_f, "\n" );
		#endif

		printf( "WR_Q %d DELAY %d ORDER %d ", _WR_QUOTA, _DELAY, _ORDER );
		if( out_f )		fprintf( out_f, "WR_Q %d DELAY %d ORDER %d ", _WR_QUOTA, _DELAY, _ORDER );

		
		for( i = 0; i < N_TH; i++ )
		{
			for( j = 0; j < N_ELEMS; j++ )		marks[j] = 0;
			
			int jr = 0, jw = 0;
			while( jr + jw < N_ELEMS )
			{
				do{
					k = (RAND()%N_ELEMS);
				}while( marks[k] );
				marks[k] = 1;
	
				if( chunk*i <= k && k < chunk*(i+1) )		to_acc[i][1][jw++] = k;
				else										to_acc[i][0][jr++] = k;
			}
			if( _ORDER < 2 )	rw_order[i] = _ORDER;
			else				rw_order[i] = i % 2;
		}
		
		print_version( stdout );
		if( out_f ){	print_version( out_f );	}
	
		#ifdef TIMEOUT
		_TIMEOUT = get_c() + t_to_c( timeout );
		#else
		_ITERATE = iterate;
		#endif
		PARALLEL_EXECUTE( N_TH, run_work, NULL );
	}

	#ifdef DYNAMIC
	printf("\n");	if( out_f )		fprintf( out_f, "\n" );
	#endif
	
	
	DESTROY_TM_THREADS( N_TH );
	
	tstats_t tot_stats = stats_get_total();

	stats_print( stdout, tot_stats );	fflush(stdout);
	if( out_f ){	stats_print( out_f, tot_stats );	fflush(out_f);	}

	return (EXIT_SUCCESS);
}

