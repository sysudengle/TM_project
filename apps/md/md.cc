/* Simple molecular dynamics simulation */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <iostream>

#include "../../src/tm.h"

#define NTHREADS 4

#ifndef RAND_MAX
#define RAND_MAX 0x7fff
#endif

#define ndim 3
#define nparts 512
#define nsteps 10
#define R8F "% 30.20e" //

typedef double real8;
typedef real8 vnd_t[ndim];

typedef tm_type<double> tm_real8;
//typedef real8 tm_real8;

typedef tm_real8 tm_vnd_t[ndim];

int nd = ndim;
int np = nparts;
real8 mass = 1.0;
real8 rmass;
real8 dt = 0.1;//1.0e-6;
vnd_t box;

tm_vnd_t pos[nparts];
tm_vnd_t vel[nparts];
tm_vnd_t f[nparts];
tm_vnd_t a[nparts];
	
tm_real8 pot, kin;

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
int run_compute( void* arg, int id )
{
	int i, j, k;
	vnd_t rij;
	real8  d;
	
	for (i = id; i < np; i += NTHREADS )
	{
		BEGIN_TRANSACTION();
		
		/* compute potential energy and forces */
		for (j = 0; j < nd; j++)
			f[i][j] = 0.0;
		
		for (j = 0; j < np; j++)
		{
			if (i != j)
			{
				d = dist( nd, pos[i], pos[j], rij);
				
				/* attribute half of the potential energy to particle 'j' */
				pot = pot + 0.5 * vzz(d);
				
				for (k = 0; k < nd; k++)
					f[i][k] = f[i][k] - rij[k]* dv(d) /d;                
			}
		}
		/* compute kinetic energy */
		kin = kin + dotr8( nd, vel[i], vel[j]);
		
		COMMIT_TRANSACTION();
	}
	
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
	
	kin = kin*0.5*mass;
}
	
/***********************************************************************
* Perform the time integration, using a velocity Verlet algorithm
***********************************************************************/
int run_update( void* arg, int id )
{
	int i, j;
	real8 aux0, aux1, aux2, aux3;
	
	for (i = id; i < np; i += NTHREADS )
	{
		BEGIN_TRANSACTION();
		
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
		
		COMMIT_TRANSACTION();
	}
	
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

int main (int argc, char **argv)
{
	/* simulation parameters */

	real8 E0;

	int i;
	double t0,t1;
	
	for (i = 0; i < nd; i++)
		box[i] = 10.0;

	/* set initial positions, velocities, and accelerations */
	initialize();
	rmass = 1.0/mass;

	t0 = t();

	//set_version(2, 2);

	CREATE_TM_THREADS( NTHREADS );

	/* compute the forces and energies */
	compute();
	E0 = pot + kin;

	/* This is the main time stepping loop */
	for (i = 0; i < nsteps; i++)
	{
		compute();

		printf(R8F " " R8F " " R8F "\n", (real8)pot, (real8)kin, (real8)(pot + kin - E0)/E0);

		
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

	printf("Execution time\t %f s\n", t1-t0 );

	exit (0);
}

