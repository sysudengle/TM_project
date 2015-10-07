/* Simple molecular dynamics simulation */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef RAND_MAX
#define RAND_MAX 0x7fff
#endif

#include "../../include/tm_threads.h"

#define ndim 3
#define nparts 2048
#define nsteps 10

#define R8F "% 30.16e" //

typedef double real8;
typedef real8 vnd_t[ndim] ;

int nd = ndim;
int np = nparts;
real8 mass = 1.0;
real8 rmass;
real8 dt = 1.0e-6;
vnd_t box;
vnd_t pos[nparts];
vnd_t vel[nparts];
vnd_t f[nparts];
vnd_t a[nparts];
real8 pot, kin;
int i_step;

double _t1, _t2, _t3, _t4;

double t() {
    struct timeval tv;
    gettimeofday(&tv, ((struct timezone *)0));
    return (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

pthread_mutex_t pot_mx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kin_mx = PTHREAD_MUTEX_INITIALIZER;

/* statement function for the pair potential and its derivative
   This potential is a harmonic well which smoothly saturates to a
   maximum value at PI/2.  */

real8 vzz(real8 x) {
  if (x < M_PI_2) 
    return pow(sin(x), 2.0);
  else
    return 1.0;
}

real8 dv(real8 x) {
  if (x < M_PI_2) 
    return 2.0 * sin(x) * cos(x);
  else
    return 0.0;
}


/***********************************************************************
 * Initialize the positions, velocities, and accelerations.
 ***********************************************************************/
void initialize()
{
  int i, j;
  real8 x;
  
  srand(4711L);
  for (i = 0; i < np; i++) {
    for (j = 0; j < nd; j++) {
      x = rand() % 10000/(real8)10000.0;
      pos[i][j] = box[j]*x;
      vel[i][j] = 0.0;
      a[i][j] = 0.0;
    }
  }
}

/* Compute the displacement vector (and its norm) between two particles. */
real8 dist(int nd, vnd_t r1, vnd_t r2, vnd_t dr)
{
  int i;
  real8 d;
  
  d = 0.0;
  for (i = 0; i < nd; i++) {
    dr[i] = r1[i] - r2[i];
    d += dr[i] * dr[i];
  }
  return sqrt(d);
}

/* Return the dot product between two vectors of type real*8 and length n */
real8 dotr8(int n, vnd_t x,vnd_t y)
{
  int i;
  real8 t = 0.0;
  
  for (i = 0; i < n; i++) {
    t += x[i]*y[i];
  }
  
  return t;
}

/***********************************************************************
 * Compute the forces and energies, given positions, masses,
 * and velocities
 ***********************************************************************/
/*
void compute() 
{
    real8 x;
    int i, j, k;
    vnd_t rij;
    real8  d;
    
    pot = 0.0;
    kin = 0.0;

    // The computation of forces and energies is fully parallel. 
    #pragma omp parallel for default(shared) private(i,j,k,rij,d) reduction(+ : pot, kin)
    for (i = 0; i < np; i++)
    {
        // compute potential energy and forces 
        for (j = 0; j < nd; j++)
            f[i][j] = 0.0;

        for (j = 0; j < np; j++) 
        {
            if (i != j) 
            {
                d = dist(nd,pos[i],pos[j],rij);
                // attribute half of the potential energy to particle 'j' 
                
                pot = pot + 0.5 * vzz(d);
                for (k = 0; k < nd; k++) 
                {
                    f[i][k] = f[i][k] - rij[k]* dv(d) /d;
                }
            }
        }
        // compute kinetic energy 
        kin = kin + dotr8(nd,vel[i],vel[j]);
    }

    kin = kin*0.5*mass;
}
*/

int run_compute( void* arg, int id )
{
    int i, j, k;
    vnd_t rij;
    real8  d, _pot = 0.0, _kin = 0.0;
    
    _t1 = t();
       
    for (i = id; i < np; i += NTHREADS )
    {
        /* compute potential energy and forces */
        for (j = 0; j < nd; j++)
            f[i][j] = 0.0;
                
	
	
        for (j = 0; j < np; j++)
        {
            if (i != j)
            {
                d = dist( nd, pos[i], pos[j], rij);
                
                /* attribute half of the potential energy to particle 'j' */
                
		pthread_mutex_lock( &pot_mx );
                _pot = _pot + 0.5 * vzz(d);
		pthread_mutex_unlock( &pot_mx );
                
                
                for (k = 0; k < nd; k++)
                    f[i][k] = f[i][k] - rij[k]* dv(d) /d;                
            }
        }
        /* compute kinetic energy */
	
	pthread_mutex_lock( &kin_mx );
        _kin = _kin + dotr8( nd, vel[i], vel[j]);
	pthread_mutex_unlock( &kin_mx );
    }
    
   _t2 = t(); 
    
    pthread_mutex_lock( &pot_mx );

    kin += _kin;
    pot += _pot;

    pthread_mutex_unlock( &pot_mx );
   //printf( "time: %f  %f   %f \n", _t2-_t1, _t3-_t2, _t3-_t1 );
    
    return 0;
}

void compute() 
{
    pot = 0.0;
    kin = 0.0;
    
    /* The computation of forces and energies is fully parallel. */
    //#pragma omp parallel for default(shared) private(i,j,k,rij,d) reduction(+ : pot, kin)
    
    
    PARALLEL_EXECUTE( NTHREADS, run_compute, NULL );
    //run_compute( NULL, 0 );
    
    //for (int i = 0; i < np; i += 1 )
    //    printf("%d:  f[%d]  "R8F" "R8F" "R8F"\n", i_step, i, f[i][0], f[i][1], f[i][2]);
    
    kin = kin*0.5*mass;
}
      
/***********************************************************************
 * Perform the time integration, using a velocity Verlet algorithm
 ***********************************************************************/
/*
void update()
{
    int i, j;
    real8 rmass;

    rmass = 1.0/mass;
    
    // The time integration is fully parallel 
    #pragma omp parallel for default(shared) private(i,j) firstprivate(rmass, dt)
    for (i = 0; i < np; i++) 
    {
        for (j = 0; j < nd; j++) 
        {
            pos[i][j] = pos[i][j] + vel[i][j]*dt + 0.5*dt*dt*a[i][j];
            vel[i][j] = vel[i][j] + 0.5*dt*(f[i][j]*rmass + a[i][j]);
            a[i][j] = f[i][j]*rmass;
        }

    }
}
*/
int run_update( void* arg, int id )
{
    int i, j;
    
    _t1 = t();
    
    for (i = id; i < np; i += NTHREADS )
    {
        for (j = 0; j < nd; j++)
        {
            pos[i][j] = pos[i][j] + vel[i][j]*dt + 0.5*dt*dt*a[i][j];
            vel[i][j] = vel[i][j] + 0.5*dt*(f[i][j]*rmass + a[i][j]);
            a[i][j] = f[i][j]*rmass;
        }
        
    }
    
    _t2 = t(); 
    
   //printf( "time: %f \n", _t2-_t1 );
    
    return 0;
}


void update()
{
    /* The time integration is fully parallel */
    //#pragma omp parallel for default(shared) private(i,j) firstprivate(rmass, dt)
    
    PARALLEL_EXECUTE( NTHREADS, run_update, NULL );
    //run_update( NULL, 0 );

}
/******************
 * main program 
 ******************/



int main (int argc, char **argv) {
  
  /* simulation parameters */
  
  int i;
  double t0,t1;
  real8 E0;
  i_step = -1;
  
  for (i = 0; i < nd; i++)
    box[i] = 10.0;
  rmass = 1.0/mass;
    
  /* set initial positions, velocities, and accelerations */
  initialize();

  t0 = t();
  
  CREATE_TM_THREADS( NTHREADS );

  /* compute the forces and energies */
  compute();
  E0 = pot + kin;
    
  /* This is the main time stepping loop */
  for (i_step = 0; i_step < nsteps; i_step++) 
  {
    compute();
#if 0
    printf("%d:  "R8F " " R8F " \n", i_step, pot, kin/*, (pot + kin - E0)/E0*/);    
#endif

    update();
    /*
    for( i = 0; i < np; i++ )
    {
        printf("%d:  pos[%d]: "R8F " " R8F " " R8F "\n", i_step, i, pos[i][0], pos[i][1], pos[i][2] );
        printf("%d:  vel[%d]: "R8F " " R8F " " R8F "\n", i_step, i, vel[i][0], vel[i][1], vel[i][2] );
        printf("%d:    a[%d]: "R8F " " R8F " " R8F "\n", i_step, i, a[i][0], a[i][1], a[i][2] );
    }
    */
  }

  DESTROY_TM_THREADS( NTHREADS );
  
  t1 = t();

  printf("Execution time\t %f s\n", t1-t0);

  exit (0);
}

