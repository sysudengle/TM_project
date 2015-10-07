/*
 *  tm.h
 *
 *  Defines the LibTM Transactional Memory Library API
 *
 *  Created on: 2014-05-05
 *      Author: Mike Dai Wang
 */

#ifndef TM_H_
#define TM_H_

#include "tm_general.h"
#include "utils/tm_threads.h" 		// Threading related macros
#include "tm_mgr_x.h"

/**************************************************************************
 *		Transaction Setup and Teardown
 **************************************************************************/
// Global fall-back lock between HW/Hybrid and SW transactions
static tm_mutex_t 	fallback_lock = 0;

#define TM_END()	mgr_on_end()
#define TM_INIT()

static const int MAX_HW_RETRIES = 6;
static const int MAX_HYBROD_HW_RETRIES = 0;

/**
 * added these as possible optimizations
 *
 * 	static tran_type_t likely_txn_type = HW;		\
	__txn_type = __txn_type < likely_txn_type ? __txn_type : likely_txn_type; \
 */
#define BEGIN_TRANSACTION()							\
	assign_thread_id();							\
	__in_transact = 1;							\
	tran_type_t txn_type = HW;	   					\
	cstats_inc(p_trans.stats.n_start_total);     				\
	switch (txn_type) {							\
		case HW:							\
			if (begin_hw(&fallback_lock, MAX_HW_RETRIES)) {		\
				break;						\
			}							\
		case HYBRID:							\
			txn_type = HYBRID;					\
			__trap_access = 1;					\
			__hybrid_aborted = 0;					\
			cstats_inc(p_trans.stats.n_start_hybrid); 		\
			sigsetjmp( *mgr_on_begin(), 0 );			\
			if (__hybrid_aborted == 0) {				\
				break;						\
			}							\
		case SW:							\
			txn_type = SW;						\
			__trap_access = 0;					\
			cstats_inc(p_trans.stats.n_start_sw); 			\
			begin_sw(&fallback_lock);				\
			break;							\
		default:							\
			break;							\
	}

#define END_TRANSACTION()							\
	switch (txn_type) {							\
		case HYBRID:							\
			mgr_on_commit(&fallback_lock);				\
			break;							\
		default:							\
			txn_end(&fallback_lock);				\
	}									\
	__in_transact = 0;							\
	__trap_access = 0;



#define CHECK_TRANSACTION()			mgr_on_check()
#define SAVE_TRANSACTION( s_id )	0

/**************************************************************************
 *		TM_OBJ - MEMORY MANAGEMENT
 **************************************************************************/
void* 	mgr_on_new(size_t size);
void 	mgr_on_delete(void* ptr);
void 	tm_delete(void* ptr);

class tm_obj {
 public:
	void* operator new(size_t size) {
		return mgr_on_new(size);
	}

	void* operator new[](size_t size) {
		return mgr_on_new(size);
	}

	void operator delete(void* ptr) {
		mgr_on_delete(ptr);
	}

	void operator delete[](void* ptr) {
		mgr_on_delete(ptr);
	}
};

/**************************************************************************
 *		Macros
 **************************************************************************/

#define  ReadValue( r_addr, __meta )							\
({																\
	(*(T*)mgr_on_rd( r_addr, __meta ));							\
})

#define  ReadT()				ReadValue( ((ptr_t) &_value), __meta )

#define  ReadTmT( tm_r )		ReadValue( ((ptr_t) &tm_r._value), tm_r.__meta )

#define  WriteT()			((T*)mgr_on_wr( (ptr_t)&_value, sizeof(T), __meta ))

/**************************************************************************
 *		tm_type definitions
 **************************************************************************/

template< typename T >
class tm_type: public tm_obj {
 private:

	typedef tm_type<T> tm_T;

 public:

	volatile T 	_value;
	ver_t 	*__meta;
	//backup pointer, always points to object's own tag object
	//should never be used _directly_; should only ever be used
	//to restore the __meta pointing to the original tag object.
	//once assigned for the first time, there should be no need to ever modify
	//this pointer again. Name stands for "fine-grained meta".
	//Only players should ever really use this, other tm_objects should ignore it.
	//To ensure this, it's best if only the _application_ uses it, NOT the library
	//(because the library doesn't know what kind of tm_object it is using).
	ver_t * __fg_meta;

	tm_type() {
		__meta = 0;
		__fg_meta = 0;
	}

	explicit tm_type(const T &new_value) {
		_value = new_value;
		__meta = 0;
		__fg_meta = 0;
	}

	/**
	 * Conversion operator, converting tm_T to T,
	 * essentially an implicit conversion on read
	 */
	operator volatile T() {
		return ReadT();
	}

	/**
	 * Assignment operator with object of type T
	 */
	T& operator=(const T &new_value) {
		if( !__trap_access ) {
			_value = new_value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}

		return (*WriteT() = new_value);
	}

	/*  Assignment operator with object of type tm_T */
	T& operator=(const tm_T &rhs_tm) {
		if( !__trap_access ) {
			_value = rhs_tm._value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}

		return (*WriteT() = ReadTmT( rhs_tm ));
	}

	/**************************************************************************
	 *		Increment and Decrement Operators
	 **************************************************************************/
	T& operator++() {
		if( !__trap_access ) {
			++_value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}

		T current_value = ReadT();
		return (*WriteT() = current_value + 1);
	}

	T& operator--() {
		if( !__trap_access ) {
			--_value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}

		T current_value = ReadT();
		return (*WriteT() = current_value - 1);
	}

	T operator++(int) {
		T current_value;
		if( !__trap_access ) {
			current_value = _value++;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return current_value;
		}
		current_value = ReadT();
		*WriteT() = current_value + 1;
		return current_value;
	}

	T operator--(int) {
		T current_value;
		if( !__trap_access ) {
			current_value = _value--;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return current_value;
		}
		current_value = ReadT();
		*WriteT() = current_value - 1;
		return current_value;
	}

	/**************************************************************************
	 *		Compound Assignment Operators
	 **************************************************************************/
	T& operator+=(const T &rhs_value) {
		if( !__trap_access ) {
			_value += rhs_value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}

		T current_value = ReadT();
		return (*WriteT() = current_value + rhs_value);
	}

	T& operator-=(const T &rhs_value) {
		if( !__trap_access ) {
			_value -= rhs_value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}
		T current_value = ReadT();
		return (*WriteT() = current_value - rhs_value);
	}

	T& operator*=(const T &rhs_value) {
		if( !__trap_access ) {
			_value *= rhs_value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}
		T current_value = ReadT();
		return (*WriteT() = current_value * rhs_value);
	}

	T& operator/=(const T &rhs_value) {
		if( !__trap_access ) {
			_value /= rhs_value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}
		T current_value = ReadT();
		return (*WriteT() = current_value / rhs_value);
	}

	/**************************************************************************
	 *		Compound Assignment Operators With Objects
	 **************************************************************************/
	T& operator+=(const tm_T &rhs_tm) {
		if( !__trap_access ) {
			_value += rhs_tm._value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}
		T current_value = ReadT();
		return (*WriteT() = current_value + ReadTmT( rhs_tm ));
	}

	T& operator-=(const tm_T &rhs_tm) {
		if( !__trap_access ) {
			_value -= rhs_tm._value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}
		T current_value = ReadT();
		return (*WriteT() = current_value - ReadTmT( rhs_tm ));
	}

	T& operator*=(const tm_T &rhs_tm) {
		if( !__trap_access ) {
			_value *= rhs_tm._value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}
		T current_value = ReadT();
		return (*WriteT() = current_value * ReadTmT( rhs_tm ));
	}

	T& operator/=(const tm_T &rhs_tm) {
		if( !__trap_access ) {
			_value /= rhs_tm._value;
			if (__in_transact) {
				++(__meta->tag_version);
			}
			return *((T*)(&_value));
		}

		T current_value = ReadT();
		return (*WriteT() = current_value / ReadTmT( rhs_tm ));
	}


	/**************************************************************************
	 *		Lock Assignment
	 **************************************************************************/

	void assign_lock( ver_t* new_meta ){
		__meta = (ver_t *)new_meta;
	}
};

/**************************************************************************
 *		TYPE REDEFINITIONS
 **************************************************************************/

typedef tm_type<char> 			tm_char;
typedef tm_type<short> 			tm_short;
typedef tm_type<int> 			tm_int;
typedef tm_type<long> 			tm_long;
typedef tm_type<long long> 		tm_llong;

typedef tm_type<unsigned char> 		tm_uchar;
typedef tm_type<unsigned short> 	tm_ushort;
typedef tm_type<unsigned int> 		tm_uint;
typedef tm_type<unsigned long> 		tm_ulong;
typedef tm_type<unsigned long long> 	tm_ullong;

typedef tm_type<float> 			tm_float;
typedef tm_type<double> 		tm_double;

#endif	// TM_H_
