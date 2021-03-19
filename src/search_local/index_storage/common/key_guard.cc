/*
 * =====================================================================================
 *
 *       Filename:  key_guard.cc
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
#include <string.h>
#include "mem_check.h"

#include "key_guard.h"

static int keycmp_1(const char *a, const char *b, int ks)
{
	return *a - *b;
}

static int keycmp_2(const char *a, const char *b, int ks)
{
	return *(uint16_t *)a - *(uint16_t *)b;
}

static int keycmp_4(const char *a, const char *b, int ks)
{
	return *(uint32_t *)a - *(uint32_t *)b;
}

static int keycmp_n(const char *a, const char *b, int ks)
{
	return memcmp(a, b, ks);
}

static int keycmp_b(const char *a, const char *b, int ks)
{
	return *a != *b ? 1 : memcmp(a + 1, b + 1, *a);
}

KeyGuard::KeyGuard(int ks, int exp)
{
	keySize = ks;
	timeout = exp;
	switch (keySize)
	{
	case 0:
		keycmp = keycmp_b;
	case 1:
		keycmp = keycmp_1;
	case 2:
		keycmp = keycmp_2;
	case 4:
		keycmp = keycmp_4;
	default:
		keycmp = keycmp_n;
	}
	for (int i = 0; i < HASHBASE; i++)
	{
		INIT_LIST_HEAD(hashList + i);
	}
	INIT_LIST_HEAD(&lru);
};

KeyGuard::~KeyGuard()
{
}

void KeyGuard::try_expire(void)
{
	unsigned int deadline = time(NULL) - timeout;
	while (!list_empty(&lru))
	{
		struct keyslot *k = list_entry(lru.next, struct keyslot, tlist);
		if (k->timestamp >= deadline)
			break;
		list_del(&k->hlist);
		list_del(&k->tlist);
		FREE(k);
	}
}

void KeyGuard::add_key(unsigned long barHash, const char *ptrKey)
{
	struct list_head *h = hashList + ((unsigned long)barHash) % HASHBASE;
	struct keyslot *k;
	list_for_each_entry(k, h, hlist)
	{
		if (keycmp(ptrKey, k->data, keySize) == 0)
		{
			k->timestamp = time(NULL);
			_list_move_tail(&k->tlist, &lru);
			return;
		}
	}

	int size = keySize;
	if (size == 0)
		size = 1 + *(unsigned char *)ptrKey;
	k = (struct keyslot *)MALLOC(offsetof(struct keyslot, data) + size);
	memcpy(k->data, ptrKey, size);
	k->timestamp = time(NULL);
	list_add(&k->hlist, h);
	list_add(&k->tlist, &lru);
}

bool KeyGuard::in_set(unsigned long barHash, const char *ptrKey)
{
	struct list_head *h = hashList + ((unsigned long)barHash) % HASHBASE;
	struct keyslot *k;
	list_for_each_entry(k, h, hlist)
	{
		if (keycmp(ptrKey, k->data, keySize) == 0)
			return true;
	}

	return false;
}
