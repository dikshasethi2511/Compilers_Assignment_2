#include "support.h"
#include <stdio.h>

void foo(int *arr) {
  int *ptr = mymalloc(4);

  ptr[0] = 100;
label:
  if (ptr == NULL) {
    printf("before error \n");

    ptr[0] = 100;
    printf("after error \n");

  } else {
    ptr[0] = 100;
  }
  ptr[0] = 100;
  ptr = arr;
  goto label;
}

int main() {
  foo(NULL);
  return 0;
}
