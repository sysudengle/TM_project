#ifndef __BENCH_LINKED_LIST_H__
#define __BENCH_LINKED_LIST_H__

#include "IntSet.h"

extern int aaa;

namespace bench
{
    // LLNode is a single node in a sorted linked list
    class LLNode;
    typedef tm_type< LLNode*>   tm_p_LLNode;

    class LLNode : public tm_obj
    {
      public:
		int m_val;
		tm_p_LLNode m_next;
		
        // ctor
        LLNode(int val = -1) : m_val(val)
        { m_next = NULL; }

        LLNode(int val, LLNode* next) : m_val(val)
        { m_next = next; }

    };


    // We construct other data structures from the Linked List; in order to
    // do their sanity checks correctly, we might need to pass in a
    // validation function of this type
    typedef bool (*verifier)(unsigned long, unsigned long);


    // Set of LLNodes represented as a linked list in sorted order
    class LinkedList : public IntSet
    {
      private:

        tm_p_LLNode sentinel;

      public:

        LinkedList();

        // insert a node if it doesn't already exist
        virtual void insert(int val);

        // true iff val is in the data structure
        virtual bool lookup(int val) const;

        // remove a node if its value = val
        virtual void remove(int val);

        // make sure the list is in sorted order
        virtual bool isSane() const;

        // print the whole list (assumes isolation)
        virtual void print() const;

        // make sure the list is in sorted order and for each node x,
        // v(x, verifier_param) is true
        virtual bool extendedSanityCheck(verifier v,
                                         unsigned long param) const;
    };

} // namespace bench
#endif // __BENCH_LINKED_LIST_H__
