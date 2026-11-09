#pragma once
#include <stdlib.h>
// Force-included: newlib's itoa() is not part of glibc.
char* itoa(int value, char* str, int base);
// Furi's malloc/calloc/realloc return zeroed memory (pvPortMalloc ends in memset(.., 0, ..));
// glibc's do not, so give firmware code the same guarantee.
void* mayhem_realloc(void* ptr, size_t size);
#define malloc(n) calloc(1, (n))
#define realloc(p, n) mayhem_realloc((p), (n))
