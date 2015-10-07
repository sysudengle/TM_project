#ifndef __CGHASH_H__
#define __CGHASH_H__

#include "IntSet.h"
#include "FGL.h"

static const int N_CGHBUCKETS = 256;

// the Hash class is an array of N_CGHBUCKETS FGL lists
class CGHash : public IntSet
{
    FGL bucket[N_CGHBUCKETS];
  public:
    virtual void insert(int val);
    virtual bool lookup(int val) const;
    virtual void remove(int val);
    virtual bool isSane() const;
    virtual void print() const;
};

#endif // __CGHASH_H__
