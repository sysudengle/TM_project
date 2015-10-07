#ifndef __FGL_H__
#define __FGL_H__

#include "atomic_ops.h"
#include "IntSet.h"

// each node in the list has a private lock, a value, and a next pointer
class FGLNode
{
  public:
    int val;
    FGLNode* next;
    r_tatas_lock_t lock;

    FGLNode(int val, FGLNode* next) : val(val), next(next), lock(0) { }

    // wrap the lock actions
    inline void acquire() { r_tatas_acquire(&lock); }
    inline void release() { r_tatas_release(&lock); }

    //void* operator new(size_t size) { return stm::tx_alloc(size); }
    //void operator delete(void* p) { stm::tx_free(p); }
};

// We construct other data structures from the Linked List; in order to do
// their sanity checks correctly, we might need to pass in a validation
// function of this type
typedef bool (*verifier)(unsigned long, unsigned long);

// the FGL benchmark just implements the four default functions lookup, insert,
// remove, and isSane.
class FGL : public IntSet
{
    // (pass by reference) search the list for val
    virtual bool search(FGLNode *&left, FGLNode *&right, int val) const;

  public:
    // sentinel for the head of the list
    FGLNode* prehead;

    FGL() { prehead = new FGLNode(-1, 0); }

    virtual bool lookup(int val) const;
    virtual void insert(int val);
    virtual void remove(int val);
    virtual bool isSane() const;
    virtual void print() const;

    // make sure the list is in sorted order and for each node x,
    // v(x, verifier_param) is true
    virtual bool extendedSanityCheck(verifier v, unsigned long param) const;
};

#endif // __FGL_H__
