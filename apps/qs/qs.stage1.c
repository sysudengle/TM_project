/*
 * 
 * $Id: qs.c,v 10.3 1997/12/20 23:56:42 alc Exp $
 * Adopted to use PThread model by Chuck on 2005.12.10
 * More adaption work carried by Chuck Zhao on 2006.01.24
 * 
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>


//#define NPROCS 		8
#define NPROCS 			1
#define NBARRIERS 		32			/* max 32 Barriers					*/
#define NCONDS 			4			/* max 4  Conditions				*/
#define NLOCKS      	4 * 1024 	/* max 4K Locks						*/
#define QSLOCKS     	4 * 1024 	/* max 4K Locks						*/
#define TASKSTACKLOCKS  4 * 1024 	/* max 4K Locks						*/
	
int	Tmk_proc_id = 0;			/* current Tmk_proc_id thread	*/
int	Tmk_nprocs  = NPROCS;		/* total number of available threads in pool	*/

// pthread pool
	// pthread context pool
	pthread_t pthreads[NPROCS];
	
	// pthread lock pool
	pthread_mutex_t locks[NLOCKS];
	
	// pthread condition variable pool
	pthread_cond_t conds[NCONDS];

// pthread data used for barrier
pthread_mutex_t mutex_arrival 	= 	PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_arrival	  	= 	PTHREAD_COND_INITIALIZER;
int arrived  = 0;
pthread_mutex_t print_lock    	= 	PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t qslocks[QSLOCKS]= 	{PTHREAD_MUTEX_INITIALIZER};
pthread_mutex_t taskstacklocks[TASKSTACKLOCKS] = {PTHREAD_MUTEX_INITIALIZER};


typedef struct pthread_param_struct_t{
	int threadID;
	// place more pthread-param data here
	// ...

}ParamStruct;

struct timeval start, finish;


#define	MAX_SORT_SIZE	1024*1024
//#define	DEFAULT_SIZE	256*1024
#define	DEFAULT_SIZE	1*1024
#define MAX_TASK_QUEUE	512
#define DEFAULT_BUBBLE	1024
#define BEEP		7
#define	DONE		(-1)

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
	unsigned	left;
	unsigned	right;
} TaskElement;

typedef struct GlobalMemory {
    TaskElement		TaskStack[MAX_TASK_QUEUE];
    unsigned		TaskStackTop;
    unsigned		NumWaiting;
    unsigned 		A[MAX_SORT_SIZE];
	int				TaskStackLock;
} GlobalMemory;

GlobalMemory	*gMem;

//===========================================================================

// No consistency implied by getting through WAITPAUSE!

#define	INITPAUSE(f,x)	pauseInit(&(f), (x))
#define	SETPAUSE(f)		pauseSet(&(f),1)
#define	CLEARPAUSE(f)	pauseSet(&(f),0)
/*
#define	WAITPAUSE(f)	while (!(f).flag) \
				sigpause(0); \
			Tmk_lock_acquire((f).lock); \
			Tmk_lock_release((f).lock);
*/
#define	WAITPAUSE(f)	while (!(f).flag) \
				sigpause(0); \
			pthread_qs_lock((f).lock); \
			pthread_qs_unlock((f).lock);

typedef struct  {
    volatile int	flag;		// Must be in private memory
	int				lock;		// Because of the above, care must 
								// be taken to initialize this on 
								// all procs
} pauseFlagT;

pauseFlagT		pauseFlag;



//===========================================================================
int size = DEFAULT_SIZE;
int BubbleThresh = DEFAULT_BUBBLE;
int Sequential, verify;
int debug, performance, printed_usage, quiet, dump_array, bubble;

char	dumpfilename[32];

// Lots of variables for collecting timing stats.
int	start_stime, end_stime,	// Times spent executing server processes 
	start_utime, end_utime,	// Times spent executing user processes   
	start_retrans, end_retrans; // Number of packet retransmissions.  
int	start_time, end_time, start_clicks, end_clicks;
double	total_time;

//////////////////////////////////////////////////////////////////////////////

// **************************************************************************

//
// PThread function prototypes
//
void pthread_barrier(void);
void pthread_errexit(char *);
void * thread_work(void *);
void pthread_qs_lock(int);
void pthread_qs_unlock(int);
void pthread_taskstack_lock(int);
void pthread_taskstack_unlock(int);

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
void pauseInit(pauseFlagT *, int);

void showMode(int);
void InitData(void);

// **************************************************************************

//////////////////////////////////////////////////////////////
//
//	a default barrier, without taking a barrier id
//
//////////////////////////////////////////////////////////////
void pthread_barrier(){

	pthread_mutex_lock(&mutex_arrival);

	arrived++;
	if(arrived < Tmk_nprocs){
		pthread_cond_wait(&cond_arrival, &mutex_arrival);			
	}
	else{
		pthread_cond_broadcast(&cond_arrival);
		arrived = 0; // ready for next barrier
	}

	pthread_mutex_unlock(&mutex_arrival);

}


//
//	void pthread_qs_lock(int);	
//  use pthread mutex lock to support Tmk_lock(int)
//
void pthread_qs_lock(int lock_id){
	
	// check lock_id within range
	if(lock_id <0 || lock_id >= QSLOCKS){
		printf("lock_id %d not within [0..%d]\n", lock_id, QSLOCKS);
		printf("no pthread locking performed\n");
		return;
	}
	
	// actually perform the lock operation 
	pthread_mutex_lock(&qslocks[lock_id]);
	
}
	
//
//	void pthread_qs_unlock(int);
//	use pthread_mutex_unlock to support Tmk_unlock(int);
//
//
void pthread_qs_unlock(int lock_id){
	
	// check lock_id within range
	if(lock_id <0 || lock_id >= QSLOCKS){
		printf("lock_id %d not within [0..%d]\n", lock_id, QSLOCKS);
		printf("no pthread unlocking performed\n");
		return;
	}
	
	// actually perform the unlock operation
	pthread_mutex_unlock(&qslocks[lock_id]);
	
}

//
//	void pthread_taskstack_lock(int);	
//  use pthread mutex lock to support Tmk_lock(int)
//
void pthread_taskstack_lock(int lock_id){
	
	// check lock_id within range
	if(lock_id <0 || lock_id >= TASKSTACKLOCKS){
		printf("lock_id %d not within [0..%d]\n", lock_id, TASKSTACKLOCKS);
		printf("no pthread locking performed\n");
		return;
	}
	
	// actually perform the lock operation 
	pthread_mutex_lock(&taskstacklocks[lock_id]);
	
}
	
//
//	void pthread_taskstack_unlock(int);
//	use pthread_mutex_unlock to support Tmk_unlock(int);
//
//
void pthread_taskstack_unlock(int lock_id){
	
	// check lock_id within range
	if(lock_id <0 || lock_id >= TASKSTACKLOCKS){
		printf("lock_id %d not within [0..%d]\n", lock_id, TASKSTACKLOCKS);
		printf("no pthread unlocking performed\n");
		return;
	}
	
	// actually perform the unlock operation
	pthread_mutex_unlock(&taskstacklocks[lock_id]);
	
}

//
//
//	void pthread_errexit(char * msg);
//
//
void pthread_errexit(char * msg){
	
	printf("%s\n", msg);	// print the error msg
	exit(-1);				// exit with error

}

//
//
//	void showMode(int mode_type);
//
//
void showMode(int mode_type){
	
	if(mode_type){
		printf("Sequential mode\n");
	}
	else{
		printf("Parallel mode\n");	
	}
	
}

//
//
//	void InitData(void);
//
//	Note: only 1 thread will need to do it
//
void InitData(void){
	int i;
	
	gMem = (GlobalMemory *)malloc(sizeof(GlobalMemory));
	if(gMem == NULL){
		fprintf(stderr, "Unable to allocate memory for gMem\n");
		exit(-1);	
	}
	memset(gMem, '\0', sizeof(gMem));
	
	// Create locks and give hints.
	if (!quiet){
		printf("\tROOT: Creating locks, barriers, and condition vars.\n");
	}

	#ifdef	NOTDEF
	// Chuck's comments: not defined in the source code?
	// not sure if they are part of TMK system call
		//TaskStackLock = CreateLock();	// Lock number 0x1000
		//AssociateDataAndSynch(TaskStack, TaskStackLock);
		//AssociateDataAndSynch(&gMem->TaskStackTop, TaskStackLock);
	#endif	//	NOTDEF

	INITPAUSE(pauseFlag, 2);
	gMem->TaskStackLock = 1;

	// All of the elements are unique.
	for (i = 0; i < size; i++){
		gMem->A[i] = i;
	}
	
	//Shuffle them randomly.
	#ifdef	SWAP
		for (i = 0; i < size; i++)	{
			SWAP(gMem->A, i, (lrand48() % (size-i)) + i);
		}
	#else
		for (i = 0; i < size; i++)	{
			Swap(i, (random() % (size-i)) + i);
		}
	#endif

	gMem->TaskStackTop = 0;
	gMem->NumWaiting = 0;
	
	// Special: On this node, there are *2* threads (worker and master). 
	if (!quiet){
	   printf("\tSorting %d-entry array on %d procs! Bubble thresh: %d.\n",
				size, Tmk_nprocs, BubbleThresh);
	}
	
	PushWork(0, size-1);	// Push the initial value. 

}


//////////////////////////////////////////////////////////
//
//	The parallel entry of work that each thread
//	will take after being created successfully
//
///////////////////////////////////////////////////////////
void * thread_work(void * param_in){
	ParamStruct *theParam = (ParamStruct *) (param_in);
	int threadID = theParam->threadID;
		
	//printf("Hello, thread %d\n", threadID);
	
	// front barrier: make sure all thread have arrived
	pthread_barrier();
	
	if(threadID == 0){
    	gettimeofday(&start, NULL);
	}

	//Note: each thread, when entering Worker() method, will try to grap a share of the load on the work queue
	// So, there is no explicit data-partition carried on from the invocation site	
    Worker();

	// back barrier: make sure all threads have finished, before returning
    pthread_barrier();
    
    if(threadID == 0){
    	gettimeofday(&finish, NULL);
    }


	return NULL;
}

//
//
//	void pauseInit(pauseFlagT * , int);
//
//
void pauseInit(pauseFlagT *flag, int lockId){
	
    flag->lock = lockId;
    flag->flag = 0;
	//Tmk_distribute((char *)flag, sizeof(*flag));

}


//
//
//	void pauseSet(pauseFlagT *, int);
//
//
void pauseSet(pauseFlagT *flag, int val){
	
	//Tmk_lock_acquire(flag->lock);
	pthread_qs_lock(flag->lock);
	flag->flag = val;	
	//Tmk_distribute((char *)&flag->flag, sizeof(flag->flag));
	//Tmk_lock_release(flag->lock);
	pthread_qs_unlock(flag->lock);

}


//
//
//	void usage(void);
//
//
void usage(void){
	
    if (!printed_usage){
		fprintf(stderr,"qs -n<hosts> -s<sz> -b<bubble-thres> -d<debug>\n");
    }
    printed_usage = 1;

}


//
//  By default, user_init is called before the remote hosts are spawned,
//  so any changes to "non-shared" data items that are performed here get
//  propagated to the children.
//
//  Note: you CAN NOT create threads here, because the remote hosts have
// not been created.  Basically, this should contain the sequential
// initialization code for the program.
//
//
int main(unsigned argc, char **argv){
    char 	hostname[128];
    FILE 	*fp;
    char 	*cp;
    unsigned 	start_time, start_clicks, end_time, end_clicks;
    unsigned 	init_time, init_clicks;
    double 	total_time, create_time, compute_time;
    int 	i, j, c;
    struct timeval	start, finish;
    extern char	*optarg;
	int rc, status;
	ParamStruct param[NPROCS];
	
    while ((c = getopt(argc, argv, "s:vb:dDpqB")) != -1) {
		int done = 0;
		
		switch (c) {
			case 's':
		    	if ((size = atoi(optarg)) > MAX_SORT_SIZE) {
					//Tmk_errexit("Max sort size is %d\n", MAX_SORT_SIZE);
					char buffer[100];
					memset(buffer, '\0', 100);
					sprintf(buffer, "Max sort size is %d\n", MAX_SORT_SIZE);
					pthread_errexit(buffer);
		    	}
		    	done++; 
		    	break;
		  	case 'b': 
		  		BubbleThresh = atoi(optarg); done++; 
		  		break;
		  	case 'B': 
		  		bubble = 1; 
		  		break;
		  	case 'd': 
		  		debug++; 
		  		break;
		  	case 'D': 
		  		dump_array++; 
		  		break;
		  	case 'p': 
		  		performance = 1; 
		  		break;
		  	case 'q': 
		  		quiet = 1; 
		  		break;
		  	case 'v': 
		  		verify = 1; 
		  		break;
		}
    }
	//Tmk_startup(argc, argv);
    
    Sequential = (Tmk_nprocs == 1);
	showMode(Sequential);
    
	InitData();    
    
// 
//	REAL work starts here
//	
	// build PThreads' parameters' list
	for(i= 0; i< NPROCS; i++){
		param[i].threadID  = i;
	}

	// create parallel threads to work on the tasks
	for(i= 0; i< NPROCS; i++){
		pthread_create(&pthreads[i], NULL, thread_work, (void *) (&param[i]));
		// code inside thread_work starts parallel work
	}

/*
	pthread_barrier();
	
    gettimeofday(&start, NULL);

    Worker();

    pthread_barrier();
    
    gettimeofday(&finish, NULL);
    printf("Elapsed time: %.2f seconds\n",
		   (((finish.tv_sec * 1000000.0) + finish.tv_usec) -
	    	((start.tv_sec * 1000000.0) + start.tv_usec)) / 1000000.0);
*/

	// wait for all threads to finish their work before continue
	for(i=0; i < NPROCS; i++){
		rc = pthread_join(pthreads[i], (void **)&status);
	
		if (rc){
			printf("ERROR; return code from pthread_join() of thread %d is %d\n", i, rc);
			exit(-1);
		}

	}

	// all pthreads joined, sequential mode resumed
    if (dump_array) {
    	DumpArray();
    }
    else{
    	printf("DumpArray not called\n");	
    }
    
    if (verify) {
		for (i = 1; i < size; i++) {
		    if (gMem->A[i] < gMem->A[i-1]) {
				fprintf(stderr, "bad sort %d\n", i);
		    }
		}
	
    }
    else{
    	printf("not verified\n");	
    }

//
//	REAL work ends here
//
	//Tmk_exit(0);
	return 0;
	
}

//
//
//	void PushWork(int , int);
//
//
void PushWork(int i, int j){
    
    if (!Sequential) {
		//Tmk_lock_acquire(gMem->TaskStackLock);
		pthread_taskstack_lock(gMem->TaskStackLock);
    }
    
    if (gMem->TaskStackTop == MAX_TASK_QUEUE - 1) {
		fprintf(stderr, "ERROR: Work stack overflow (%d entries)!\n",
						gMem->TaskStackTop);
		exit(-1);
    }
    
    gMem->TaskStack[gMem->TaskStackTop].left = i;
    gMem->TaskStack[gMem->TaskStackTop].right = j;
    gMem->TaskStackTop++;
    
    if (debug) {
    	fprintf(stderr, "\t%d: PushingWork - pushed <%d,%d>.  Top now %d.\n",
				Tmk_proc_id, i,j, gMem->TaskStackTop);
    }
    
    if (!Sequential) {
		if (gMem->TaskStackTop == 1){
		    SETPAUSE(pauseFlag);
		}
		//Tmk_lock_release(gMem->TaskStackLock);
		pthread_taskstack_unlock(gMem->TaskStackLock);
    }

}

//
//
// Return the array indices of a sub-array that needs sorting. 
//	int PopWork(TaskElement *);
//
//
int PopWork(TaskElement * task){
    int i;

    if (!Sequential) {
		//Tmk_lock_acquire(gMem->TaskStackLock);
		pthread_taskstack_lock(gMem->TaskStackLock);

		while (gMem->TaskStackTop == 0) {		// Check for empty stack!
		    if (++gMem->NumWaiting == Tmk_nprocs) {
				// DONE
				SETPAUSE(pauseFlag);
				//Tmk_lock_release(gMem->TaskStackLock);
				pthread_taskstack_unlock(gMem->TaskStackLock);
				return(DONE);
		    }
		    else {
				// Wait for some work to get pushed on... 
				if (gMem->NumWaiting == 1){
			    	CLEARPAUSE(pauseFlag);
				}
				//Tmk_lock_release(gMem->TaskStackLock);
				pthread_taskstack_unlock(gMem->TaskStackLock);
	
				if (debug){
					fprintf(stderr,"\t%d: PopWork waiting for work.\n",Tmk_proc_id);
				}
				
				WAITPAUSE(pauseFlag);
				//Tmk_lock_acquire(gMem->TaskStackLock);
				pthread_taskstack_lock(gMem->TaskStackLock);

				if (gMem->NumWaiting == Tmk_nprocs) {
					//Tmk_lock_release(gMem->TaskStackLock);
					pthread_taskstack_unlock(gMem->TaskStackLock);
					return(DONE);
				}
				--gMem->NumWaiting;
		    }// end of else
		} // while task-stack empty 
    }
    else // non-sequential
    if (gMem->TaskStackTop == 0) {
    	return(DONE);
    }

    gMem->TaskStackTop--;
    task->left  = gMem->TaskStack[gMem->TaskStackTop].left;
    task->right = gMem->TaskStack[gMem->TaskStackTop].right;

    if (debug) {
    	fprintf(stderr,"\t%d: PopWork - returning <%d,%d>.  Top now %d.\n",
				Tmk_proc_id, task->left, task->right,
		      	gMem->TaskStackTop);
    }
    
    if (!Sequential) {
		//Tmk_lock_release(gMem->TaskStackLock);
		pthread_taskstack_unlock(gMem->TaskStackLock);
    }

    return(0);

}

//
//
// Partition the array between elements i and j around the specified pivot.
//	int Partition(int i, int j, int pivot);
//
//
int Partition(int i, int j, int pivot){
    int left, right;

    left = i;
    right = j;
	
    do {
		#ifdef	SWAP
			SWAP(gMem->A, left, right);
		#else
			Swap(left,right);
		#endif

		while (gMem->A[left]  <  pivot) {
			left++;
		}
		
		while ((right >= i) && (gMem->A[right] >= pivot)) {
			right--;
		}
		
    } while (left <= right);

    return(left);
}


//
//
//	void LocalQuickSort(int,int);
//
//
void LocalQuickSort(int i, int j){
    int pivot, k;

    if (j <= i) {
    	return;
    }
    
    if (debug) {
    	fprintf(stderr,"\t%d: Local QuickSorting %d-%d.\n", Tmk_proc_id, i, j);
    }

    pivot = FindPivot(i,j);	
	//pivot = (i + j) >> 1;

    k = Partition(i,j,gMem->A[pivot]);
    
    if (debug) {
    	fprintf(stderr,"\t%d: Splitting at %d-%d:%d-%d\n", Tmk_proc_id,i,k-1,k,j);
    }

    if (k > i) {
		LocalQuickSort(i, k-1);
		LocalQuickSort(k,	j);
    }
    else {
		LocalQuickSort( i + 1, j);
    }
    
}

//
//
// Return the largest of first two keys
//	int FindPivot(int i, int j);
//
//

int FindPivot(int i, int j){

    if (gMem->A[i] > gMem->A[i+1]) {
    	return i;
    }
    else {
    	return i+1;
    }

}
	

#ifndef	SWAP
// Swap two elements in the array being sorted.
void Swap(int i, int j){
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
//	void BubbleSort(int i, int j)
//
void BubbleSort(int i, int j){
    register int x, y, tmp;
	
    if (debug) {
    	fprintf(stderr,"\t%d: BubbleSorting %d-%d.\n", Tmk_proc_id, i, j);
    }

    for (x = i; x < j; x++) {
		for (y = j; y > i; y--)	{
			#ifdef fast
				if (gMem->A[y] < gMem->A[y-1]) {
					tmp = gMem->A[y];
					gMem->A[y] = gMem->A[y-1];
					gMem->A[y-1] = tmp;
				}
			#else

				#ifdef	SWAP
					unsigned *P = gMem->A;
					if (P[y] < P[y-1]) {
						SWAP(P, y, y-1);
					}
				#else
					if (gMem->A[y] < gMem->A[y-1]) {
						Swap(y, y-1);
					}
				#endif
			
			#endif
		}
    }

}


//
//
//	void QuickSort(int,int);
//
//
void QuickSort(int i, int j){
    int pivot, k;

    if (debug) {
    	fprintf(stderr,"\t%d: QuickSorting %d-%d.\n", Tmk_proc_id, i, j);
    }

    // pivot is index of the pivot element
    if (j-i+1 < BubbleThresh) {
		if (bubble) {
			BubbleSort(i,j);
		}
		else {
			LocalQuickSort(i,j);
		}
		return;
    } 
    pivot = FindPivot(i,j);

    k = Partition(i,j,gMem->A[pivot]);

    if (debug) {
    	fprintf(stderr,"\t%d: Splitting at %d-%d:%d-%d\n", Tmk_proc_id,i,k-1,k,j);
    }
    
	#define FAST_SEQUENTIAL
	#ifdef FAST_SEQUENTIAL
	    if (Sequential) {
			QuickSort(i, k-1);
			QuickSort(k,j);
			return;
	    }
	#endif //	FAST_SEQUENTIAL
	
	if (k-i > j-k) {
		// The lower half [i through (k-1)] contains more entries.	
		// Put the other half into the queue of unsorted subarrays	
		// and recurse on the other half.			
		PushWork(k,j);
		QuickSort(i, k-1);
    }
    else {
		// The lower half [i through (k-1)] contains more entries.
		// Put the other half into the queue of unsorted subarrays
		// and recurse on the other half.			
		PushWork(i,k-1);
		QuickSort(k,j);
    }

}



//
//
//	void DumpArray(void);
//
//
void DumpArray(void){
    int i;

    for(i = 0; i < size; i++) {
		if ((i % 10) == 0 && (i != size-1)) {
			printf("\n\t");
		}
		printf("%5d ", gMem->A[i]);
    }
    
    printf("\n\n");
}


//
//
//	void Worker(void);
//
//
void Worker(void){
    int curr = 0, i, last;
    TaskElement task;

    // Wait for signal to start. 
    if (!Sequential) {
    	//Tmk_barrier(0);
    	pthread_barrier();
    }
	//GetProcessorUsage(&start_stime, &start_utime, &start_retrans);

    for (;;) {
		// Continuously get a sub-array and sort it
		if (PopWork(&task) == DONE) {
			break;
		}

		QuickSort(task.left, task.right);
    }

    if (debug) {
    	fprintf(stderr,"\t%d: DONE.\n", Tmk_proc_id);
    }

    if (!Sequential) {
		//Tmk_barrier(0);	// Signal completion.
		pthread_barrier();	// Signal completion.
    }

}

