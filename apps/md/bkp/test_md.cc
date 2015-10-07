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
#define nparts 10
#define nsteps 5
#define R8F "% 30.20e" //

typedef double real8;
typedef real8 vnd_t[ndim];

template <typename T, int __shared = 0>
class _tm_type
{
protected:
    // The managed value
    T	_t;
    
    // The meta-information
    //meta_t __meta[ __shared ];

public:
    // Default ctor
    _tm_type()
    {   /*if( __shared )            mgr_t::initialize_meta( &__meta[0] );*/    }
    // Copy ctor
    _tm_type( const T & _r) : _t( _r )
    {   /*printf("copy\n");if( __shared )            mgr_t::initialize_meta( &__meta[0] );*/    }
    
    // Default destructor
    ~_tm_type()
    {   /*if( __shared )            mgr_t::destroy_meta( &__meta[0] ); */       }

    

public:
    // Access operator
    operator T ()
    {
        // if in a transaction and the variable is shared
        /*if( __tran__ && __shared )
        {
	    printf("%u  read %x\n", __tran__->tran_id,  &_t);
            cptr_t p_t = reinterpret_cast< cptr_t>( &_t);
            T const* rez = reinterpret_cast< T const* >( mgr_t::on_rd( p_t, sizeof( T ), &__meta[0] ) );
            return (* rez);            
        }
        else*/
            return _t;
    }
private:
    //write hook
    T& write( T const& _r )
    {
        /*if( __tran__ )
        {
            printf("%u  write %x\n", __tran__->tran_id,  &_t);
            ptr_t p_t = reinterpret_cast< ptr_t>( &_t);
            cptr_t p_r = reinterpret_cast< cptr_t>( &_r);
            T* rez = reinterpret_cast<T*>( mgr_t::on_wr( p_t, p_r, sizeof( T ), &__meta[0], __shared ) );
            return (* rez);            
        }
        else
        {*/
            _t = _r;
            return _t;
        //}
    }
    
public:
    // Assignment operator
    T & operator = ( T const& _r)
    {
        _t = _r;
        return _t;
        //return write( _r );
    }
};


typedef _tm_type<double,1> tm_real8;
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
    
    //printf("%x id %d   pos %x, vel %x, f %x, pot %x, kin %x \n", pthread_self(), id, pos, vel, f, pot, kin);
    
    //INIT_TRANSACTIONS();
    
    for (i = id; i < np; i += NTHREADS )
    {
        //BEGIN_TRANSACTION()
        
        /* compute potential energy and forces */
        for (j = 0; j < nd; j++)
            f[i][j] = 0.0;
        
        //printf( "%x id %d   after f = 0\n", pthread_self(), id );

        for (j = 0; j < np; j++)
        {
            if (i != j)
            {
                //printf( "_  %x  id %d   before dist %d\n", pthread_self(), id, j );
                
                d = dist( nd, pos[i], pos[j], rij);
                
                //printf( "__  %x  id %d   after dist %d\n", pthread_self(), id, j );
                
                /* attribute half of the potential energy to particle 'j' */
                (*pot) = (*pot) + 0.5 * vzz(d);
                
                for (k = 0; k < nd; k++)
                    f[i][k] = f[i][k] - rij[k]* dv(d) /d;                
            }
        }
        /* compute kinetic energy */
        (*kin) = (*kin) + dotr8( nd, vel[i], vel[j]);
        
        //printf( "_  %x  id %d   before dist %d\n", pthread_self(), id, j );
        
        printf( "comp: f[%d] ", i );print_d3( real8(f[i][0]), real8(f[i][1]), real8(f[i][2]) );printf("\n");
        printf( "comp: %d pot: ",i);print_d( real8(*pot));printf(" kin: ");print_d(real8(*kin));printf("\n");
        
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
    printf( "comp_fin: pot: ");print_d( real8(*pot));printf(" kin: ");print_d(real8(*kin));printf("\n");
    //printf( "comp_fin: pot: " R8F " kin: " R8F "\n", real8(*pot), real8(*kin) );
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

void delim()
{}
void idelim()
{}

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
    
    //INIT_TRANSACTIONS();
    
    for (i = id; i < np; i += NTHREADS )
    {
        //BEGIN_TRANSACTION()
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
    real8 dt = 0.1;//1.0e-6;
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
        printf( "\nstep: %d \n", i);
        compute( nparts, ndim, position, velocity, mass, force, &potential, &kinetic);
        
#if 1
        //printf(R8F " " R8F " " R8F "\n", (real8)potential, (real8)kinetic, (real8)(potential + kinetic - E0)/E0);
        printf("\n");print_d3( real8(potential), real8(kinetic), real8((potential + kinetic - E0)/E0) );printf("\n");
        
#endif
        
        update( nparts, ndim, position, velocity, force, accel, mass, dt);
    }

    //DESTROY_TM_THREADS( NTHREADS );

    t1 = t();

   printf("Execution time\t %f s\n", t1-t0 );

    exit (0);
}

