#ifndef BITMAP_C_HASH_H__
#define BITMAP_C_HASH_H__

#include <stdint.h>

//hash function for consistent hash
//the algorithm is the same as new_hash, but use another initial value
uint32_t chash(const char *data, int len);

#endif
