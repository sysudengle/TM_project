#include <stdio.h>
#include <stdlib.h>

#include "../../src/tm.h"
#include "../../src/utils/hrtime.h"

typedef tm_type<int> tm_int;
typedef tw_type<int> tw_int;

tm_int b = 8;

int main( int argc, char** argv )
{
	tw_int a = 2;

	printf( "a = %d    b = %d\n", (int)a, (int)b );

	BEGIN_TRANSACTION();
	a = b;
	++a;
	++b;

	COMMIT_TRANSACTION();

	printf( "a = %d    b = %d\n", (int)a, (int)b );

	return 0;
}


