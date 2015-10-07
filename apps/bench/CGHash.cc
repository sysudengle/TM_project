#include "CGHash.h"
#include <iostream>
using std::cout;

// The main hash methods are very straightforward; choose a bucket, then use
// the LinkedList code on that bucket
bool CGHash::lookup(int val) const
{
    FGLNode* head = bucket[val % N_CGHBUCKETS].prehead;
    head->acquire();
    FGLNode* curr = head->next;
    while (curr) {
        if (curr->val > val) {
            head->release();
            return false;
        }
        else if (curr->val == val) {
            head->release();
            return true;
        }
        else
            curr = curr->next;
    }
    head->release();
    return false;
}

void CGHash::insert(int val)
{
    // lock the bucket
    FGLNode* head = bucket[val % N_CGHBUCKETS].prehead;
    head->acquire();

    // search the bucket
    FGLNode* prev = head;
    FGLNode* curr = head->next;
    while (curr) {
        if (curr->val >= val)
            break;
        prev = curr;
        curr = curr->next;
    }

    // insert, unlock, and return
    if (!curr || curr->val > val)
        prev->next = new FGLNode(val, curr);
    head->release();
}

void CGHash::remove(int val)
{
    // lock the bucket
    FGLNode* head = bucket[val % N_CGHBUCKETS].prehead;
    head->acquire();

    // search the bucket
    FGLNode* prev = head;
    FGLNode* curr = head->next;
    while (curr) {
        // item is in list
        if (curr->val == val) {
            prev->next = curr->next;
            delete curr;
            break;
        }
        // item is not in list
        if (curr->val > val)
            break;
        prev = curr;
        curr = curr->next;
    }
    head->release();
}

// during a sanity check, we want to make sure that every element in a bucket
// actually hashes to that bucket; we do it by passing this method to the
// extendedSanityCheck for the bucket
bool verify_cghash_function(unsigned long val, unsigned long bucket)
{
    return ((val % N_CGHBUCKETS) == bucket);
}

// call extendedSanityCheck() on each bucket (just use the FGL version)
bool CGHash::isSane() const
{
    for (int i = 0; i < N_CGHBUCKETS; i++) {
        if (!bucket[i].extendedSanityCheck(verify_cghash_function, i)) {
            return false;
        }
    }
    return true;
}

// print the hash (just use the FGL version)
void CGHash::print() const
{
    cout << "::CGHash::" << endl;
    for (int i = 0; i < N_CGHBUCKETS; i++) {
        bucket[i].print();
    }
    cout << "::End::" << endl;
}
