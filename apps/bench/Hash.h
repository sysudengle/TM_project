#ifndef __HASH_H__
#define __HASH_H__

#include <iostream>

#include "IntSet.h"
#include "LinkedList.h"

namespace bench
{

    static const int N_BUCKETS = 256;

    // the Hash class is an array of N_BUCKETS LinkedLists
    template
    <
        class ListType = LinkedList
    >
    class HashTable : public IntSet
    {
      private:

        /**
         *  Templated type defines what kind of list we'll use at each bucket.
         */
        ListType bucket[N_BUCKETS];


        /**
         *  during a sanity check, we want to make sure that every element in a
         *  bucket actually hashes to that bucket; we do it by passing this
         *  method to the extendedSanityCheck for the bucket.
         */
        static bool verify_hash_function(unsigned long val,
                                         unsigned long bucket)
        {
            return ((val % N_BUCKETS) == bucket);
        }


      public:

        virtual void insert(int val)
        {
            bucket[val % N_BUCKETS].insert(val);
        }


        virtual bool lookup(int val) const
        {
            return bucket[val % N_BUCKETS].lookup(val);
        }


        virtual void remove(int val)
        {
            bucket[val % N_BUCKETS].remove(val);
        }


        virtual bool isSane() const
        {
            for (int i = 0; i < N_BUCKETS; i++)
            {
                if (!bucket[i].extendedSanityCheck(verify_hash_function, i))
                {
                    return false;
                }
            }

            return true;
        }


        virtual void print() const
        {
            std::cout << "::Hash::" << std::endl;

            for (int i = 0; i < N_BUCKETS; i++)
            {
                bucket[i].print();
            }

            std::cout << "::End::" << std::endl;
        }
    };

    typedef HashTable<> Hash;

} // namespace bench

#endif // __HASH_H__
