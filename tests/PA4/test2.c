#include <stdio.h>
#include <stdlib.h>
#include "memory.h"

struct List {
	int *arr;
	struct List *next;
};

void __attribute__((noinline)) bar(int *arr, int offset) {
	arr[offset] = 0;
}

void __attribute__((noinline)) foo(struct List *node, int offset) {
	int *arr = node->arr; // arr = og_arr[offset + 1]
	arr[offset] = 20; // og_arr[offset + 1 + offset] = 20
	bar(&arr[offset+8], offset); // of_arr[offset + 1 + offset + 8] = 0
}

int main(int argc, const char *argv[])
{
	if (argc != 3) {
		printf("Usage:: <size> <offset>\n");
		return 0;
	}
	int size = readArgv(argv, 1); // 20
	int offset = readArgv(argv, 2); // idk
	int a[size]; // a[20]
	struct List node;
	node.arr = &a[offset+1]; // a[offset + 1]
	foo(&node, offset); // foo(arr[offset + 1], offset)
	return 0;
}
// 20 
// variable
// 2 * offset + 1 <= 20
// 2 * offset + 9 <= 20

// offset <= 5