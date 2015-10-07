#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "../../src/tm.h"
#include "../../src/utils/hrtime.h"


typedef	tm_type<int>         tm_int;


#define N_TH		2			// no of threads accessing the array in paralel

#define N_PARTS		3000			// size of the array
#define N_PARTS_TO_ACC	100			// no of elements to access per transaction
#define WR_QUOTA	30			// percentage of write accesses from the total amount of accesses

#define N_ITER		1000000			// No of iterations

#define RAND()	((unsigned int)random())

unsigned int tx[N_TH] = {0};
unsigned int t_tx[N_TH] = {0};
unsigned int wrs[N_TH] = {0};

tm_int cnt[N_PARTS+1];				// array of ints

int run_work( void* arg, int id )
{
    int i,j, k, aux=0, base;

    volatile double t1, dt, _t1, _dt;
    volatile int _tx = 0, _t_tx = 0, _wrs = 0;
    
    int to_acc[N_PARTS_TO_ACC*10];   // indexes of the elements to be accesed
    
    srandom( id+1 );

    for( k = 0; k < 10; k++ ) {
	for( i = 0; i < N_PARTS_TO_ACC; i++ ) {
	    if( (RAND()%100) < WR_QUOTA ) {
	        to_acc[k*N_PARTS_TO_ACC + i] = -((RAND()%N_PARTS) + 1);		// index for write access
		_wrs++;
	    }
	    else
		to_acc[k*N_PARTS_TO_ACC + i] = ((RAND()%N_PARTS) + 1);			// index for read access
    	}
    }

    
    
    for( k = 0; k < N_ITER; k++ ) {
	base = RAND() % (N_PARTS_TO_ACC*9);

	t1 = get_t();
        BEGIN_TRANSACTION();
		_t_tx++;
		_t1 = get_t();

		if( _t_tx % 100000 == 0 )	printf( "ATEMPTS: %d  - CMTS: %d\n", _t_tx, _tx );

	        for( i = 0, j = to_acc[base+i]; i < N_PARTS_TO_ACC; j = to_acc[++i] )
		{
			if( j > 0 ) 	aux = cnt[j];					// read access
			else		cnt[-j] = aux;					// write access
		}
		
		_dt += (get_t() - _t1);
        COMMIT_TRANSACTION();
	
	dt += (get_t() - t1);
	_tx++;
    }
    

    t_tx[id] = _t_tx;
    tx[id] = _tx;
    wrs[id] = _wrs;

    float ab_rate = (float)(_t_tx-_tx)/(float)(_tx);
    float wr_rate = (float)(100*(_wrs))/(float)(_tx*N_PARTS_TO_ACC);
    printf( "\n%lf = %lf  -- CMTS: %d ; ABRS: %d ; AB_RATE: %.2f  -- WR_RATE: %.2f\n", dt, _dt, _tx, _t_tx-_tx, ab_rate, wr_rate );

    return 0;
}

int main(int argc, char** argv)
{
    unsigned int i, tx_cnt = 0, t_tx_cnt = 0, wrs_cnt = 0;


    CREATE_TM_THREADS( N_TH );
    PARALLEL_EXECUTE( N_TH, run_work, NULL );
    DESTROY_TM_THREADS( N_TH );
    

	for( i = 0; i < N_TH; i++)
	{
		tx_cnt += tx[i];
		t_tx_cnt += t_tx[i];
		wrs_cnt += wrs[i];
	}

	printf( "\ntx = %u; a_tx=%u;  t_tx=%u\n", tx_cnt, t_tx_cnt - tx_cnt, t_tx_cnt );
    
	return (EXIT_SUCCESS);
}

