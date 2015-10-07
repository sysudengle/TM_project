/* Simple molecular dynamics simulation */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <iostream>

#include "../../include/tm_basics.h"
#include "../../include/tm_threads.h"

#define NTHREADS 1

#ifndef RAND_MAX
#define RAND_MAX 0x7fff
#endif

#define ndim 3
#define nparts 2048
#define nsteps 10
#define R8F "% 30.20e" //

typedef double real8;
typedef real8 vnd_t[ndim];

typedef tm_type<double,1> tm_real8;
//typedef real8 tm_real8;

typedef tm_real8 tm_vnd_t[ndim];


double t() {
    struct timeval tv;
    gettimeofday(&tv, ((struct timezone *)0));
    return (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

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
void initialize(int np, int nd,
		vnd_t box, tm_vnd_t *pos, tm_vnd_t *vel, tm_vnd_t *acc)
{
  int i, j;
  real8 x;
  
  srand(4711L);
  for (i = 0; i < np; i++) {
    for (j = 0; j < nd; j++) {
      x = rand() % 10000/(real8)10000.0;
      pos[i][j] = box[j]*x;
      vel[i][j] = 0.0;
      acc[i][j] = 0.0;
    }
  }
}

/* Compute the displacement vector (and its norm) between two particles. */
real8 dist(int nd, tm_vnd_t r1, tm_vnd_t r2, vnd_t dr)
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
real8 dotr8(int n, tm_real8* x, tm_real8* y)
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
typedef struct
{
    int np;
    int nd;
    
    tm_vnd_t *pos;
    tm_vnd_t *vel;
    tm_vnd_t *f;
    
    tm_real8 *pot;
    tm_real8 *kin;    
} compute_t;

int run_compute( void* arg, int id )
{
    int i, j, k;
    vnd_t rij;
    real8  d;
    
    compute_t* ct = (compute_t*)arg;
    int np = ct->np;
    int nd = ct->nd;
    tm_vnd_t *pos = ct->pos;
    tm_vnd_t *vel = ct->vel;
    tm_vnd_t *f = ct->f;
    tm_real8 *pot = ct->pot;
    tm_real8 *kin = ct->kin;
    
    //INIT_TRANSACTIONS();
    
    for (i = id; i < np; i += NTHREADS )
    {
        //BEGIN_TRANSACTION()
        
        /* compute potential energy and forces */
        for (j = 0; j < nd; j++)
            f[i][j] = 0.0;
        
        for (j = 0; j < np; j++)
        {
            if (i != j)
            {
                d = dist( nd, pos[i], pos[j], rij);
                
                /* attribute half of the potential energy to particle 'j' */
                (*pot) = (*pot) + 0.5 * vzz(d);
                
                for (k = 0; k < nd; k++)
                    f[i][k] = f[i][k] - rij[k]* dv(d) /d;                
            }
        }
        /* compute kinetic energy */
        (*kin) = (*kin) + dotr8( nd, vel[i], vel[j]);
        
        //COMMIT_TRANSACTION()
    }
    
    return 0;
}

void compute(int np, int nd, tm_vnd_t *pos, tm_vnd_t *vel, real8 mass, tm_vnd_t *f, tm_real8 *pot, tm_real8 *kin) 
{
    *pot = 0.0;
    *kin = 0.0;
    
    /* The computation of forces and energies is fully parallel. */
    //#pragma omp parallel for default(shared) private(i,j,k,rij,d) reduction(+ : pot, kin)
    
    compute_t* ct = (compute_t*)malloc( sizeof(compute_t) );
    ct->np = np;
    ct->nd = nd;
    ct->pos = pos;
    ct->vel = vel;
    ct->f = f;
    ct->pot = pot;
    ct->kin = kin;
    
    //PARALLEL_EXECUTE( NTHREADS, run_compute, ct );
    run_compute( ct, 0 );
    
    (*kin) = (*kin)*0.5*mass;
}
       
/***********************************************************************
 * Perform the time integration, using a velocity Verlet algorithm
 ***********************************************************************/
typedef struct
{
    int np;
    int nd;
    
    tm_vnd_t *pos;
    tm_vnd_t *vel;
    tm_vnd_t *f;
    tm_vnd_t *a;
    
    real8 rmass;
    real8 dt;
} update_t;

int run_update( void* arg, int id )
{
    int i, j;
    
    update_t* ut = (update_t*)arg;
    int np = ut->np;
    int nd = ut->nd;
    tm_vnd_t *pos = ut->pos;
    tm_vnd_t *vel = ut->vel;
    tm_vnd_t *f = ut->f;
    tm_vnd_t *a = ut->a;
    real8 rmass = ut->rmass;
    real8 dt = ut->dt;
    real8 aux0, aux1, aux2, aux3;
    
    //INIT_TRANSACTIONS();
    
    for (i = id; i < np; i += NTHREADS )
    {
        //BEGIN_TRANSACTION()
        
        for (j = 0; j < nd; j++)
        {
            /*
            aux1 = vel[i][j]*dt;
            aux2 = a[i][j]*0.5;            
            pos[i][j] = pos[i][j] + aux1 + aux2*dt*dt;
            
            aux1 = f[i][j]*rmass;
            aux2 = a[i][j];
            vel[i][j] = vel[i][j] + 0.5*dt*(aux1 + aux2);
            
            a[i][j] = aux1;
            */
            aux0 = pos[i][j];
            aux1 = vel[i][j];
            aux2 = a[i][j];
            aux3 = f[i][j];
            
            pos[i][j] = aux0 + aux1*dt + 0.5*dt*dt*aux2;
            vel[i][j] = aux1 + 0.5*dt*(aux3*rmass + aux2);
            a[i][j] = aux3*rmass;
        }
        
        //COMMIT_TRANSACTION()
    }
    
    return 0;
}


void update(int np, int nd, tm_vnd_t *pos, tm_vnd_t *vel, tm_vnd_t *f, tm_vnd_t *a, real8 mass, real8 dt)
{
    real8 rmass;

    rmass = 1.0/mass;

    /* The time integration is fully parallel */
    //#pragma omp parallel for default(shared) private(i,j) firstprivate(rmass, dt)
    
    update_t* ut = (update_t*)malloc( sizeof(update_t) );
    ut->np = np;
    ut->nd = nd;
    ut->pos = pos;
    ut->vel = vel;
    ut->f = f;
    ut->a = a;
    ut->rmass = rmass;
    ut->dt = dt;
    
    //PARALLEL_EXECUTE( NTHREADS, run_update, ut );
    run_update( ut, 0 );

}

/******************
 * main program 
 ******************/

int main (int argc, char **argv)
{
    /* simulation parameters */

    real8 mass = 1.0;
    real8 dt = 1.0e-6;
    vnd_t box;
    
    tm_vnd_t position[nparts];
    tm_vnd_t velocity[nparts];
    tm_vnd_t force[nparts];
    tm_vnd_t accel[nparts];
    
    tm_real8 potential, kinetic;
    real8 E0;

    int i;
    double t0,t1;
    int np;

    for (i = 0; i < ndim; i++)
        box[i] = 10.0;

    /* set initial positions, velocities, and accelerations */
    initialize(nparts,ndim,box,position,velocity,accel);

    t0 = t();

    //CREATE_TM_THREADS( NTHREADS );

    /* compute the forces and energies */
    compute( nparts, ndim, position, velocity, mass, force, &potential, &kinetic);
    E0 = potential + kinetic;

    /* This is the main time stepping loop */
    for (i = 0; i < nsteps; i++)
    {
        compute( nparts, ndim, position, velocity, mass, force, &potential, &kinetic);
#if 1
        printf(R8F " " R8F " " R8F "\n", (real8)potential, (real8)kinetic, (real8)(potential + kinetic - E0)/E0);
#endif
        
        update( nparts, ndim, position, velocity, force, accel, mass, dt);
    }

    //DESTROY_TM_THREADS( NTHREADS );

    t1 = t();

    printf("Execution time\t %f s\n", t1-t0 );

    exit (0);
}

