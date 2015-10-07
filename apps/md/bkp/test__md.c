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

#define ndim 3
#define nparts 10
#define nsteps 5

#define R8F "% 30.20e" //

typedef double real8;
typedef real8 vnd_t[ndim] ;

double t() {
    struct timeval tv;
    gettimeofday(&tv, ((timezone *)0));
    return (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

/* statement function for the pair potential and its derivative
   This potential is a harmonic well which smoothly saturates to a
   maximum value at PI/2.  */

void print_d(double d)
{
    char* pd = (char*)&d;
    
    for( int i = 7; i >= 0; i-- )
    {
        for( int j = 7; j >= 0; j-- )
            if( (pd[i] & (1 << j)) )
                printf("1");
            else
                printf("0");
    }
    printf(" ");
    
}
void print_d3(double d1, double d2, double d3)
{
    print_d( d1 );
    print_d( d2 );
    print_d( d3 );
}




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
		vnd_t box, vnd_t *pos, vnd_t *vel, vnd_t *acc)
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
void compute(int np, int nd, 
	     real8 *box, 
	     vnd_t *pos, vnd_t *vel, 
	     real8 mass, vnd_t *f, 
	     real8 *pot_p, real8 *kin_p) 
{
  real8 x;
  int i, j, k;
  vnd_t rij;
  real8  d;
  real8 pot, kin;
  
  pot = 0.0;
  kin = 0.0;
  
  /* The computation of forces and energies is fully parallel. */
#pragma omp parallel for default(shared) private(i,j,k,rij,d) reduction(+ : pot, kin)
  for (i = 0; i < np; i++) {
    /* compute potential energy and forces */
    for (j = 0; j < nd; j++)
      f[i][j] = 0.0;
    
    for (j = 0; j < np; j++) 
    {
      if (i != j) 
      {
	d = dist(nd,pos[i],pos[j],rij);
	/* attribute half of the potential energy to particle 'j' */
	pot = pot + 0.5 * vzz(d);
	for (k = 0; k < nd; k++) 
        {
	  f[i][k] = f[i][k] - rij[k]* dv(d) /d;
	}
      }
    }
    /* compute kinetic energy */
    kin = kin + dotr8(nd,vel[i],vel[j]);
    /*
    printf( "comp: f[%d] " R8F " " R8F " " R8F "\n", i, real8(f[i][0]), real8(f[i][1]), real8(f[i][2]) );
    printf( "comp: %d pot: " R8F " kin: " R8F "\n", i, real8(pot), real8(kin) );
    */
    printf( "comp: f[%d] ", i );print_d3( real8(f[i][0]), real8(f[i][1]), real8(f[i][2]) );printf("\n");
    printf( "comp: %d pot: ",i);print_d( real8(pot));printf(" kin: ");print_d(real8(kin));printf("\n");
        
  }
  
  kin = kin*0.5*mass;
  
  //printf( "comp_fin: pot: " R8F " kin: " R8F "\n", real8(pot), real8(kin) );
  printf( "comp_fin: pot: ");print_d( real8(pot));printf(" kin: ");print_d(real8(kin));printf("\n");
    
  *pot_p = pot;
  *kin_p = kin;
}
      
void delim()
{}
void idelim()
{}
/***********************************************************************
 * Perform the time integration, using a velocity Verlet algorithm
 ***********************************************************************/
void update(int np, int nd, vnd_t *pos, vnd_t *vel, vnd_t *f, vnd_t *a,
	    real8 mass, real8 dt)
{
  int i, j;
  real8 rmass;
  
  rmass = 1.0/mass;
  
  /* The time integration is fully parallel */
#pragma omp parallel for default(shared) private(i,j) firstprivate(rmass, dt)
  for (i = 0; i < np; i++) 
  {
      /*
        printf( "bef_upd: pos[%d] " R8F " " R8F " " R8F "\n", i, real8(pos[i][0]), real8(pos[i][1]), real8(pos[i][2]) );
        printf( "bef_upd: vel[%d] " R8F " " R8F " " R8F "\n", i, real8(vel[i][0]), real8(vel[i][1]), real8(vel[i][2]) );
        printf( "bef_upd: a[%d] " R8F " " R8F " " R8F "\n", i, real8(a[i][0]), real8(a[i][1]), real8(a[i][2]) );
        printf( "bef_upd: f[%d] " R8F " " R8F " " R8F "\n", i, real8(f[i][0]), real8(f[i][1]), real8(f[i][2]) );
        printf( "bef_upd: [%d] rmass: " R8F " dt: " R8F "\n", i, real8(rmass), real8(dt));
        */
        printf( "bef_upd: pos[%d] ", i );print_d3( real8(pos[i][0]), real8(pos[i][1]), real8(pos[i][2]) );printf("\n");
        printf( "bef_upd: vel[%d] ", i );print_d3( real8(vel[i][0]), real8(vel[i][1]), real8(vel[i][2]) );printf("\n");
        printf( "bef_upd: a[%d] ", i );print_d3( real8(a[i][0]), real8(a[i][1]), real8(a[i][2]) );printf("\n");
        printf( "bef_upd: f[%d] ", i );print_d3( real8(f[i][0]), real8(f[i][1]), real8(f[i][2]) );printf("\n");
        printf( "bef_upd: [%d] rmass: ",i);print_d( real8(rmass));printf(" dt: ");print_d(real8(dt));printf("\n");
        
        
        for (j = 0; j < nd; j++) 
        {
            real8 __x1 = 0.5*dt;
            real8 __x2 = __x1*dt;
            delim();
            real8 __x3 = 0.5*a[i][j]*dt*dt;
            idelim();
            real8 _x1 = 0.5*dt*dt*a[i][j];
            delim();
            real8 _x2 = vel[i][j]*dt;
            real8 _x3 = _x1 + _x2;
            
            pos[i][j] = pos[i][j] + _x3;
            
            printf( "upd: __x1 [%d][%d] ", i, j );print_d( real8(__x1) );printf("\n");
            printf( "upd: __x2 [%d][%d] ", i, j );print_d( real8(__x2) );printf("\n");
            printf( "upd: __x3 [%d][%d] ", i, j );print_d( real8(__x3) );printf("\n");
            
            printf( "upd: _x1 [%d][%d] ", i, j );print_d( real8(_x1) );printf("\n");
            printf( "upd: _x2 [%d][%d] ", i, j );print_d( real8(_x2) );printf("\n");
            printf( "upd: _x3 [%d][%d] ", i, j );print_d( real8(_x3) );printf("\n");
            
            vel[i][j] = vel[i][j] + 0.5*dt*(f[i][j]*rmass + a[i][j]);
            a[i][j] = f[i][j]*rmass;
        }
/*
        printf( "upd: pos[%d] " R8F " " R8F " " R8F "\n", i, real8(pos[i][0]), real8(pos[i][1]), real8(pos[i][2]) );
        printf( "upd: vel[%d] " R8F " " R8F " " R8F "\n", i, real8(vel[i][0]), real8(vel[i][1]), real8(vel[i][2]) );
        printf( "upd: a[%d] " R8F " " R8F " " R8F "\n", i, real8(a[i][0]), real8(a[i][1]), real8(a[i][2]) );
*/
        printf( "upd: pos[%d] ", i );print_d3( real8(pos[i][0]), real8(pos[i][1]), real8(pos[i][2]) );printf("\n");
        printf( "upd: vel[%d] ", i );print_d3( real8(vel[i][0]), real8(vel[i][1]), real8(vel[i][2]) );printf("\n");
        printf( "upd: a[%d] ", i );print_d3( real8(a[i][0]), real8(a[i][1]), real8(a[i][2]) );printf("\n");
        
  }
}

/******************
 * main program 
 ******************/



int main (int argc, char **argv) {
  
  /* simulation parameters */
  
  real8 mass = 1.0;
  real8 dt = 0.1;//1.0e-6;
  vnd_t box;
  vnd_t position[nparts];
  vnd_t velocity[nparts];
  vnd_t force[nparts];
  vnd_t accel[nparts];
  real8 potential, kinetic, E0;
  int i;
  double t0,t1;
  int np;

  for (i = 0; i < ndim; i++)
    box[i] = 10.0;
    
  /* set initial positions, velocities, and accelerations */
  initialize(nparts,ndim,box,position,velocity,accel);

  t0 = t();

  /* compute the forces and energies */
  compute(nparts,ndim,box,position,velocity,mass,
	  force,&potential,&kinetic);
  E0 = potential + kinetic;

  /* This is the main time stepping loop */
  for (i = 0; i < nsteps; i++) 
  {
    printf( "\nstep: %d \n", i);
    compute(nparts,ndim,box,position,velocity,mass,force,&potential,&kinetic);

#if 1
    //printf(R8F " " R8F " " R8F "\n", potential, kinetic, (potential + kinetic - E0)/E0);
    printf("\n");print_d3( real8(potential), real8(kinetic), real8((potential + kinetic - E0)/E0) );printf("\n");
#endif

    update(nparts,ndim,position,velocity,force,accel,mass,dt);
  }

  t1 = t();

#ifdef _OPENMP
  np = omp_get_max_threads();
#else
  np = 1;
#endif
  printf("Execution time\t %f s\n", t1-t0);

  exit (0);
}

