#ifndef TM_GENERAL_H_
#define TM_GENERAL_H_

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <setjmp.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//	Enable/Disable support for "tw_type" and condition variables
#define ENABLE_EXTRAS

//#define DISABLE_ALL
//#define DISABLE_LOCKS

/****************************************************************
							Constants
****************************************************************/
#ifdef _LP64

# define MAX_NO_THREADS		62  // 0 - 61
# define REDIRECTION_BIT 	62
# define WRITE_BIT		63  // for locking
# define BITMASK		1LL // define a general mask

#else

# define MAX_NO_THREADS		30  // 0 - 29
# define REDIRECTION_BIT 	30
# define WRITE_BIT		31  // for locking
# define BITMASK		1   // define a general mask

#endif


/****************************************************************
				Debugging and logging related macros
****************************************************************/
#ifdef TM_DEBUG

#define tm_assert( expr )	assert( expr )

#else

#define tm_assert( expr )

#endif

#ifdef TM_INFO

#define tm_log( log_str )		({fprintf( stderr, log_str, p_thread_id );	fflush(stderr);})
#define tm_log1( log_str, a1 )		({fprintf( stderr, log_str, p_thread_id, a1 );	fflush(stderr);})
#define tm_log2( log_str, a1, a2 )	({fprintf( stderr, log_str, p_thread_id, a1, a2 ); fflush(stderr);})
#define tm_log3( log_str, a1, a2, a3 )	({fprintf( stderr, log_str, p_thread_id, a1, a2, a3 );	fflush(stderr);})

#else

#define tm_log( log_str )
#define tm_log1( log_str, a1 )
#define tm_log2( log_str, a1, a2 )
#define tm_log3( log_str, a1, a2, a3 )

#endif

/****************************************************************
						Type Definitions
****************************************************************/
// Pointers
typedef unsigned char	byte_t;
typedef byte_t*		ptr_t;
typedef const byte_t*	cptr_t;


// Integers (added 64 bit compatibility)
#ifdef _LP64

	typedef uint64_t	uint_t;
	typedef int64_t		int_t;

	typedef volatile unsigned long long	word_t;
	typedef volatile unsigned long long 	dword_t;

#else

	typedef uint32_t	uint_t;
	typedef int32_t		int_t;

	typedef volatile unsigned int 		word_t;
	typedef volatile unsigned long long 	dword_t;

#endif

typedef unsigned short		ushort_t;
typedef unsigned long		ulong_t;
typedef unsigned long long	ullong_t;

// Transaction Types
typedef enum _tran_type_t{HW, HYBRID, SW} 	tran_type_t;

typedef volatile unsigned long long tm_mutex_t;
typedef struct _vert_t {
	unsigned long   tag_version;
	unsigned long   conflict_counter;
	//tm_mutex_t		tag_lock;
} ver_t;

typedef ver_t*		ver_ptr_t;

#define TAG_RETRY 1


/****************************************************************
					Compiling Related
****************************************************************/

// Branching helpers
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define ___always_inline inline
//__attribute__((always_inline))

/****************************************************************
					Memory Copying
****************************************************************/
___always_inline  static
void tm_memcpy( ptr_t dest_addr, ptr_t src_addr, size_t size )
{
	uint_t i;
	for( i = 0; i < (size >> 2); i++ )
	    ((int*)dest_addr)[i] = ((int*)src_addr)[i];
	for( i = (size & (~3)); i < size; i++ )
	    dest_addr[i] = src_addr[i];
}

/****************************************************************
					Memory Comparison
****************************************************************/
___always_inline  static
int tm_memcmp( ptr_t orig_addr, ptr_t current_addr, size_t size )
{
	//printf("comparing %x vs. %x\n", current_addr, orig_addr);

	uint_t i;
	for( i = 0; i < (size >> 2); i++ ) {
	    if (((int*)orig_addr)[i] != ((int*)current_addr)[i]) {
	    	//printf("values %d vs. %d\n", ((int*)orig_addr)[i], ((int*)current_addr)[i]);
	    	return 0;
	    }
	}
	for( i = (size & (~3)); i < size; i++ ) {
		if (orig_addr[i] != current_addr[i]) {
			//printf("values %c vs. %c\n", orig_addr[i], current_addr[i]);
			return 0;
		}
	}

	return 1;
}


#endif /*TM_GENERAL_H_*/
