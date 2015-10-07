/* Moldyn - Molecular Dynamics Simulation */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(PTHREAD)
# include <sched.h>
#endif

#ifdef LIBTM
# include "../../src/tm.h"
#elif PTHREAD
# include "../../src/infra/barrier.h"
# include "../../src/infra/bitset.h"
#endif

#define LOCAL       /* Such a function that changes no global vars */
#define INPARAMS    /* All parameters following this are 'in' params  */

#define SQRT(a)  sqrt(a)
#define POW(a,b) pow(a,b)
#define SQR(a)   ((a)*(a))
#define DRAND(a)  drand_x(a)

int NTHREADS;

#define NDIM 3

extern long random();
extern int srandom();

/*
!======================  DATA-SETS  ======================================
*/
#define MAXNEIGHBOURS    500

# ifdef  SMALL
#      define BOXSIZE                 4    /* creates 256 molecules */
#      define NUMBER_TIMESTEPS       30
#      define MAXINTERACT         32000    /* size of interaction array */
# elif defined(MEDIUM)
#      define BOXSIZE                 8
#      define NUMBER_TIMESTEPS       30
#      define MAXINTERACT        320000
# elif defined(SUPER)
#      define BOXSIZE                20
#      define NUMBER_TIMESTEPS       30
#      define MAXINTERACT     5120000
# else
#      define BOXSIZE                13
#      define NUMBER_TIMESTEPS       30
#      define MAXINTERACT       1600000
# endif

#define NUM_PARTICLES      (4*BOXSIZE*BOXSIZE*BOXSIZE)
#define DENSITY            0.83134
#define TEMPERATURE        0.722
#define CUTOFF             3.5000
#define DEFAULT_TIMESTEP   0.064
#define SCALE_TIMESTEP     4
#define TOLERANCE          1.2

#define DIMSIZE NUM_PARTICLES
#define DSIZE   2
#define INDX(aa,bb)  (((aa)*DSIZE) + (bb))    /* used to index inter  */
#define IND(aa,bb)   ((aa)*DIMSIZE + (bb))    /* used to index x,f,vh */
#define MIN(a,b)     (((a)<(b))?(a):(b))

/*
!======================  GLOBAL ARRAYS ======================================
!
! Note : inter is usually the biggest array. If BOXSIZE = 13, it will
!        cause 1 million interactions to be generated. This will need
!        a minimum of 80 MBytes to hold 'inter'. The other
!        arrays will need about a sum of around 800 KBytes. Note
!        that MAXINTERACT may be defined to a more safe value causing
!        extra memory to be allocated. (~ 130 MBytes !)
!============================================================================
*/

#ifdef LIBTM
# define MOLDYN_DOUBLE tm_double
# define MOLDYN_INT    tm_int
#else
# define MOLDYN_DOUBLE double
# define MOLDYN_INT    int
#endif


/* main data structures for tracking position, forces and velocities */
MOLDYN_DOUBLE x[NUM_PARTICLES * NDIM];
MOLDYN_DOUBLE f[NUM_PARTICLES * NDIM];
MOLDYN_DOUBLE vh[NUM_PARTICLES * NDIM];

/* Replaced old data structure to track interactions */
/* int inter[MAXINTERACT * DSIZE]; */
MOLDYN_INT inter[NUM_PARTICLES][MAXNEIGHBOURS];

#ifdef MEASURE
int connect[NUM_PARTICLES];
#endif

/*
!======================  GLOBAL VARIABLES ===================================
*/

double			side;                  /*  length of side of box                 */
double			sideHalf;              /*  1/2 of side                           */
double			cutoffRadius;          /*  cuttoff distance for interactions     */
int				neighUpdate;           /*  timesteps between interaction updates */
double			perturb;               /*  perturbs initial coordinates          */

double			timeStep;              /*  length of each timestep   */
double			timeStepSq;            /*  square of timestep        */
double			timeStepSqHalf;        /*  1/2 of square of timestep */

int				numMoles;              /*  number of molecules                   */
MOLDYN_INT      ninter;                /*  number of interacting molecules pairs  */
double			vaver;                 /*                                        */

MOLDYN_DOUBLE   epot;                  /*  The potential energy      */
MOLDYN_DOUBLE   vir;                   /*  The virial  energy        */
MOLDYN_DOUBLE   count, vel ;
MOLDYN_DOUBLE   ekin;

int				n3;

#ifdef PTHREAD
barrier_t barrier;
#endif

//number of neighbours for each molecule
MOLDYN_INT num_interactions [NUM_PARTICLES];

/* ----------- UTILITY ROUTINES --------------------------- */

/*
!=============================================================
!  Function : drand_x()
!  Purpose  :
!    Used to calculate the distance between two molecules
!    given their coordinates.
!=============================================================
*/
LOCAL double drand_x(double x)
{
  double tmp = ( (double) random() ) * 4.6566128752458e-10;
#ifdef PRINT_RANDS
  printf("%lf\n", tmp);
#endif
  return tmp;
}

/*
!=============================================================
!  Function : Foo()
!  Purpose  :
!    Used to calculate the distance between two molecules
!    given their coordinates.
!=============================================================
*/
LOCAL double Foo(double xi, double yi, double zi, double xj, double yj, double zj)
{
  double xx, yy, zz, rd;

  xx = xi - xj;
  yy = yi - yj;
  zz = zi - zj;
  if ( xx < -sideHalf ) xx += side ;
  if ( yy < -sideHalf ) yy += side ;
  if ( zz < -sideHalf ) zz += side ;
  if ( xx >  sideHalf ) xx -= side ;
  if ( yy >  sideHalf ) yy -= side ;
  if ( zz >  sideHalf ) zz -= side ;
  rd = xx*xx + yy*yy + zz*zz ;
  return (rd);
}


/*---------- INITIALIZATION ROUTINES HERE               ------------------*/


/*
!============================================================================
!  Function :  InitSettings()
!  Purpose  :
!     This routine sets up the global variables
!============================================================================
*/

int InitSettings()
{
   numMoles  = 4*BOXSIZE*BOXSIZE*BOXSIZE;

   side   = POW( ((double)(numMoles)/DENSITY), 0.3333333);
   sideHalf  = side * 0.5 ;

   cutoffRadius  = MIN(CUTOFF, sideHalf );

   timeStep      = DEFAULT_TIMESTEP/SCALE_TIMESTEP ;
   timeStepSq    = timeStep   * timeStep ;
   timeStepSqHalf= timeStepSq * 0.5 ;

   neighUpdate   = 10*(1+SCALE_TIMESTEP/4);
   perturb       = side/ (double)BOXSIZE;     /* used in InitCoordinates */
   vaver         = 1.13 * SQRT(TEMPERATURE/24.0);

   n3 = numMoles * 3;

#ifdef PRINT_RESULTS
# if (!defined(PRINT_COORDINATES) && !defined(PRINT_INTERACTION_LIST))
   fprintf(stdout,"----------------------------------------------------");
   fprintf(stdout,"\n MolDyn - A Simple Molecular Dynamics simulation \n");
   fprintf(stdout,"----------------------------------------------------");
   fprintf(stdout,"\n number of particles is ......... %6d", numMoles);
   fprintf(stdout,"\n side length of the box is ...... %13.6le",side);
   fprintf(stdout,"\n cut off radius is .............. %13.6le",cutoffRadius);
   fprintf(stdout,"\n temperature is ................. %13.6le",TEMPERATURE);
   fprintf(stdout,"\n time step is ................... %13.6le",timeStep);
   fprintf(stdout,"\n interaction-list updated every..   %d steps",neighUpdate);
   fprintf(stdout,"\n total no. of steps .............   %d ",NUMBER_TIMESTEPS);
   fprintf(stdout,
     "\n TimeStep   K.E.        P.E.        Energy    Temp.     Pres.    Vel.    rp ");
   fprintf(stdout,
     "\n -------- --------   ----------   ----------  -------  -------  ------  ------");
# endif
#endif
}


/*
!============================================================================
!  Function : InitCoordinates()
!  Purpose  :
!     Initialises the coordinates of the molecules by
!     distribuuting them uniformly over the entire box
!     with slight perturbations.
!============================================================================
*/

void InitCoordinates(int numMoles, int siz, double perturb)
{
 int n, k, ij,  j, i, npoints;
 double tmp = 0;

   npoints = siz * siz * siz ;
   for ( n =0; n< npoints; n++) {
      k   = n % siz ;
      j   = (int)((n-k)/siz) % siz;
      i   = (int)((n - k - j*siz)/(siz*siz)) % siz ;

      x[IND(0,n)] = i*perturb ;
      x[IND(1,n)] = j*perturb ;
      x[IND(2,n)] = k*perturb ;

      x[IND(0,n+npoints)] = i*perturb + perturb * 0.5 ;
      x[IND(1,n+npoints)] = j*perturb + perturb * 0.5;
      x[IND(2,n+npoints)] = k*perturb ;

      x[IND(0,n+npoints*2)] = i*perturb + perturb * 0.5 ;
      x[IND(1,n+npoints*2)] = j*perturb ;
      x[IND(2,n+npoints*2)] = k*perturb + perturb * 0.5;

      x[IND(0,n+npoints*3)] = i*perturb ;
      x[IND(1,n+npoints*3)] = j*perturb + perturb * 0.5 ;
      x[IND(2,n+npoints*3)] = k*perturb + perturb * 0.5;
   }
}

/*
!============================================================================
! Function  :  InitVelocities()
! Purpose   :
!    This routine initializes the velocities of the
!    molecules according to a maxwellian distribution.
!============================================================================
*/

int  InitVelocities(double h)
{
   int i, j, k, nmoles1, nmoles2, iseed;
   double ts, sp, sc, r, s;
   double u1, u2, v1, v2, ujunk,tscale;
   double DRAND(double);

   iseed = 4711;
   ujunk = DRAND(iseed);
   iseed = 0;
   tscale = (16.0)/(1.0*numMoles - 1.0);

   for ( i =0; i< n3; i=i+2) {
     do {
       u1 = DRAND(iseed);
       u2 = DRAND(iseed);
       v1 = 2.0 * u1   - 1.0;
       v2 = 2.0 * u2   - 1.0;
       s  = v1*v1  + v2*v2 ;
     } while( s >= 1.0 );

     r = SQRT( -2.0*log(s)/s );
     vh[i]    = v1 * r;
     vh[i+1]  = v2 * r;
   }



   /* There are three parts - repeat for each part */
   nmoles1 = n3/3 ;
   nmoles2 = nmoles1 * 2;

   /*  Find the average speed  for the 1st part */
   sp   = 0.0 ;
   for ( i=0; i<nmoles1; i++) {
     sp = sp + vh[i];
   }
   sp   = sp/nmoles1;


   /*  Subtract average from all velocities of 1st part*/
   for ( i=0; i<nmoles1; i++) {
     vh[i] = vh[i] - sp;
   }

   /*  Find the average speed for 2nd part*/
   sp   = 0.0 ;
   for ( i=nmoles1; i<nmoles2; i++) {
     sp = sp + vh[i];
   }
   sp   = sp/(nmoles2-nmoles1);

   /*  Subtract average from all velocities of 2nd part */
   for ( i=nmoles1; i<nmoles2; i++) {
     vh[i] = vh[i] - sp;
   }

   /*  Find the average speed for 2nd part*/
   sp   = 0.0 ;
   for ( i=nmoles2; i<n3; i++) {
     sp = sp + vh[i];
   }
   sp   = sp/(n3-nmoles2);

   /*  Subtract average from all velocities of 2nd part */
   for ( i=nmoles2; i<n3; i++) {
     vh[i] = vh[i] - sp;
   }

   /*  Determine total kinetic energy  */
   ekin = 0.0 ;
   for ( i=0 ; i< n3; i++ ) {
     ekin  = ekin  + vh[i]*vh[i] ;
   }
   ts = tscale * ekin ;
   sc = h * SQRT(TEMPERATURE/ts);
   for ( i=0; i< n3; i++) {
     vh[i] = vh[i] * sc ;
   }
}

/*
!============================================================================
!  Function :  InitForces()
!  Purpose :
!    Initialize all the partial forces to 0.0
!============================================================================
*/

int  InitForces()
{
int i;

   for ( i=0; i<n3; i++ ) {
     f[i] = 0.0 ;
   }
}

int FirstCoordinates()
{
	int i;
	for ( i = 0; i < n3; i ++) {
		x[i] = x[i] + vh[i];
		if ( x[i] < 0.0 )    x[i] = x[i] + side ;
		if ( x[i] > side   ) x[i] = x[i] - side ;
	}
}

/*---------- UPDATE ROUTINES HERE               ------------------*/

void PrintCoordinates(INPARAMS int numMoles)
{
  int i, j;
  printf("%d\n", numMoles);
  for (i=0;i<numMoles;i++)
   {
     printf("%f,%f,%f\n", (double)x[IND(0,i)], (double)x[IND(1,i)],(double)x[IND(2,i)]);
   }
}

/*
!============================================================================
!  Function :  BuildNeigh()
!  Purpose  :
!     This routine is called after every x timesteps
!     to  rebuild the list of interacting molecules
!     Note that molecules within cutoffRad+TOLERANCE
!     are included. This tolerance is in order to allow
!     for molecules that might move within range
!     during the computation.
!============================================================================
*/

//we should use dynamic scheduling here
int BuildNeigh( void* arg, int id )
{
  double rd, cutoffSquare;
  int    i,j,k;

  cutoffSquare  = (cutoffRadius * TOLERANCE)*(cutoffRadius * TOLERANCE);

  /* for ( i=0; i<numMoles; i++) */ //single-threaded version
  for ( i = id; i < numMoles; i += NTHREADS)
  {
    j = 0;

#ifdef LIBTM
    num_interactions[i] = 0;
#endif

    for ( k = 0; k < numMoles; k++ )
    {
      if (i == k) continue;

#ifdef LIBTM
    BEGIN_TRANSACTION();
#endif
      rd = Foo ( (double)x[IND(0,i)], (double)x[IND(1,i)], (double)x[IND(2,i)],
                 (double)x[IND(0,k)], (double)x[IND(1,k)], (double)x[IND(2,k)]);

      if ( rd <= cutoffSquare)
      {
        inter[i][j++] = k;
        num_interactions[i]++;
        if ( j == MAXNEIGHBOURS - 1) perror("Too many interactions for molecule");
      }

#ifdef LIBTM
    COMMIT_TRANSACTION();
#endif
    }

  }
  return 0;
}

void PrintInteractionList(INPARAMS int ninter)
{
  int i;
  printf("%d\n", ninter);
  for (i=0;i<ninter;i++)
   {
     printf("%d %d\n", inter[INDX(i,0)], inter[INDX(i,1)]);
   }
}

#ifdef MEASURE
void PrintConnectivity()
{
  int ii, i;
  int min, max;
  float sum, sumsq, stdev, avg;

  bzero((char *)connect, sizeof(int) * NUM_PARTICLES);

  for (ii=0;ii<ninter;ii++)
    {
      assert(inter[INDX(ii,0)] < NUM_PARTICLES);
      assert(inter[INDX(ii,1)] < NUM_PARTICLES);

      connect[inter[INDX(ii,0)]]++;
      connect[inter[INDX(ii,1)]]++;
    }

  sum = 0.0;
  sumsq = 0.0;

  sum = connect[0];
  sumsq = SQR(connect[0]);
  min = connect[0];
  max = connect[0];
  for (i=1;i<NUM_PARTICLES;i++)
    {
      sum += connect[i];
      sumsq += SQR(connect[i]);
      if (min > connect[i])
  min = connect[i];
      if (max < connect[i])
  max = connect[i];
    }

  avg = sum / NUM_PARTICLES;
  stdev = sqrt((sumsq / NUM_PARTICLES) - SQR(avg));

  printf("avg = %4.1lf, dev = %4.1lf, min = %d, max = %d\n",
   avg, stdev, min, max);

}
#endif

/*
!============================================================================
! Function :  ComputeForces
! Purpose  :
!   This is the most compute-intensive portion.
!   The routine iterates over all interacting  pairs
!   of molecules and checks if they are still within
!   inteacting range. If they are, the force on
!   each  molecule due to the other is calculated.
!   The net potential energy and the net virial
!   energy is also computed.
!============================================================================
*/

int ComputeForces( void* arg, int id )
{
  double cutoffSquare;
  double xx, yy, zz, rd, rrd, rrd2, rrd3, rrd4, rrd5, rrd6, rrd7, r148;
  double forcex, forcey, forcez;
  int    i,j,k;

  double vir_tmp = 0;
  double epot_tmp = 0;


  cutoffSquare = cutoffRadius*cutoffRadius ;

  for(i=id; i<numMoles; i+=NTHREADS)
  {



    forcex = 0;
    forcey = 0;
    forcez = 0;

    for (j = 0; j < num_interactions[i]; ++j)
    {
#ifdef LIBTM
    BEGIN_TRANSACTION();
#endif
      k = inter[i][j];

      xx = x[IND(0,i)] - x[IND(0,k)];
      yy = x[IND(1,i)] - x[IND(1,k)];
      zz = x[IND(2,i)] - x[IND(2,k)];

      if (xx < -sideHalf) xx += side;
      if (yy < -sideHalf) yy += side;
      if (zz < -sideHalf) zz += side;
      if (xx > sideHalf) xx -= side;
      if (yy > sideHalf) yy -= side;
      if (zz > sideHalf) zz -= side;

      rd = (xx*xx + yy*yy + zz*zz);

      if ( rd < cutoffSquare ) {
        rrd   = 1.0/rd;
        rrd2  = rrd*rrd ;
        rrd3  = rrd2*rrd ;
        rrd4  = rrd2*rrd2 ;
        rrd6  = rrd2*rrd4;
        rrd7  = rrd6*rrd ;
        r148  = rrd7 - 0.5 * rrd4 ;

        forcex += xx*r148;
        forcey += yy*r148;
        forcez += zz*r148;

        if (i < k)
        {
          vir_tmp += rd*r148 ;
          epot_tmp += (rrd6 - rrd3);
        }
      }
#ifdef LIBTM
    COMMIT_TRANSACTION();
#endif
    }
    /* pthread_mutex_lock (&mutex_f); */
    f[IND(0,i)]  = forcex ;
    f[IND(1,i)]  = forcey ;
    f[IND(2,i)]  = forcez ;
    /* pthread_mutex_unlock (&mutex_f); */

  }

#if defined(PTHREAD)
  pthread_mutex_lock (&global_mutex1);
#elif LIBTM
  BEGIN_TRANSACTION();
#endif

  vir -= vir_tmp;
  epot += epot_tmp;

#if defined(PTHREAD)
  pthread_mutex_unlock (&global_mutex1);
#elif LIBTM
  COMMIT_TRANSACTION();
#endif

  return 0;
}

/*
!============================================================================
!  Function : Update
!  Purpose  :
!       Updates the everything
!============================================================================
*/

//we should use dynamic scheduling here
int Update( void* arg, int id )
{
  int i,j;

  double vaverh, sq;

  double sum = 0;
  double velocity = 0;
  double counter = 0;

  double force_x, force_y, force_z;
  double velocity_x, velocity_y, velocity_z;
  double velocity_x_sq, velocity_y_sq, velocity_z_sq;

  vaverh = vaver * timeStep ;

  /* for ( i = 0; i < numMoles; i ++) */
  for ( i = id; i < numMoles; i += NTHREADS)
  {
#ifdef LIBTM
	BEGIN_TRANSACTION();
#endif

    /* Compute Velocity */
    force_x  = f[IND(0,i)] * timeStepSqHalf ;
    force_y  = f[IND(1,i)] * timeStepSqHalf ;
    force_z  = f[IND(2,i)] * timeStepSqHalf ;

    velocity_x  = vh[ IND(0,i) ] + force_x ;
    velocity_y  = vh[ IND(1,i) ] + force_y ;
    velocity_z  = vh[ IND(2,i) ] + force_z ;

    velocity_x_sq = SQR(velocity_x);
    velocity_y_sq = SQR(velocity_y);
    velocity_z_sq = SQR(velocity_z);

    /* Compute KEVel */
    sum += velocity_x_sq;
    sum += velocity_y_sq;
    sum += velocity_z_sq;

    sq = SQRT(  velocity_x_sq + velocity_y_sq + velocity_z_sq );

    if ( sq > vaverh ) counter += 1.0 ;

    velocity += sq ;

    /* Update Coordinates */
	/*
	_tmp = num_interactions [i];
    while (counter_f[i] != _tmp);
	*/

    /* must be atomic
     * should this be at the very end of the transaction?
     * don't think so, as long as it's _before_ incrementing counter_x
	 */
    /* set_mb (&counter_f[i], 0); */

    /* velocity_* is already vh[ * ] + forcex ; */
    vh[IND(0,i)] = velocity_x + force_x;
    vh[IND(1,i)] = velocity_y + force_y;
    vh[IND(2,i)] = velocity_z + force_z;

    /* protect x from readers in ComputeForces */
    /* pthread_mutex_lock (&mutex_x); */

    x[IND(0,i)] += velocity_x + force_x;
    x[IND(1,i)] += velocity_y + force_y;
    x[IND(2,i)] += velocity_z + force_z;

    for (j = 0; j < 3; ++j) {
       if ( x[IND(j, i)] < 0.0 )    x[IND(j, i)] += side;
       if ( x[IND(j, i)] > side )	x[IND(j, i)] -= side;
    }

#ifdef LIBTM
    COMMIT_TRANSACTION();
#endif
  }

#if defined(PTHREAD)
  pthread_mutex_lock (&global_mutex2);
#elif LIBTM
  BEGIN_TRANSACTION();
#endif

  ekin += sum/timeStepSq;
  vel += velocity/timeStep;
  count += counter;

#if defined(PTHREAD)
  pthread_mutex_unlock (&global_mutex2);
#elif LIBTM
  COMMIT_TRANSACTION();
#endif

  return 0;
}


/*
!=============================================================
!  Function : PrintResults()
!  Purpose  :
!    Prints out the KE, the PE and other results
!=============================================================
*/

LOCAL int PrintResults(int move, double ekin, double epot,  double vir, double vel, double count, int numMoles, int ninteracts)
{
   double ek, etot, temp, pres, rp, tscale ;

   ek   = 24.0 * ekin ;
   epot = 4.00 * epot ;
   etot = ek + epot ;
   tscale = (16.0)/((double)numMoles - 1.0);
   temp = tscale * ekin ;
   pres = DENSITY * 16.0 * (ekin-vir)/numMoles ;
   vel  = vel/numMoles;

   rp   = (count/(double)(numMoles)) * 100.0 ;

   fprintf(stdout,
     "\n %4d %12.4lf %12.4lf %12.4lf %8.4lf %8.4lf %8.4lf %5.1lf",
        move, ek,    epot,   etot,   temp,   pres,   vel,     rp);
#ifdef DEBUG
   fprintf(stdout,"\n\n In the final step there were %d interacting pairs\n", ninteracts);
#endif
}

void dump_values(char *s)
{
  int i;
  printf("\n%s\n", s);
  for (i=0;i<n3/3;i++)
    {
      printf("%d: coord = (%lf, %lf, %lf), vel = (%lf, %lf, %lf), force = (%lf, %lf, %lf)\n",
	     i, (double)x[IND(0,i)], (double)x[IND(1,i)], (double)x[IND(2,i)],
	     (double)vh[IND(0,i)], (double)vh[IND(1,i)], (double)vh[IND(2,i)],
	     (double)f[IND(0,i)], (double)f[IND(1,i)], (double)f[IND(2,i)]);
    }
}


#ifdef coredump
#define WITH_SMT
#endif

/* main work function for each thread */
#if defined(PTHREAD)
void * do_thread_work (void * _id) {


  int id = (int) _id;

  int tstep, i;

  cpu_set_t mask;
  CPU_ZERO( &mask );
# ifdef WITH_SMT
	CPU_SET( id*2, &mask );
# else
	CPU_SET( id, &mask );
# endif
  sched_setaffinity(0, sizeof(mask), &mask);

  for ( tstep=0; tstep< NUMBER_TIMESTEPS; tstep++)
  {
# ifdef PTHREAD
    barrier_wait (&barrier);

    vir  = 0.0;
    epot = 0.0;
    ekin = 0.0;
    vel = 0.0;
    count = 0.0;

    barrier_wait (&barrier);
# endif

    if ( tstep % neighUpdate == 0)
	{
      barrier_wait (&barrier);

      if (id == 0) {

# ifdef PRINT_COORDINATES
		PrintCoordinates(numMoles);
# endif
		ninter = 0;
      }
/*
      for (i = id; i < numMoles; i += NTHREADS) {
         num_interactions [i] = 0;
      }
*/
      barrier_wait (&barrier);

      BuildNeigh (NULL, id);

      barrier_wait (&barrier);

      if (id == 0) {
# ifdef PRINT_INTERACTION_LIST
		PrintInteractionList(INPARAMS ninter);
# endif

# ifdef MEASURE
		PrintConnectivity();
# endif
      }
      barrier_wait (&barrier);
    }

    ComputeForces (NULL, id);

# ifdef PTHREAD
    barrier_wait (&barrier);
# endif

    Update(NULL, id);

# ifdef PTHREAD
    barrier_wait (&barrier);
# endif

# if defined(PTHREAD) && defined(PRINT_RESULTS)
    if (id == 0) {
      PrintResults (INPARAMS tstep, (double)ekin, (double)epot, (double)vir,(double)vel,(double)count,numMoles,(int)ninter);
	}
# endif

  }

  return NULL;
}
#endif

#ifdef LIBTM
void do_libtm_work(void) {

  int tstep, i;

  for ( tstep=0; tstep< NUMBER_TIMESTEPS; tstep++)
  {
    vir  = 0.0;
    epot = 0.0;
    ekin = 0.0;
    vel = 0.0;
    count = 0.0;

    if ( tstep % neighUpdate == 0)
	{
# ifdef PRINT_COORDINATES
      PrintCoordinates(numMoles);
# endif

      ninter = 0;

      PARALLEL_EXECUTE( NTHREADS, BuildNeigh, NULL );

# ifdef PRINT_INTERACTION_LIST
      PrintInteractionList(INPARAMS ninter);
# endif

# ifdef MEASURE
      PrintConnectivity();
# endif
    }

	PARALLEL_EXECUTE( NTHREADS, ComputeForces, NULL );
	PARALLEL_EXECUTE( NTHREADS, Update, NULL );

	PrintResults (INPARAMS tstep, (double)ekin, (double)epot, (double)vir,(double)vel,(double)count,numMoles,(int)ninter);
  }
  return;
}
#endif


/*
!============================================================================
!  Function : main()
!  Purpose  :
!      All the main computational structure  is here
!      Iterates for specified number of  timesteps.
!      In each time step,
!        UpdateCoordinates() changes molecules coordinates based
!              on the velocity of that molecules
!              and that molecules
!        BuildNeigh() rebuilds the interaction-list on
!              certain time-steps
!        ComputeForces() - the time-consuming step, iterates
!              over all interacting pairs and computes forces
!        UpdateVelocities() - updates the velocities of
!              all molecules  based on the forces.
!============================================================================
*/

int main( int argc, char *argv[])
{

#if defined(PTHREAD) || defined(LIBTM)
  if (argc < 2)
  {
    printf("Usage: ./moldyn NUM_THREADS\n");
    return 0;
  }
  NTHREADS = atoi(argv[1]);
#endif

  int i, j;

  InitSettings   ();
  InitCoordinates(INPARAMS numMoles, BOXSIZE, perturb);
  InitVelocities (INPARAMS timeStep);
  InitForces     ();

  /* funciton to compensate for moving updatecoordinates to the end */
  FirstCoordinates();

#ifdef PTHREAD
  /* barrier only needed in the non predicated-commit pthread version */
  barrier_init(&barrier, NTHREADS);
#endif

#if defined(PTHREAD)
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_t* threads = new pthread_t[ NTHREADS-1 ];
#endif

  vir  = 0.0;
  epot = 0.0;
  ekin = 0.0;
  vel = 0.0;
  count = 0.0;

#if defined(PTHREAD)
  for( i = 1; i < NTHREADS; i++ )
    pthread_create( &threads[i-1], &attr, do_thread_work, (void*)i );

  do_thread_work ((void *)0);

  for( i = 1; i < NTHREADS; i++ )
    pthread_join( threads[i-1], NULL );
#else
  CREATE_TM_THREADS( NTHREADS );

  do_libtm_work();

  DESTROY_TM_THREADS( NTHREADS );
#endif


  printf("\n");

  return 0;
}
