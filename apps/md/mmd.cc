// 
// File:   mmd.cc
// Author: daniel
//
// Created on April 10, 2007, 3:19 PM
//
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../src/tm.h"
#include "../../src/utils/hrtime.h"

#define NTHREADS 1
typedef double real8;

typedef tm_type<real8> tm_real8;
//typedef real8 tm_real8;

#define nparts 4000
#define nsteps 10000 //10000

int np = nparts;

tm_real8 pos[nparts];
//tm_real8 vel[nparts];
//tm_real8 f[nparts];
//tm_real8 a[nparts];
    
//tm_real8 pot, kin;

//real8 d[nparts];

double bt = 0.0;
double ct = 0.0;
double et = 0.0;

//extern double _ct1, _ct2, _ct3, _ct4;

int run_compute( void* arg, int id )
{
    int i, j, k;
    real8  d;
    
    double t0, t1, t2, t3;
    t0 = get_td();
    
    BEGIN_TRANSACTION();
    
    t1 = get_td();    
    
    
    for (i = id; i < np; i += NTHREADS )
    {
        d = pos[i]+1;
        //pos[i] = 3;
    }
    
    t2 = get_td();
    
    COMMIT_TRANSACTION();
    
    t3 = get_td();
    
    bt += t1-t0;
    et += t2-t1;
    ct += t3-t2;
    return 0;
}


int main(int argc, char** argv)
{
    int i;
    double t0, t1;
    
    t0 = get_td();
    
    for( i = 0; i < nsteps; i++ )
        run_compute( NULL, 0);
    
    t1 = get_td();
    
    
    printf("Execution time\t %lf s   bt: %lf et: %lf ct: %lf  \n", t1-t0, bt, et, ct );
    
    return (EXIT_SUCCESS);
}

