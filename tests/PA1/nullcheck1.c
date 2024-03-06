#include "support.h"
#include <stdio.h>

void foo(int *arr) {
  int *ptr = mymalloc(4);

  ptr[0] = 100;
  ptr = arr;
  // if (ptr == NULL) {
  //   exit(0);
  // }
  // TODO: Fix test back to original
  ptr[0] = 100;
  printf('Here2 : ');

  //   if (ptr == NULL) {
  //     ptr = mymalloc(4);
  //     ptr[0] = 100;
  // 	}
  // 	else {
  // 		ptr[0] = 100;
  // 	}
  // 	ptr[0] = 100;
}

int main() {
  int *ptr = mymalloc(4);
  foo(NULL);
  return 0;
}
