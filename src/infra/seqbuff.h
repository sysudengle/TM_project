#ifndef seqbuff_H
#define	seqbuff_H

/**
 * How the sequential buffer (Linked List) works:
 *
 * 1. Every seqbuff_t is a linked list of sequential buffers, each seq.buff
 *    node stores 16MB of data.
 *
 * 2. Each seq buf node consists of:
 * 	  put_pos: where to put new entry
 * 	  get_pos: where to get the current head entry
 * 	  last_pos: where does the seq.buf end in memory
 * 	  next:	points to the next sbuff node in the list
 * 	  data: the actual data
 */

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdlib.h>

#define likely(x)       	__builtin_expect(!!(x), 1)
#define unlikely(x)     	__builtin_expect(!!(x), 0)

/**
 *  limit set sizes to 32KB each, should really be <4KB in order to prevent
 *  L1 overflow related aborts.
 */

#define SBUFF_SIZE			(1024 * 32 *4)

typedef struct _seqbuff_t {
    char* 	put_pos;
    char* 	get_pos;
    char* 	last_pos;
    char 	data[0];
} seqbuff_t;

inline __attribute__((always_inline))
seqbuff_t* add_node( size_t size ) {
	seqbuff_t* new_node = (seqbuff_t*) malloc( sizeof( seqbuff_t ) + (size) );
    new_node->put_pos = &new_node->data[0];
    new_node->get_pos = &new_node->data[0];
    new_node->last_pos = &new_node->data[size];

    return new_node;
}

/**
 * Initialize a sequential buffer of size SBUFF_SIZE which defaults to 16MB
 */
inline __attribute__((always_inline))
seqbuff_t* seqbuff_init( size_t sbuff_size = SBUFF_SIZE  ) {
	return add_node( sbuff_size );
}

/**
 * Create and initialize a new sequential buffer
 */
inline __attribute__((always_inline))
seqbuff_t* new_seqbuff(size_t sbuff_size = SBUFF_SIZE ) {
    seqbuff_t* new_seqb = seqbuff_init( sbuff_size );
    return new_seqb;
}

inline __attribute__((always_inline))
void reset_seqbuff( seqbuff_t* seqb ) {
    seqb->get_pos = &seqb->data[0];
    seqb->put_pos = &seqb->data[0];
}

inline __attribute__((always_inline))
void free_seqbuff( seqbuff_t* seqb ) {
    free( seqb );
}

inline __attribute__((always_inline))
char* seqbuff_put_ptr( seqbuff_t* seqb, size_t _sz, size_t sbuff_size = SBUFF_SIZE ) {
    char* result;

    tm_assert(seqb->put_pos + _sz <= seqb -> last_pos); // SeqBuf Overflow
    result = seqb->put_pos;
    seqb->put_pos += _sz;

    return result;
}

#define end_of_sb( sb )				( (sb)->get_pos == (sb)->put_pos )
#define inc_sb( sb, _sz)			sb->get_pos += _sz
#define has_next_node( sb )			( !end_of_sb(sb) )


inline __attribute__((always_inline))
char* seqbuff_get_ptr( seqbuff_t* seqb, size_t _sz ) {
    char* result = NULL;

    if (has_next_node(seqb)) {
    	result = seqb->get_pos;
    	inc_sb(seqb, _sz);
    }

	return result;
}

#define seqbuff_read_ptr( sb )	( has_next_node( sb ) ? (sb)->get_pos : NULL )

#define _seqbuff_for_each( entry, f_seqb, entry_t, entry_sz )				\
		entry_t*    entry;													\
		for( entry = (entry_t*)(f_seqb->get_pos); 							\
			 (char *)entry < f_seqb->put_pos; 										\
			 entry = (entry_t *)((char *)entry + entry_sz ))

#define  seqbuff_for_each( entry, f_seqb, entry_t, f_offset )						\
	_seqbuff_for_each( entry, f_seqb, entry_t, sizeof(entry_t)+(f_offset) )

#define  seqbuff_pass_by( entry, f_seqb, entry_t, f_offset )						\
	_seqbuff_for_each( entry, f_seqb, entry_t, sizeof(entry_t)+(f_offset) )


#ifdef	__cplusplus
}
#endif

#endif	/* seqbuff_H */

