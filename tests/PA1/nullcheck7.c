#include <stdio.h>
#include "support.h"

int *gvar = NULL;

void foo (int *arr)
{
	int *ptr = mymalloc(4);

	ptr[0] = 100;
	ptr = arr;
        printf("before error \n");

        ptr[0] = 100;
        printf("after error \n");

        if (ptr == NULL) {
		ptr = mymalloc(4);
		ptr[0] = 100;
	}
	else {
		ptr = gvar;
		ptr[0] = 100;
	}
	ptr[0] = 100;
}

int main()
{
	foo(NULL);
	return 0;
}
