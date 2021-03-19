/*
 * =====================================================================================
 *
 *       Filename:  stat_client.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <sys/mman.h>
#include <string.h>
#include "stat_client.h"

StatClient::StatClient(void)
{
	lastSN = 0;
}

StatClient::~StatClient(void)
{
}

int64_t StatClient::read_counter_value(iterator i, unsigned int cat)
{
	if (cat > nfmap())
		return 0;
	int64_t *ptr = (int64_t *)(fmap[cat] + i->offset());
	if (cat == SC_10M || cat == SCC_10M)
		return ptr[0] >> 10;
	return ptr[0];
}

int64_t StatClient::read_sample_average(iterator i, unsigned int cat)
{
	if (cat > nfmap() || i->is_sample() == 0)
		return 0;

	int64_t *ptr = (int64_t *)(fmap[cat] + i->offset());

	return ptr[1] ? ptr[0] / ptr[1] : 0;
}

int64_t StatClient::read_sample_counter(iterator i, unsigned int cat, unsigned int n)
{
	if (cat > nfmap() || i->is_sample() == 0 || n > i->count())
		return 0;
	int64_t *ptr = (int64_t *)(fmap[cat] + i->offset());
	if (cat == SC_10M || cat == SCC_10M)
		return ptr[n + 1] >> 10;
	return ptr[n + 1];
}

int StatClient::init_stat_info(const char *name, const char *fn)
{
	int ret = StatManager::init_stat_info(name, fn, 1);
	if (ret < 0)
		return ret;

	mprotect(fmap[SC_CUR], header->datasize, PROT_READ);
	mprotect(fmap[SC_10S], header->datasize, PROT_READ);
	mprotect(fmap[SC_10M], header->datasize, PROT_READ);
	mprotect(fmap[SC_ALL], header->datasize, PROT_READ);
	fmap[SCC_10S] = (char *)mmap(0, header->datasize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	fmap[SCC_10M] = (char *)mmap(0, header->datasize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	fmap[SCC_ALL] = (char *)mmap(0, header->datasize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return ret;
}

int StatClient::check_point(void)
{
	memcpy(fmap[SCC_10S], fmap[SC_10S], header->datasize);
	memcpy(fmap[SCC_10M], fmap[SC_10M], header->datasize);
	memcpy(fmap[SCC_ALL], fmap[SC_ALL], header->datasize);
	return 0;
}
