#include <iostream>
#include <stdio.h>

#include "LinkedList.h"


using namespace bench;

// constructor just makes a sentinel for the data structure
LinkedList::LinkedList() : sentinel(new LLNode())
{ }

// simple sanity check:  make sure all elements of the list are in sorted order
bool LinkedList::isSane(void) const
{
    bool sane = false;

    BEGIN_TRANSACTION();

    sane = true;
    LLNode* prev = sentinel;
    LLNode* curr = prev->m_next;
    
    while (curr != NULL)
    {
        if( prev->m_val >= curr->m_val )
        {
            sane = false;
            break;
        }

        prev = curr;
        curr = curr->m_next;
    }

    COMMIT_TRANSACTION();

    return sane;
}

// extended sanity check, does the same as the above method, but also calls v()
// on every item in the list
bool LinkedList::extendedSanityCheck(verifier v, unsigned long v_param) const
{
    bool sane = false;

    BEGIN_TRANSACTION();

    sane = true;
    LLNode* prev = sentinel;
    LLNode* curr = prev->m_next;

    while (curr != NULL)
    {
        if( !v(curr->m_val, v_param) || (prev->m_val >= curr->m_val) )
        {
            sane = false;
            break;
        }

        prev = curr;
        curr = prev->m_next;
    }

    COMMIT_TRANSACTION();

    return sane;
}

// insert method; find the right place in the list, add val so that it is in
// sorted order; if val is already in the list, exit without inserting
void LinkedList::insert(int val)
{
	LLNode* to_ins = NULL;
	LLNode* prev;
    LLNode* curr  = NULL;
	
    BEGIN_TRANSACTION();

    // traverse the list to find the insertion point
    prev = sentinel;
    curr = prev->m_next;
	to_ins = NULL;


    while (curr != NULL)
    {
        if (curr->m_val >= val)
            break;

        prev = curr;
        curr = prev->m_next;
    }

	
    // now insert new_node between prev and curr
    if (!curr || (curr->m_val > val))
	{
		to_ins = new LLNode(val, curr);
		prev->m_next = to_ins;

	}

    COMMIT_TRANSACTION();

}


// search function
bool LinkedList::lookup(int val) const
{
    bool found = false;

    BEGIN_TRANSACTION();

    LLNode* curr = ((LLNode*)sentinel)->m_next;

    while (curr != NULL)
    {
        if (curr->m_val >= val)
            break;

        curr = curr->m_next;
    }

    found = ((curr != NULL) && (curr->m_val == val));

    COMMIT_TRANSACTION();

    return found;
}


// remove a node if its value == val
void LinkedList::remove(int val)
{
    LLNode* to_next = NULL;
	LLNode* prev;
	LLNode* curr  = NULL;
	

    BEGIN_TRANSACTION();

    // find the node whose val matches the request
    prev = sentinel;
    curr = prev->m_next;
	to_next = NULL;

    while (curr != NULL)
    {
        // if we find the node, disconnect it and end the search
        if (curr->m_val == val)
        {
			to_next = curr->m_next;
			prev->m_next = to_next;
			
            // delete curr...
            tm_delete( curr );
			
			break;
        }
        else if (curr->m_val > val)
        {
            // this means the search failed
            break;
        }

        prev = curr;
        curr = prev->m_next;
    }

    COMMIT_TRANSACTION();
}


// print the list
void LinkedList::print() const
{
    BEGIN_TRANSACTION();

    LLNode* curr = ((LLNode*)sentinel)->m_next;

    if (curr != NULL)
    {
        std::cout << "list :: ";

        while (curr != NULL)
        {
            std::cout << curr->m_val << "->";
            curr = curr->m_next;
        }

        std::cout << "NULL" << std::endl;
    }

    COMMIT_TRANSACTION();
}

