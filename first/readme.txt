
	1. Example applications

For now I only have 4 example applications.
These are in order of complexity: test_c, test_m, qs and bench which includes
linked_list and hashtable; They can all be found in the "apps" directory.

They all have targets in the main Makefile; (make test_c, etc.).
For cleaning use: make clean_all

	2. "tm_type" and "tw_type"

tm_type is used for wrapping global data that is shared between threads executing
different transactions. It essentially protects that data making sure that it's being accessed
in a consistent manner.

tw_type is used with local data for automatic rollback in case of an abort. It doesn't provide
any protection against concurrent accesses. It is useful when you have some local data that gets
modified during the transaction. The alternative is to initialize that data yourself at the
beginning of the transaction, whenever that's possible, ( I recommend the last variant because it's
cheaper).

You have to include src/tm.h for the use of tm_type and tw_type.

tm_type< original_type >  shared_variable;
tw_type< original_type >  private_variable;

As the original_type, only basic types and pointers can be used. No structures or classes allowed.
For pointers, only the memory containing the pointer is protected, not the memory it points to.

	Implicit conversion (to the original type) operator is overloaded in order to track when read
accesses take place, while also making it possible for the variables declared as tm_types to behave
as their original type would.
	Implicit conversion takes place when passing the variable as a parameter of a function that expects
the original type, or in expressions when the type can be determined from the context and is compatible 
with the original type.

IMPORTANT:
	When dealing with constructs like "if( tm_pointer )  ...", you always need to convert the pointer
to the original type (because implicit converion isn't triggered in this case), like this:
"if( ((pointer_type)tm_pointer) )  ...".

	Assignment operators (=, +=, -=, *=, /=, ++, --) are overloaded in order to track when write accesses
 take place.

tm_type and tw_type loose all their properties when used outside of a transaction, they become dumb
wrappers for the original type.

If support for "tw_type" isn't needed, for better performance, you can comment the definition of
 ENABLE_EXTRAS in the tm_general.h file.

	3. Transactions

You usualy replace critical sections with transactions.
BEGIN_TRANSACTION() and COMMIT_TRANSACTION() need to be used inside the same scope, and cannot be nested.

	4. Memory management

Considering that tm_type and tw_type are classes is better to allocate/deallocate objects of this type using new/delete.
Also direct memory access functions like memcpy, or memset should not be used for tm_type/tw_type objects. Instead, the
 assignment operator should be used.

All the shared objects that are dynamicaly allocated inside a transaction need to be instantiated from classes that
 inherit from the "tm_obj" class. For deleting an object inside a transaction, you have to use tm_delete( void* ptr )
instead of the delete operator. Check apps/test/test_m.cc for an example.

	5. Thread management

The library can be used with a maximum number of 31 threads.

You can manage your own thread pool or you can use the next macros:
	CREATE_TM_THREADS( N_TH );
		- creates a pool of N_TH threads
    PARALLEL_EXECUTE( N_TH, run_work, arg );
		- runs on each thread of the pool (including the main thread)
		 the function run_work, passing it the argument arg
		- it has a barrier which waits for all the threads to complete the function,
		 before returning
		- can be used several times from different functions, after the pool is created
	DESTROY_TM_THREADS( N_TH );
		- destroys the pool of N_TH threads

run_work needs to have this prototype:	int run_work( void* arg, int id ) .
It is also mandatory that run_work returns always 0;

	6. Concurrency control strategy

The concurrency control strategy can be set using:
set_version( int conflict_detection, int conflict_resolution );, where
conflict_detection can take the following values :
	- 1 - Fully Pessimistic
	- 2 - Partially Read Optimistic
	- 3 - Read Optimistic
	- 4 - Fully Optimistic
conflict_resolution can take the following values :
	- 1 - WAITFOR_READERS
	- 2 - ABORT_READERS

By default the library is in the (1,1) configuration.

	7. Statistics

Statistics can be obtained using the following library calls:

tstats_t tid_stats = stats_get( tid )		// returns the stats from the 'tid' thread
tstats_t tot_stats = stats_get_total();		// returns the aggregated stats from all the threads

stats_print( stdout, tot_stats );			// prints the stats
stats_print_v( stdout, tot_stats );			// prints the stats prefixed with a label for each field

The explanation of each field follows:

N_CMT		no of commits
T_CMT		time (in seconds) spent executing transactions that commited

N_DLK_RAW	no of deadlocks experienced when waiting at a RAW conflict
T_DLK_RAW	time (in seconds) spent executing transactions that deadlocked when waiting at a RAW conflict
T_WT_RAW	time (in seconds) spent waiting at a RAW conflict

N_DLK_WAR	no of deadlocks experienced when waiting at a WAR conflict
T_DLK_WAR	time (in seconds) spent executing transactions that deadlocked when waiting at a WAR conflict
T_WT_WAR	time (in seconds) spent waiting at a WAR conflict

N_DLK_WAW	no of deadlocks experienced when waiting at a WAW conflict
T_DLK_WAW	time (in seconds) spent executing transactions that deadlocked when waiting at a WAW conflict
T_WT_WAW	time (in seconds) spent waiting at a WAW conflict

N_INV		no of invalidations
T_INV		time (in seconds) spent executing transactions that were invalidated

N_ABR		no of aborts ( = N_DLK_RAW + N_DLK_WAR + N_DLK_WAW + N_INV )
T_ABR		time (in seconds) spent executing transactions that were aborted
				( = T_DLK_RAW + T_DLK_WAR + T_DLK_WAW + T_INV )

WrRatio		percentage of write accesses


All the time statistics are non-inclusive, that is if transaction waited for a while and then commited,
the time it waited is not included in the T_CMT stats, but only in the corresponding T_WT_XXX stats.


If time statistics aren't needed, for better performance, you should comment the definition of
 TIMINGS in the src/utils/tm_stats.h file.





