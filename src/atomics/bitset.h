/* 
 * File:   bitset.h
 * Author: daniel
 *
 * Created on April 22, 2007, 7:59 PM
 */

#ifndef _bitset_H
#define	_bitset_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../tm_general.h"

#define PAUSE() asm volatile ("pause" ::: "memory")

#define is_set( src_bitset, index )         ((src_bitset)  & ( BITMASK << (index) ))
#define is_clear( src_bitset, index )       ((~(src_bitset)) & ( BITMASK << (index) ))
#define set_bit( dst_bitset, index )        (dst_bitset) |= ( BITMASK << (index) )
#define clear_bit( dst_bitset, index )      (dst_bitset) &= ( ~(BITMASK << (index)) )
#define set_mask( dst_bitset, __mask )      (dst_bitset) |= ( __mask )
#define clear_mask( dst_bitset, __mask )    (dst_bitset) &= ( ~(__mask) )

#define atomic_set_mask( mask, addr)    asm volatile( "lock ; orq %0,%1" : : "r" (mask),"m" (*(addr)) : "memory" )
#define atomic_clear_mask( mask, addr ) asm volatile( "lock ; andq %0,%1" : : "r" (~(mask)),"m" (*(addr)) : "memory" )

#define atomic_add( a_i, addr )         asm volatile( "lock ; addq %1,%0" : "+m" (*(addr)) :"ir" (a_i) : "memory" )

#define atomic_inc( addr )              asm volatile( "lock ; incq %0" :"+m" (*(addr)))
#define atomic_dec( addr )             	asm volatile( "lock ; decq %0" :"+m" (*(addr)))

inline __attribute__((always_inline))
unsigned long cas( word_t* w_ptr, unsigned long w_old, unsigned long w_new)
{
    unsigned long w_prev;
    asm volatile("lock; cmpxchgq %1, %2;" : "=a"(w_prev) : "q"(w_new), "m"(*(w_ptr)), "a"(w_old) : "memory");
    return w_prev;
}

/**
 * Atomically add an integer a_i to memory address addr
 */
inline __attribute__((always_inline))
int atomic_add_return_prev( long a_i, word_t* addr)
{
    asm volatile( "lock; xaddq %0, %1" : "+r" (a_i), "+m" (*(addr)) : : "memory");
    return a_i;
}

#define MEMBAR()	asm volatile( "mfence" : : : "memory" )

#define xchg( ptr, x)		__asm__ __volatile__("lock; xchgl %0,%1" :"=r" (x) :"m" (*(ptr)), "0" (x) :"memory")
#define set_mb(addr, value)	({ int val = (value);xchg(addr, val);   })  


/*
inline __attribute__((always_inline))
unsigned long long cas8( dword_t* d_ptr, unsigned long long d_old, unsigned long long d_new )
{
	unsigned long long d_prev;
	asm volatile("lock; cmpxchg8b %3" : "=A"(d_prev) : "b"((unsigned long)(d_new)), "c"((unsigned long)((d_new)>>32)), "m"(*(d_ptr)), "0"(d_old) : "memory");
	return d_prev;
}

inline __attribute__((always_inline))
unsigned long long cas8( dword_t* d_ptr, unsigned long long d_old, unsigned long long d_new )
{																															
	unsigned long long d_prev;																											
	asm volatile("lock; cmpxchg8b %3" : "=A"(d_prev) : "b"((unsigned long)(d_new)), "c"((unsigned long)(d_new)), "m"(*(d_ptr)), "0"(d_old) : "memory");
	return d_prev;																													
}


#define is_set8( src_bitset, index )         ((src_bitset)  & ((unsigned long long) BITMASK << (index) ))
#define is_clear8( src_bitset, index )       ((~(src_bitset)) & ((unsigned long long) BITMASK << (index) ))
#define set_bit8( dst_bitset, index )        (dst_bitset) |= ((unsigned long long) BITMASK << (index) )
#define clear_bit8( dst_bitset, index )      (dst_bitset) &= ( ~((unsigned long long) BITMASK << (index)) )



inline __attribute__((always_inline))
void atomic_set_mask8( unsigned long long mask, dword_t* addr)
{
	unsigned long long tmp;
	while(1)
	{
		tmp = *addr;
		if( cas8( addr, tmp, tmp | mask ) == tmp )		break;
	}
}

inline __attribute__((always_inline))
void atomic_clear_mask8( unsigned long long mask, dword_t* addr )
{
	unsigned long long tmp;
	while(1)
	{
		tmp = *addr;
		if( cas8( addr, tmp, tmp & (~mask) ) == tmp )	break;
	}
}


#define _atomic_clear_mask( mask, addr )  *addr &= (~(mask))
#define _atomic_add( a_i, addr )         *addr += a_i

#define _cas( w_ptr, w_old, w_new)                                                                               \
({														\
    *w_ptr = w_new;												\
    w_old;													\
})
*/
#ifdef	__cplusplus
}
#endif

#endif	/* _bitset_H */

