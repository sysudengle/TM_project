#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>



#include "../../src/tm.h"
#include "../../src/utils/hrtime.h"


class LLNode;

typedef tm_type< LLNode*>   tm_p_LLNode;
//typedef  LLNode*   tm_p_LLNode;

class LLNode : public tm_obj
{
	public:
	int m_val;
	tm_p_LLNode m_next;
	
	// ctor
	LLNode(int val = -1) : m_val(val)
	{ m_next._t = NULL; }

	LLNode(int val, LLNode* next) : m_val(val)
	{ m_next._t = next; }

};


int HASH_SIZE;			//1

LLNode** sentinel;

// simple sanity check:  make sure all elements of the list are in sorted order
bool isSane( int val )
{
    bool sane = false;

    BEGIN_TRANSACTION();

		sane = true;
		LLNode* prev = sentinel[val % HASH_SIZE];
		LLNode* curr = prev->m_next;
		
		while (curr != NULL)
		{
			if( prev->m_val >= curr->m_val )
			{
				sane = false;
				break;
			}
	
			prev = curr;
			curr = curr->m_next;
		}

    COMMIT_TRANSACTION();

    return sane;
}
// search function
bool lookup(int val)
{
    bool found = false;

	BEGIN_TRANSACTION();

		LLNode* curr = sentinel[val % HASH_SIZE]->m_next;
	
		while (curr != NULL)
		{
			if (curr->m_val >= val)            break;
			curr = curr->m_next;
		}
	
		found = ((curr != NULL) && (curr->m_val == val));

    COMMIT_TRANSACTION();

    return found;
}


int insert(int val)
{
	LLNode* curr  = NULL;
	LLNode* prev;

	//BEGIN_TRANSACTION();
		
		prev = sentinel[val % HASH_SIZE];
		curr = prev->m_next;
		
	
		while (curr != NULL)
		{
			if (curr->m_val >= val)            break;
			prev = curr;
			curr = prev->m_next;
		}
		
		if (!curr || (curr->m_val > val))
			prev->m_next = new LLNode(val, curr);
		

	//COMMIT_TRANSACTION();

	return 0;
}

// remove a node if its value == val
int remove(int val)
{
	LLNode* curr  = NULL;
	LLNode* prev;
	
	//BEGIN_TRANSACTION();
	
		prev = sentinel[val % HASH_SIZE];
		curr = prev->m_next;
		
		
		while (curr != NULL)
		{
			if (curr->m_val == val)
			{
				prev->m_next = curr->m_next;
				tm_delete( curr );
				break;
			}
			if (curr->m_val > val)	break;
				
			prev = curr;
			curr = prev->m_next;
		}
		
	//COMMIT_TRANSACTION();

	return 0;
}

#define TIMEOUT


#define N_ELEMS_CONS		1000
#define N_ELEMS_POOL		10000

#define RAND()	(random())

volatile int to_acc[MAX_NO_THREADS][N_ELEMS_POOL];
double durations[MAX_NO_THREADS] = {0.0};


int N_TH = 2;
int N_PER_TX = 2;


int _ITERATE = 0;
unsigned long long _TIMEOUT = 0;


int LOAD_FX;			//3
int INS_QUOTA;			//50



#define break_condition()																		\
	( (_TIMEOUT && !((l++)%100) && get_c() > _TIMEOUT) || ( _ITERATE && l++ >= _ITERATE ) )



int run_work( void* arg, int id )
{
	volatile int i, j, el, base, k, l, done;
	k = 0; l = 0; done = 0;

	//struct timeval tv;gettimeofday( &tv, NULL);
	srandom( id + 1 );//( ((id + 1) * tv.tv_sec)  + tv.tv_usec );

	volatile unsigned long long _start = get_c();

	while( !done )
	{
		base = RAND() % (N_ELEMS_POOL - N_ELEMS_CONS);

		for( i = 0; i < N_ELEMS_CONS; i+=N_PER_TX )
		{
			BEGIN_TRANSACTION();

			if( !break_condition() )
			{
				for( j = 0; j < N_PER_TX; j++ )
				{
					el = to_acc[id][base+i+j];
					if( el > 0 )		insert(el);
					else				remove(-el);
				}
			}
			else	done = 1;

			COMMIT_TRANSACTION();

			if( done )	break;
		}

		if( !id && ( (k++) % 10 == 0 ) )
		{
			for( i = 0; i < HASH_SIZE; i++ )
				if( !isSane( i ) )
					printf( "ERROR: Not in sorted order.\n" );
		}
	}
	durations[id] = c_to_t( get_c() - _start );

	return 0;
}

int main(int argc, char** argv)
{
	int i, j;

	if(argc < 9){
		printf("usage : ./test_m CONF_DET CONF_RES N_TH HASH_SIZE LOAD_FX N_PER_TX INS_QUOTA [TIMEOUT | ITERATE] \n");
		return 0;
	}

	set_version( atoi(argv[1]), atoi(argv[2]) );

	N_TH = atoi(argv[3]);
	HASH_SIZE = atoi(argv[4]);
	LOAD_FX = atoi(argv[5]);
	N_PER_TX = atoi(argv[6]);
	INS_QUOTA = atoi(argv[7]);

	FILE* out_f = NULL;
	if( argc == 10 )    out_f = fopen( argv[9], "a" );


	#ifdef TIMEOUT
	timeout = atoi(argv[8]);
	printf("TIMEOUT %d ", timeout );
	if( out_f )		fprintf( out_f, "TIMEOUT %d ", timeout );
	#else
	_ITERATE = atoi( argv[8] );
	printf("ITERATE %d ", _ITERATE );
	if( out_f )		fprintf( out_f, "ITERATE %d ", _ITERATE );
	#endif

	printf( "N_TH %d HASH_SIZE %d LOAD_FX %d N_PER_TX %d INS_QUOTA %d ", N_TH, HASH_SIZE, LOAD_FX, N_PER_TX, INS_QUOTA );fflush(stdout);
	if( out_f ){	fprintf( out_f, "N_TH %d HASH_SIZE %d LOAD_FX %d N_PER_TX %d INS_QUOTA %d ", N_TH, HASH_SIZE, LOAD_FX, N_PER_TX, INS_QUOTA );fflush(out_f);	}

	print_version( stdout );
	if( out_f ){	print_version( out_f );	}


	int N_ELEMS = HASH_SIZE * LOAD_FX;

	sentinel = new LLNode*[ HASH_SIZE ];
	for( i = 0; i < HASH_SIZE; i++)
		sentinel[i] = new LLNode();


	struct timeval tv;gettimeofday( &tv, NULL);
	srandom( tv.tv_sec + tv.tv_usec );

	for( i = 0; i < N_TH; i++)
	{
		for( j = 0; j < N_ELEMS_POOL; j++ )
		{
			if( (RAND()%100) < INS_QUOTA )		to_acc[i][j] =  ((RAND()%N_ELEMS) + 1);
			else								to_acc[i][j] = -((RAND()%N_ELEMS) + 1);
		}
	}

	#ifdef TIMEOUT
	_TIMEOUT = get_c() + t_to_c( timeout );
	#endif

	CREATE_TM_THREADS( N_TH );
	PARALLEL_EXECUTE( N_TH, run_work, NULL );
	DESTROY_TM_THREADS( N_TH );

	printf(" - ");	if( out_f )		fprintf( out_f, " - " );
	for( i = 0; i < N_TH; i++ )
	{
		printf( "%.4lf ", durations[i] );
		if( out_f )		fprintf( out_f, "%.4lf ", durations[i] );
	}
	printf(" - ");	if( out_f )		fprintf( out_f, " - " );

	tstats_t tot_stats = stats_get_total();
	stats_print( stdout, tot_stats );	fflush(stdout);
	if( out_f ){	stats_print( out_f, tot_stats );	fflush(out_f);	}


	return (EXIT_SUCCESS);
}

