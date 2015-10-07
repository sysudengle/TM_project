/*
 * File:   seqbuff.h
 * Author: daniel
 *
 * Created on May 7, 2007, 2:42 PM
 */

#ifndef _tatas_H
#define	_tatas_H

#ifdef	__cplusplus
extern "C" {
#endif

#define align_64
    //__attribute__ ((aligned(64)))

typedef volatile unsigned int _tatas_lock_t;

#define tatas_lock_t    _tatas_lock_t
#define TATAS_LOCK_INITIALIZER      0
#define tatas_init      _tatas_init
#define tatas_lock      _tatas_lock
#define tatas_unlock    _tatas_unlock
#define tatas_islocked	_tatas_islocked

//#define tatas_lock_t    pthread_mutex_t
//#define TATAS_LOCK_INITIALIZER      PTHREAD_MUTEX_INITIALIZER
//#define tatas_init      pthread_mutex_init
//#define tatas_lock      pthread_mutex_lock
//#define tatas_unlock    pthread_mutex_unlock


#define tas( ptr, result )                      \
({                                              \
    asm volatile("lock;"                        \
                 "xchgl %0, %1;"                \
                 : "=r"(result), "=m"(*ptr)     \
                 : "0"(1), "m"(*ptr)            \
                 : "memory");                   \
    result;                                     \
})

#define nop()    asm volatile("nop")

#define backoff( b )                        \
{                                           \
    int b_i;                                \
    for( b_i = b; b_i; b_i-- )   nop();     \
    if( b < 1024 )          b <<= 1;        \
}

inline __attribute__((always_inline))
void _tatas_init( _tatas_lock_t* L, void* v )
{
    *L = 0;
}

inline __attribute__((always_inline))
unsigned long long _tatas_lock( _tatas_lock_t* L )
{
    int b = 64, i;
    unsigned int rez;
    unsigned long long w = 0;

    if( tas( L, rez ) )
    {
        do
        {
            w += b;
            for( i = b; i; i-- )   nop();
            if( b < 4096 )          b <<= 1;
        }
        while( tas( L, rez ) );
    }
    return w;
}

inline __attribute__((always_inline))
void _tatas_unlock( _tatas_lock_t* L )
{
    *L = 0;
}

inline __attribute__((always_inline))
int _tatas_islocked( _tatas_lock_t* L )
{
    return (*L == 0);
}

#ifdef	__cplusplus
}
#endif

#endif	/* _tatas_H */

