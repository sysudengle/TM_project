/*
 * 
 * $Id: qs.c,v 10.3 1997/12/20 23:56:42 alc Exp $
 * Adopted to use PThread model by Chuck on 2005.12.10
 * More adaption work carried by Chuck Zhao on 2006.01.24
 * 
 * Status: done
 * Working fine on native Debian 2.6 kernel (tested on krusty and buserror)
 * However, it has instable behaviors on my own development workstations (Win + Eclipse or Win + Cygwin)
 * behaviors:
 *   - Win XP + Eclipse: not printing results for large value of N
 *   - Win XP + Cygwin:  Error: work stack overflow (511 entries)
 *     I think this has to do with the same Cygwin arranges stack + schedule threads
 *     (it is fine, as it won't hurt on the real Linux workstations).
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#include "../../src/tm.h"


#define	MAX_SORT_SIZE	1024*1024*32
#define	DEFAULT_SIZE	128*1024
#define MAX_TASK_QUEUE	1024
#define DEFAULT_BUBBLE	1024
#define BEEP		7
#define	DONE		(-1)


#define NPROCS 			2

	
__thread int	Tmk_proc_id = 0;			/* current Tmk_proc_id thread	*/
int	Tmk_nprocs  = NPROCS;					/* total number of available threads in pool	*/
pthread_t pthreads[NPROCS];

tm_int arrived  = 0;
tm_int cond_arrived = 0;

tm_int cond_stack = 0;

typedef struct pthread_param_struct_t
{
	int threadID;
}ParamStruct;


/*
 * Delete this macro to use the swap procedure.  The Midway/EC version
 * of Quicksort used this macro.
 *  Patched to work in initialization loop.
 */
#define SWAP(A,I,J) { int j = (J); int tmp = A[(I)]; A[(I)] = A[j]; A[j] = tmp; }

/*
 * This specifies the range of an unsorted subarray of A[].
 */
typedef struct pq {
	tm_int	left;
	tm_int	right;
} TaskElement;

typedef struct _GlobalMemory {
	TaskElement		TaskStack[MAX_TASK_QUEUE];
	tm_int			TaskStackTop;
	tm_int			NumWaiting;
	unsigned int 	A[MAX_SORT_SIZE];
} GlobalMemory;

GlobalMemory	*gMem;


//===========================================================================
int size = DEFAULT_SIZE;
int BubbleThresh = DEFAULT_BUBBLE;
int Sequential = 0, verify = 0;
int debug = 0, performance = 0, printed_usage = 0, quiet = 0, dump_array = 0, bubble = 0;

char	dumpfilename[32];
struct timeval start, finish;

// **************************************************************************

//
// PThread function prototypes
//
void pthread_barrier(void);
void pthread_errexit(char *);
void * thread_work(void *);

//
// User function prototypes
//
void PushWork(int, int);
int  PopWork(TaskElement *);
int  Partition(int,int,int);
void LocalQuickSort(int, int);
int  FindPivot(int , int );
void BubbleSort(int , int);
void QuickSort(int , int );
void DumpArray(void);
void Worker(void);
void usage(void);

void showMode(int);
void InitData(void);
void ClearData(void);

// **************************************************************************

//////////////////////////////////////////////////////////////
//
//	a default barrier, without taking a barrier id
//
//////////////////////////////////////////////////////////////
void pthread_barrier()
{
	BEGIN_TRANSACTION();
	
	arrived = arrived + 1;
	
	if( arrived < Tmk_nprocs )
	{
		TM_WAIT_CONDITION( cond_arrived );
	}
	else
	{
		TM_SIGNAL_CONDITION( cond_arrived, Tmk_nprocs-1 );
		arrived = 0; // ready for next barrier
	}
	
	COMMIT_TRANSACTION();
}

void pthread_errexit(char * msg)
{
	printf("%s\n", msg);	// print the error msg
	exit(-1);				// exit with error
}

void showMode(int mode_type)
{
	if(mode_type)	printf("Sequential mode\n");
	else			printf("Parallel mode\n");
}

void InitData(void)
{
	int i;
	
	//gMem = (GlobalMemory *) malloc(sizeof(GlobalMemory));
	gMem = new GlobalMemory;
	if( gMem == NULL )
	{
		fprintf(stderr, "Unable to allocate memory for gMem\n");
		exit(-1);	
	}

	//memset(gMem, '\0', sizeof(gMem));
	for( i = 0; i < MAX_TASK_QUEUE; i++ )
	{
		gMem->TaskStack[i].left = 0;
		gMem->TaskStack[i].right = 0;
	}

	// All of the elements are unique.
	for (i = 0; i < size; i++)		gMem->A[i] = i;
	
	//Shuffle data randomly.
#ifdef	SWAP
	for( i = 0; i < size; i++ )
		SWAP(gMem->A, i, (lrand48() % (size-i)) + i);
#else
	for( i = 0; i < size; i++ )
		Swap(i, (random() % (size-i)) + i);
#endif

	gMem->TaskStackTop = 0;
	gMem->NumWaiting = 0;

	
	if (!quiet)
		printf("\tSorting %d-entry array on %d procs! Bubble thresh: %d.\n", size, Tmk_nprocs, BubbleThresh);
	
	// Push the initial value. 
	PushWork(0, size-1);
}

void ClearData(void)
{
	if(gMem != NULL)
	{
		delete gMem;
		gMem = NULL;
	}
}

void usage(void)
{	
	if( !printed_usage )
		fprintf(stderr,"qs -n<hosts> -s<sz> -b<bubble-thres> -d<debug> -v\n");
	printed_usage = 1;
}

void VerifyArray(void)
{
	int i;	
	int res = 1;
	
	for (i = 1; i < size; i++)
	{
		if (gMem->A[i] < gMem->A[i-1])
		{
			fprintf(stderr, "bad sort %d\n", i);
			res = 0;
			break;
		}
    }

	if(res == 1)	printf("verified, result is good\n");
	else			printf("verified, result is bad\n");
}


void* thread_work(void * param_in)
{
	ParamStruct* theParam = (ParamStruct *) (param_in);
	Tmk_proc_id = theParam->threadID;
	
	if(Tmk_proc_id == 0)    	gettimeofday(&start, NULL);
	pthread_barrier();
	
	Worker();
	
	pthread_barrier();
	if(Tmk_proc_id == 0)    	gettimeofday(&finish, NULL);
	return NULL;
}

int main(unsigned argc, char **argv)
{
	char 	*cp;
	int 	i, j, c;
	extern char	*optarg;
	int rc, status;
	ParamStruct param[NPROCS];

	while( (c = getopt(argc, argv, "s:vb:dDpqBh")) != -1 )
	{
		int done = 0;

		switch (c) 
		{
			case 's':
				if( (size = atoi(optarg)) > MAX_SORT_SIZE )
				{
					char buffer[100];	memset(buffer, '\0', 100);
					sprintf(buffer, "Max sort size is %d\n", MAX_SORT_SIZE);
					pthread_errexit(buffer);
				}
				done++;break;
			case 'b': 
				BubbleThresh = atoi(optarg);
				done++;break;
			case 'h':	usage();		break;
			case 'B':	bubble = 1;		break;
			case 'd':	debug = 1;		break;
			case 'D':	dump_array = 1;	break;
			case 'p':	performance = 1;break;
			case 'q':	quiet = 1;		break;
			case 'v':	verify = 1;		break;
		}
    }

	Sequential = (Tmk_nprocs == 1);
	showMode(Sequential);

	set_version( 2, 2 );
	print_version( stdout );printf("\n");

	InitData();


	// build PThreads' parameters' list
	for(i= 0; i< NPROCS; i++)	param[i].threadID  = i;

	// create parallel threads to work on the tasks
	for(i= 0; i< NPROCS; i++)	pthread_create(&pthreads[i], NULL, thread_work, (void *) (&param[i]));

	// wait for all threads to finish their work before continue
	for(i=0; i < NPROCS; i++)
	{
		rc = pthread_join(pthreads[i], (void **)&status);
		if (rc){
			printf("ERROR; return code from pthread_join() of thread %d is %d\n", i, rc);
			exit(-1);
		}
    }

	printf("\nElapsed time: %.2f seconds\n", (((finish.tv_sec * 1000000.0) + finish.tv_usec) -
				((start.tv_sec * 1000000.0) + start.tv_usec)) / 1000000.0);

	// all pthreads joined, sequential mode resumed
	if(dump_array)		DumpArray();
	if(verify)			VerifyArray();

	ClearData();
	return 0;
}


extern __thread    int p_thread_id;
extern __thread    int __thread_id;
extern uint_t		volatile	aborted_table[ MAX_NO_THREADS*16 ];


//
//
//	void PushWork(int , int);
//
//
void PushWork(int i, int j)
{
	volatile int top, top0, top1, top2, tid;

	assert( i <= j );

	BEGIN_TRANSACTION();

	tid = __thread_id;

	top0 = gMem->TaskStackTop;
	
	/*
	if( (aborted_table[tid*16] == 0) && (( gMem->TaskStackTop.__meta[0] & (1<<tid) ) == 0) )
	{
		printf("%d - %d != %d ; aborted_table: %d  gMem->TaskStackTop.__meta: %x %x\n", tid, top1, gMem->TaskStackTop._t,
					aborted_table[tid*16],
					gMem->TaskStackTop.__meta[0], gMem->TaskStackTop.__meta[1] );fflush(stdout);
	}
	assert( aborted_table[tid*16] || ( gMem->TaskStackTop.__meta[0] & (1<<tid) ) );
	*/

	if( top0 >= MAX_TASK_QUEUE - 1 )
	{
		fprintf(stderr, "ERROR: Work stack overflow (%d entries)!\n", (int)gMem->TaskStackTop);
		exit(-1);
	}

	//sched_yield();

	top1 = gMem->TaskStackTop;
	
	sched_yield();
	
	top2 = gMem->TaskStackTop;

	if( top1 != top2 )
	{
		printf("%d - %d != %d ; aborted_table: %d  gMem->TaskStackTop.__meta: %x %x\n", __thread_id, top1, top2,
					aborted_table[__thread_id*16],
					gMem->TaskStackTop.__meta[0], gMem->TaskStackTop.__meta[1] );fflush(stdout);
	}

	assert( top1 == top2 );

	gMem->TaskStack[top1].left = i;
	gMem->TaskStack[top2].right = j;

	sched_yield();
	
	top = gMem->TaskStackTop;
	
	if( top != top1 )
	{
		printf("%d - %d != %d ; aborted_table: %d  gMem->TaskStackTop.__meta: %x %x\n", p_thread_id, top, top1,
					aborted_table[p_thread_id*16],
					gMem->TaskStackTop.__meta[0], gMem->TaskStackTop.__meta[1] );fflush(stdout);
	}

	assert( top == top1 );

	gMem->TaskStackTop = top+1;

	sched_yield();
	
	if( gMem->TaskStackTop == 1 )
		TM_SIGNAL_CONDITION( cond_stack, 1 );

	//sched_yield();
	
	COMMIT_TRANSACTION();

	if(debug)	fprintf(stderr, "\t%d-Pushed %d,%d Top %d\n", Tmk_proc_id, i,j, top );
}

//
//
// Return the array indices of a sub-array that needs sorting. 
//	int PopWork(TaskElement *);
//
//
int PopWork(TaskElement * task)
{
	int rez = 0;
	volatile int pop_top, pop_top1, pop_top2;
	
	BEGIN_TRANSACTION();

	
	while( gMem->TaskStackTop == 0 )		// Check for empty stack!
	{
		gMem->NumWaiting = gMem->NumWaiting + 1;
		if( gMem->NumWaiting == Tmk_nprocs) 
		{
			if( debug )		fprintf(stderr,"\t%d: PopWork last to find stack empty.\n",Tmk_proc_id);
			
			//sched_yield();

			TM_SIGNAL_CONDITION( cond_stack, Tmk_nprocs-1 );
			rez = (DONE);
			break;
		}
		else
		{
			// Wait for some work to get pushed on... 
			if( debug )		fprintf(stderr,"\t%d: PopWork waiting for work.\n",Tmk_proc_id);
			
			//sched_yield();

			TM_WAIT_CONDITION( cond_stack );
	
			if( gMem->NumWaiting == Tmk_nprocs )
			{
				if (debug)  fprintf(stderr,"\t%d: PopWork stack empty and all waiting.\n",Tmk_proc_id);
				rez = (DONE);
				break;
			}

			//sched_yield();

			//tm_uint two = 2;
			//gMem->NumWaiting -= two;
			gMem->NumWaiting = gMem->NumWaiting - 1;

			//sched_yield();
		}
    } // while task-stack empty 


	if( rez != (DONE) )
	{
		if( gMem->TaskStackTop >= MAX_TASK_QUEUE - 1 )
		{
			fprintf(stderr, "ERROR: Work stack overflow(2) (%d entries)!\n", (unsigned)gMem->TaskStackTop);
			exit(-1);
		}

		//sched_yield();
		pop_top = gMem->TaskStackTop - 1;
		gMem->TaskStackTop = (int)pop_top;
		
		
		volatile int aux;

		pop_top1 = gMem->TaskStackTop;
		assert( pop_top == pop_top1 );

		aux  = gMem->TaskStack[pop_top1].left;
		task->left = (int)aux;

		pop_top2 = gMem->TaskStackTop;
		assert( pop_top == pop_top2 );
		aux = gMem->TaskStack[pop_top2].right;
		task->right = (int)aux;

		assert( task->left <= task->right );

	}

	//sched_yield();

	COMMIT_TRANSACTION();

	if( debug && rez != (DONE) )
		fprintf(stderr,"\t%d-Popped %d,%d Top %d\n", Tmk_proc_id, (unsigned)task->left, (unsigned)task->right, pop_top);
	
	return rez;
}

//
// Partition the array between elements i and j around the specified pivot.
//	int Partition(int i, int j, int pivot);
//
int Partition(int i, int j, int pivot)
{
	int left, right;
	
	left = i;
	right = j;
	
	do 
	{
	#ifdef	SWAP
	    SWAP(gMem->A, left, right);
	#else
	    Swap(left, right);
	#endif

		while (gMem->A[left]  <  pivot)
		{
			left++;
		}
	
		while ((right >= i) && (gMem->A[right] >= pivot))
		{
			right--;
		}
		
	}while( left <= right );

	return(left);
}


//
//	void LocalQuickSort(int,int);
//
void LocalQuickSort(int i, int j)
{
	int pivot, k;

	if (j <= i)		return;
	
	//if( debug )	fprintf(stderr,"\t%d: Local QuickSorting %d-%d.\n", Tmk_proc_id, i, j);
	
	pivot = FindPivot(i,j);	
	//pivot = (i + j) >> 1;
	
	k = Partition(i, j, gMem->A[pivot]);
	
	//if( debug )	fprintf(stderr,"\t%d: Splitting at %d-%d:%d-%d\n", Tmk_proc_id,i,k-1,k,j);
	
	if (k > i) {
		LocalQuickSort(i, k-1);
		LocalQuickSort(k, j);
	}
	else 
		LocalQuickSort(i + 1, j);
}

//
// Return the largest of first two keys
//	int FindPivot(int i, int j);
//
int FindPivot(int i, int j)
{
	if(gMem->A[i] > gMem->A[i+1])    	return i;
	else								return i+1;
}


#ifndef	SWAP
	// Swap two elements in the array being sorted.
void Swap(int i, int j)
{
	register int tmp;
	
	tmp = gMem->A[i];
	gMem->A[i] = gMem->A[j];
	gMem->A[j] = tmp;
}
#endif


//
//
// Resort to bubble sort when we get down to the nitty gritty
//
void BubbleSort(int i, int j)
{
	register int x, y, tmp;
	
	if (debug)	fprintf(stderr,"\t%d: BubbleSorting %d-%d.\n", Tmk_proc_id, i, j);

	for(x = i; x < j; x++) 
	{
		for (y = j; y > i; y--)	
		{
			#ifdef fast
			if (gMem->A[y] < gMem->A[y-1])
			{
				tmp = gMem->A[y];
				gMem->A[y] = gMem->A[y-1];
				gMem->A[y-1] = tmp;
			}
			#else

			#ifdef	SWAP
			unsigned *P = gMem->A;
			if( P[y] < P[y-1] )					SWAP(P, y, y-1);
			#else
			if( gMem->A[y] < gMem->A[y-1] )		Swap(y, y-1);
			#endif
	
			#endif
		}
	}
}


//
//	void QuickSort(int,int);
//
void QuickSort(int i, int j)
{
	int pivot, k;

	//if( debug )	fprintf( stderr,"\t%d: QuickSorting %d-%d.\n", Tmk_proc_id, i, j );

	// pivot is index of the pivot element
	if( j-i+1 < BubbleThresh )
	{
		if(bubble)		BubbleSort(i,j);
		else			LocalQuickSort(i,j);

		return;
	}

	pivot = FindPivot( i,j );
	k = Partition( i, j, gMem->A[pivot] );
	
	//if( debug )	fprintf( stderr,"\t%d: Splitting at %d-%d:%d-%d\n", Tmk_proc_id,i,k-1,k,j );

	if( k-i > j-k )
	{
		// The lower half [i through (k-1)] contains more entries.
		// Put the other half into the queue of unsorted subarrays
		// and recurse on the other half.
		PushWork(k,  j);
		QuickSort(i, k-1);
	}
	else 
	{
		PushWork(i,k-1);
		QuickSort(k,j);
	}
}


//
//	void DumpArray(void);
//
void DumpArray(void)
{
	int i;
	for(i = 0; i < size; i++)
	{
		if ((i % 10) == 0 && (i != size-1))	    printf("\n\t");
		printf( "%5d ", gMem->A[i] );
	}
	printf("\n\n");
}

//
//	void Worker(void);
//  this is the entry point of the parallel PThread work, dealing with care
//
void Worker(void)
{
	int curr = 0, i, last;
	TaskElement task;

	for(;;)
	{
		// Continuously get a sub-array and sort it
		if( PopWork( &task ) == DONE )	break;

		QuickSort( task.left, task.right );
	}

	if( debug )	fprintf(stderr,"\t%d: DONE.\n", Tmk_proc_id);
}

