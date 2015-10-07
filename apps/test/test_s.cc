#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>


#include "../../src/tm_threads.h"
#include "../../src/tm_basics.h"
#include "../../src/hrtime.h"

typedef	tm_type<long long>			tm_int;


#define M_N_TH		30
#define M_N_PARTS	10000
int N_TH;
int N_PARTS;
int N_ITER;
int N_CHUNK;


#define RAND()	((unsigned int)random())

volatile unsigned long long tx[M_N_TH] = {0};
volatile unsigned long long t_tx[M_N_TH] = {0};

long long cnt[M_N_PARTS];
tm_int rez;

int run_work( void* arg, int id )
{
    volatile int i, j, l;

	volatile double t2, t1, t_t = 0;
    volatile unsigned long long _tx = 0, _t_tx = 0;

    //struct timeval tv;
    //gettimeofday( &tv, NULL);
    srandom( id+1 );//((id + 1) * tv.tv_sec)  + tv.tv_usec


    
	t1 = get_td();
	for( i = 0; i < N_ITER; i++ )
    {
		for( l = id*N_CHUNK; l < N_PARTS; l+=(N_TH*N_CHUNK) )
		{
			for( j = 0; j < N_CHUNK; j++ )
			{
				int t = l+j;
				int k = i*N_PARTS+l+j;

				ENTER_PHASE( k );
				//printf("set  %d :  %d  %d\n", mtid, phase_table[0], phase_table[1] );fflush(stdout);
				BEGIN_PHASE_TRANSACTION();//k
				_t_tx++;

				if(t%4 == 0)	rez = rez + cnt[t];
				if(t%4 == 1)	rez = rez - cnt[t];
				if(t%4 == 2)	rez = rez * cnt[t];
				if(t%4 == 3)	rez = rez / cnt[t];

				COMMIT_PHASE_TRANSACTION();//n_k
				//printf("com  %d :  %d  %d\n", mtid, phase_table[0], phase_table[1] );fflush(stdout);
				_tx++;
			}
		}
	}

	set_phase( id, SMX );

	t2 = get_td();
	t_t += t2 - t1;
		

	t_tx[id] = _t_tx;
	tx[id] = _tx;

	float ab_rate = (float)(_t_tx-_tx)/(float)(_tx);
    printf( "\n  %x - %lf -- CMTS: %llu ; ABRS: %llu ; AB_RATE: %.2f \n", mtid, t_t, _tx, _t_tx-_tx, ab_rate );

    return 0;
}

int main(int argc, char** argv)
{
    long long i, j, tx_cnt = 0, t_tx_cnt = 0;
	volatile double t2, t1;
	long long _rez = 0;

	if(argc < 5){	printf("usage : ./test_s  N_TH N_PARTS N_ITER N_CHUNK\n");return 0;	}

	N_TH = atoi(argv[1]);
	N_PARTS = atoi(argv[2]);
	N_ITER = atoi(argv[3]);
	N_CHUNK = atoi(argv[4]);

	srandom( 1 );

	rez = 0;
	for( i = 0; i < N_PARTS; i++)
    	cnt[i] = RAND();
	t1 = get_td();
    for( j = 0; j < N_ITER; j++ )
    {
		for( i = 0; i < N_PARTS; i++ )
		{
			if(i%4 == 0)	_rez = _rez + cnt[i];
			if(i%4 == 1)	_rez = _rez - cnt[i];
			if(i%4 == 2)	_rez = _rez * cnt[i];
			if(i%4 == 3)	_rez = (_rez+1) / cnt[i];
		}
	}
	t2 = get_td();

	for( i = 0; i < N_TH; i++)
		set_phase( i, 0 );
	
    CREATE_TM_THREADS( N_TH );
    PARALLEL_EXECUTE( N_TH, run_work, NULL );
    DESTROY_TM_THREADS( N_TH );

	for( i = 0; i < N_TH; i++)
	{
		tx_cnt += tx[i];
		t_tx_cnt += t_tx[i];
	}

	float ab_rate = (float)(t_tx_cnt-tx_cnt)/(float)(tx_cnt);

    printf( "\n%lf - rez = %llu = %llu ;   -- CMTS: %llu ; ABRS: %llu ; AB_RATE: %.2f\n", (t2-t1),(long long)rez, _rez, tx_cnt, t_tx_cnt - tx_cnt, ab_rate);
    
	return (EXIT_SUCCESS);
}

