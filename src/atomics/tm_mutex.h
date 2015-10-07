#ifndef __TM_MUTEX_H
#define __TM_MUTEX_H

#include <immintrin.h>
#include "bitset.h"
#include "../tm_general.h"


inline void mutex_lock(tm_mutex_t *lock, long thread_id) {
	while(cas(lock, 0, thread_id) != 0) {
	    do { PAUSE(); } while(*lock != 0);
	}
}

inline void mutex_unlock(tm_mutex_t *lock) {
	set_mb(lock, 0);
}

inline int mutex_is_locked(tm_mutex_t *lock) {
	return *lock != 0;
}

inline int mutex_is_free(tm_mutex_t *lock) {
	return *lock == 0;
}

inline int mutex_is_locked_by_me(tm_mutex_t *lock, long t_id ) {
	return *lock == t_id;
}

inline int mutex_is_locked_by_another(tm_mutex_t *lock, long t_id ) {
	return (*lock != 0) && (*lock != t_id);
}

#endif  //__TM_MUTEX_H
