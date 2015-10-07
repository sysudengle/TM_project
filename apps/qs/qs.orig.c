/*
 * $Id: qs.c,v 10.3 1997/12/20 23:56:42 alc Exp $
 */
#include <stdio.h>

#include <sys/time.h>

#include <Tmk.h>

#define	MAX_SORT_SIZE	1024*1024
#define	DEFAULT_SIZE	256*1024
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

    int			TaskStackLock;
} GlobalMemory;

GlobalMemory	*gMem;

/*===========================================================================*/

/* No consistency implied by getting through WAITPAUSE! */

#define	INITPAUSE(f,x)	pauseInit(&(f), (x))
#define	SETPAUSE(f)	pauseSet(&(f),1)
#define	CLEARPAUSE(f)	pauseSet(&(f),0)
#define	WAITPAUSE(f)	while (!(f).flag) \
				sigpause(0); \
			Tmk_lock_acquire((f).lock); \
			Tmk_lock_release((f).lock);

typedef struct  {
    volatile int	flag;		/* Must be in private memory. */
    int			lock;		/* Because of the above, care must */
					/* be taken to initialize this on */
					/* all procs. */ 
} pauseFlagT;

pauseFlagT		pauseFlag;

void pauseInit(pauseFlagT *flag, int lockId) 
{
    flag->lock = lockId;
    flag->flag = 0;
    Tmk_distribute((char *)flag, sizeof(*flag));
}

void pauseSet(pauseFlagT *flag, int val)
{
    Tmk_lock_acquire(flag->lock);
    flag->flag = val;
    Tmk_distribute((char *)&flag->flag, sizeof(flag->flag));
    Tmk_lock_release(flag->lock);
}


/*===========================================================================*/

int size = DEFAULT_SIZE;
int BubbleThresh = DEFAULT_BUBBLE;
int Sequential, verify;
int debug, performance, printed_usage, quiet, dump_array, bubble;

char	dumpfilename[32];

/* Lots of variables for collecting timing stats. */
int	start_stime, end_stime,	/* Times spent executing server processes */
	start_utime, end_utime,	/* Times spent executing user processes   */
	start_retrans, end_retrans; /* Number of packet retransmissions.  */
int	start_time, end_time, start_clicks, end_clicks;
double	total_time;

extern void DumpArray();
extern void PushWork();
extern void Swap();
extern void Worker();

void
usage()
{
    if (!printed_usage)
	fprintf(stderr,"qs -n<hosts> -s<sz> -b<bubble-thres> -d<debug>\n");
    printed_usage = 1;
}


/*
 *  By default, user_init is called before the remote hosts are spawned,
 * so any changes to "non-shared" data items that are performed here get
 * propagated to the children.
 *
 *  Note: you CAN NOT create threads here, because the remote hosts have
 * not been created.  Basically, this should contain the sequential
 * initialization code for the program.
 */
main(argc, argv)
unsigned 	argc;
char 		**argv;
{
    char 	hostname[128];
    FILE 	*fp;
    char 	*cp;
    unsigned 	start_time, start_clicks, end_time, end_clicks;
    unsigned 	init_time, init_clicks;
    double 	total_time, create_time, compute_time;
    int 	i, j, c;
    struct timeval	start, finish;
    extern char	*optarg;
	
    while ((c = getopt(argc, argv, "s:vb:dDpqB")) != -1) {
	int done = 0;
	
	switch (c) {
	  case 's':
	    if ((size = atoi(optarg)) > MAX_SORT_SIZE) {
		Tmk_errexit("Max sort size is %d\n", MAX_SORT_SIZE);
	    }
	    done++; break;
	  case 'b': BubbleThresh = atoi(optarg); done++; break;
	  case 'B': bubble = 1; break;
	  case 'd': debug++; break;
	  case 'D': dump_array++; break;
	  case 'p': performance = 1; break;
	  case 'q': quiet = 1; break;
	  case 'v': verify = 1; break;
	}
    }
    Tmk_startup(argc, argv);
    
    Sequential = (Tmk_nprocs == 1);
    
    if (Tmk_proc_id == 0) {
	gMem = (GlobalMemory *)Tmk_malloc(sizeof(GlobalMemory));
	Tmk_distribute((char *)&gMem, sizeof(gMem));

	/* Create locks and give hints. */
	if (!quiet)
	   printf("\tROOT: Creating locks, barriers, and condition vars.\n");

#ifdef	NOTDEF
	TaskStackLock = CreateLock();		/* Lock number 0x1000 */
	AssociateDataAndSynch(TaskStack, TaskStackLock);
	AssociateDataAndSynch(&gMem->TaskStackTop, TaskStackLock);
#endif	/*NOTDEF*/

	INITPAUSE(pauseFlag, 2);
	gMem->TaskStackLock = 1;

	/* All of the elements are unique. */
	for (i = 0; i < size; i++)	gMem->A[i] = i;
	
	/* Shuffle them randomly. */
#ifdef	SWAP
	for (i = 0; i < size; i++)	SWAP(gMem->A, i, (lrand48() % (size-i)) + i);
#else
	for (i = 0; i < size; i++)	Swap(i, (random() % (size-i)) + i);
#endif
	gMem->TaskStackTop = 0;
	gMem->NumWaiting = 0;
	
	/* Special: On this node, there are *2* threads (worker and master). */
	if (!quiet)
	   printf("\tSorting %d-entry array on %d procs! Bubble thresh: %d.\n",
		size, Tmk_nprocs, BubbleThresh);
	PushWork(0, size-1);	/* Push the initial value. */
    }
    else {
	freopen("/tmp/err.log", "w", stderr); setbuf(stderr, NULL);
    }
    Tmk_barrier(0);

    gettimeofday(&start, NULL);

    Worker();

    Tmk_barrier(0);
    gettimeofday(&finish, NULL);
    printf("Elapsed time: %.2f seconds\n",
	   (((finish.tv_sec * 1000000.0) + finish.tv_usec) -
	    ((start.tv_sec * 1000000.0) + start.tv_usec)) / 1000000.0);

    if (dump_array) DumpArray();
    if (verify) {
	for (i = 1; i < size; i++) {
	    if (gMem->A[i] < gMem->A[i-1]) {
		fprintf(stderr, "bad sort %d\n", i);
	    }
	}
	Tmk_barrier(0);
    }
    
    Tmk_exit(0);
}


/* Put an unsorted sub-array on the stack of things to be sorted. */
void
PushWork(i, j)
    int i, j;
{
    if (!Sequential) Tmk_lock_acquire(gMem->TaskStackLock);
    if (gMem->TaskStackTop == MAX_TASK_QUEUE - 1) {
	fprintf(stderr, "ERROR: Work stack overflow (%d entries)!\n",
		gMem->TaskStackTop);
	exit();
    }
    gMem->TaskStack[gMem->TaskStackTop].left = i;
    gMem->TaskStack[gMem->TaskStackTop].right = j;
    gMem->TaskStackTop++;
    if (debug) fprintf(stderr, "\t%d: PushingWork - pushed <%d,%d>.  Top now %d.\n",
			Tmk_proc_id, i,j, gMem->TaskStackTop);
    if (!Sequential) {
	if (gMem->TaskStackTop == 1)
	    SETPAUSE(pauseFlag);
	Tmk_lock_release(gMem->TaskStackLock);
    }
}

/* Return the array indices of a sub-array that needs sorting. */
PopWork(task)
    TaskElement *task;
{
    int i;


    if (!Sequential) {
	Tmk_lock_acquire(gMem->TaskStackLock);

	while (gMem->TaskStackTop == 0) {		/* Check for empty stack! */
	    if (++gMem->NumWaiting == Tmk_nprocs) {
		/* DONE. */
		SETPAUSE(pauseFlag);
		Tmk_lock_release(gMem->TaskStackLock);
		return(DONE);
	    }
	    else {
		/* Wait for some work to get pushed on... */
		if (gMem->NumWaiting == 1)
		    CLEARPAUSE(pauseFlag);
		Tmk_lock_release(gMem->TaskStackLock);
		if (debug)fprintf(stderr,"\t%d: PopWork waiting for work.\n",Tmk_proc_id);
		WAITPAUSE(pauseFlag);
		Tmk_lock_acquire(gMem->TaskStackLock);
		if (gMem->NumWaiting == Tmk_nprocs) {
		    Tmk_lock_release(gMem->TaskStackLock);
	            return(DONE);
		}
		--gMem->NumWaiting;
	    }
	} /* while task-stack empty */
    } /* non-sequential */
    else if (gMem->TaskStackTop == 0) return(DONE);

    gMem->TaskStackTop--;
    task->left  = gMem->TaskStack[gMem->TaskStackTop].left;
    task->right = gMem->TaskStack[gMem->TaskStackTop].right;

    if (debug) fprintf(stderr,"\t%d: PopWork - returning <%d,%d>.  Top now %d.\n",
			Tmk_proc_id, task->left, task->right,
		      gMem->TaskStackTop);

    if (!Sequential) Tmk_lock_release(gMem->TaskStackLock);

    return(0);
}


/* Partition the array between elements i and j around the specified pivot. */
Partition(i, j, pivot)
    int i, j, pivot;
{
    int left, right;

    left = i;
    right = j;
	
    do {
#ifdef	SWAP
	SWAP(gMem->A, left, right);
#else
	Swap(left,right);
#endif
	while (gMem->A[left]  <  pivot) left++;
	while ((right >= i) && (gMem->A[right] >= pivot)) right--;
    } while (left <= right);

    return(left);
}

void
LocalQuickSort(i, j)
    int i,j;
{
    int pivot, k;

    if (j <= i) return;
    
    if (debug) fprintf(stderr,"\t%d: Local QuickSorting %d-%d.\n", Tmk_proc_id, i, j);

    pivot = FindPivot(i,j);	
/*    pivot = (i + j) >> 1;*/

    k = Partition(i,j,gMem->A[pivot]);
    if (debug) fprintf(stderr,"\t%d: Splitting at %d-%d:%d-%d\n", Tmk_proc_id,i,k-1,k,j);

    if (k > i) {
	LocalQuickSort(i, k-1);
	LocalQuickSort(k,j);
    }
    else {
	LocalQuickSort( i + 1, j);
    }
}


/* Return the largest of first two keys. */
FindPivot(i, j)
    int i,j;
{
    if (gMem->A[i] > gMem->A[i+1]) return i;
    else return i+1;
}
			
#ifndef	SWAP
/* Swap two elements in the array being sorted. */
void
Swap(i, j)
    int i, j;
{
    register int tmp;

    tmp = gMem->A[i];
    gMem->A[i] = gMem->A[j];
    gMem->A[j] = tmp;
}
#endif

/* Resort to bubble sort when we get down to the nitty gritty. */
void
BubbleSort(i, j)
    int i,j;
{
    register int x, y, tmp;
	
    if (debug) fprintf(stderr,"\t%d: BubbleSorting %d-%d.\n", Tmk_proc_id, i, j);

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
	    if (P[y] < P[y-1]) SWAP(P, y, y-1);
#else
	    if (gMem->A[y] < gMem->A[y-1]) Swap(y, y-1);
#endif
#endif
	}
    }
}
	
void
QuickSort(i, j)
    int i,j;
{
    int pivot, k;

    if (debug) fprintf(stderr,"\t%d: QuickSorting %d-%d.\n", Tmk_proc_id, i, j);

    /* pivot is index of the pivot element */
    if (j-i+1 < BubbleThresh) {
	if (bubble) BubbleSort(i,j);
	else LocalQuickSort(i,j);
	return;
    } 
    pivot = FindPivot(i,j);

    k = Partition(i,j,gMem->A[pivot]);
    if (debug) fprintf(stderr,"\t%d: Splitting at %d-%d:%d-%d\n", Tmk_proc_id,i,k-1,k,j);
#define FAST_SEQUENTIAL
#ifdef FAST_SEQUENTIAL
    if (Sequential) {
	QuickSort(i, k-1);
	QuickSort(k,j);
	return;
    }
#endif /*FAST_SEQUENTIAL*/
    if (k-i > j-k) {
	/* The lower half [i through (k-1)] contains more entries.	*/
	/* Put the other half into the queue of unsorted subarrays	*/
	/* and recurse on the other half.				*/
	PushWork(k,j);
	QuickSort(i, k-1);
    }
    else {
	/* The lower half [i through (k-1)] contains more entries.	*/
	/* Put the other half into the queue of unsorted subarrays	*/
	/* and recurse on the other half.				*/
	PushWork(i,k-1);
	QuickSort(k,j);
    }
}

void
DumpArray()
{
    int i;

    for(i = 0; i < size; i++) {
	if ((i % 10) == 0 && (i != size-1)) printf("\n\t");
	printf("%5d ", gMem->A[i]);
    }
    printf("\n\n");
}

void
Worker()
/*    unsigned wnum; */
{
    int curr = 0, i, last;
    TaskElement task;


    /* Wait for signal to start. */
    if (!Sequential) Tmk_barrier(0);

/*    GetProcessorUsage(&start_stime, &start_utime, &start_retrans);
 */

    for (;;) {
	/* Continuously get a sub-array and sort it. */
	if (PopWork(&task) == DONE) break;
	QuickSort(task.left, task.right);
    }

    if (debug) fprintf(stderr,"\t%d: DONE.\n", Tmk_proc_id);

    if (!Sequential) Tmk_barrier(0);	/* Signal completion. */


}

