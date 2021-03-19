/*
 * =====================================================================================
 *
 *       Filename:  chash.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef BITMAP_C_HASH_H__
#define BITMAP_C_HASH_H__

#include <stdint.h>

//hash function for consistent hash
//the algorithm is the same as new_hash, but use another initial value
uint32_t chash(const char *data, int len);

#endif
