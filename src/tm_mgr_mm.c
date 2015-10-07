#include "tm_mgr_mm.h"
#include "tm_mgr.h"


/**********************************************************************
		MEMMAG
**********************************************************************/

void* mgr_on_new( size_t size )
{
	void* ptr = malloc(size);
	memset( ptr, 0, size );

	return ptr;
}

void mgr_on_delete( void* ptr )
{
	free(ptr);
}

void tm_delete( void* ptr )
{
	free(ptr);
}
