#include <stdio.h>
#include <stdlib.h>
#include "support.h"

/*
 * EXPECTED CHECKS
 * needed check before:   %arrayidx1 = getelementptr inbounds i32, i32* %3, i64 0
 * needed check before:   %arrayidx5 = getelementptr inbounds i32, i32* %7, i64 0
 * needed check before:   %arrayidx6 = getelementptr inbounds i32, i32* %8, i64 0
 */

int *bar(int size)
{
	return NULL;
}

void foo (int *arr)
{
	int *ptr = mymalloc(4);

	ptr[0] = 100;
	ptr = arr;
	ptr[0] = 100;
	if (ptr == NULL) {
		ptr = mymalloc(4);
		ptr[0] = 100;
	}
	else {
		ptr = bar(4);
		ptr[0] = 100;
	}
	ptr[0] = 100;
}

int main()
{
	foo(NULL);
	return 0;
}
