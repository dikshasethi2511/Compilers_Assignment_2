#include "support.h"
#include "memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void checkTypeInv(void *Src, unsigned long long DstType) {}

void checkSizeInv(void *Dst, unsigned DstSize) {
  unsigned DstOrigSize = GetSize(Dst);

  if (DstOrigSize < DstSize) {
    printf("Invalid obj size: min_required:%x current:%x\n", (unsigned)DstSize,
           DstOrigSize);
    exit(0);
  }
}

void checkSizeAndTypeInv(void *Src, unsigned long long DstType,
                         unsigned DstSize) {
  checkTypeInv(Src, DstType);
  checkSizeInv(Src, DstSize);
}

void *mycast(void *Ptr, unsigned long long Bitmap, unsigned Size) {
  // checkSizeInv(Ptr, Size);
  SetType(Ptr, Bitmap);
  return Ptr;
}

void IsSafeToEscape(void *Base, void *Ptr) {}

void BoundsCheck(void *Base, void *Ptr, size_t AccessSize) {
  size_t Size = GetSize(Base);

  if ((char *)Base <= (char *)Ptr &&
      (char *)Ptr + AccessSize <= (char *)Base + Size) {
  } else {
    // printf("ERROR: Out of bounds access\n");
    exit(0);
  }
}

void BoundsCheckWithSize(void *RealBase, void *Ptr, size_t Size,
                         size_t AccessSize) {

  if ((char *)RealBase <= (char *)Ptr &&
      (char *)Ptr + AccessSize <= (char *)RealBase + Size) {
  } else {
    // printf("ERROR: Out of bounds access\n");
    exit(0);
  }
}

void CallingExit() {
  // From the assignmet: we can’t
  // infer that the argument arr was actually derived from the object allocated
  // at line-2. To prevent this, you need to add dynamic checks to abort the
  // program in all these cases.
  // printf(
  // "ERROR: Abort in the cases when real base address cannot be inferred\n");
  exit(0);
}

void WriteBarrier(void *Base, void *Ptr, size_t AccessSize) {
  unsigned long long Type = GetType(Base);

  // Check if the updated pointer is NULL or points to a valid object.
  if (Ptr != NULL) {
    unsigned long long PtrType = GetType(Ptr);
    if ((PtrType == 0) || (PtrType & Type)) {
      // Pointer is valid.
      return;
    }
  }

  // Assertion failed: updated value is either NULL or points to a valid object.
  printf("ERROR: Write barrier failed - Invalid pointer update detected.\n");
  exit(0);
}

void WriteBarrierWithSize(void *RealBase, void *Ptr, size_t Size,
                          size_t AccessSize, unsigned long long Type) {}
