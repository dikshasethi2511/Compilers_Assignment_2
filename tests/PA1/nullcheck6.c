#include <stdio.h>
#include "support.h"

int bar (int a)
{
	return a;
}

void foo ()
{
	int (*fnptr)(int) = bar;
	fnptr(1);
	if (fnptr != NULL) {
		fnptr = NULL;
                printf("before error \n");

                fnptr(1);
                printf("after error \n");

                fnptr = bar;
	}
	else {
		fnptr(1);
	}
	fnptr(1);
}

int main()
{
	foo();
	return 0;
}
