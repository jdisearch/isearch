/*
 * =====================================================================================
 *
 *       Filename:  key_guard.h
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
#include "timer_list.h"
#include "list.h"

class KeyGuard
{
public:
	KeyGuard(int keysize, int timeout);
	~KeyGuard();

	void add_key(unsigned long barHash, const char *ptrKey);
	bool in_set(unsigned long barHash, const char *ptrKey);

private:
	void try_expire(void);

private:
	enum
	{
		HASHBASE = 65536
	};
	int keySize;
	int timeout;
	struct list_head hashList[HASHBASE];
	struct list_head lru;
	int (*keycmp)(const char *a, const char *b, int ks);

	struct keyslot
	{
		struct list_head hlist;
		struct list_head tlist;
		unsigned int timestamp;
		char data[0];
	} __attribute__((__aligned__(1)));
};
