#include <sys/mman.h>
#include <string.h>
#include "StatClient.h"

CStatClient::CStatClient(void)
{
	lastSN = 0;
}

CStatClient::~CStatClient(void)
{
}

int64_t CStatClient::ReadCounterValue(iterator i, unsigned int cat)
{
	if(cat > nfmap())
		return 0;
	int64_t *ptr = (int64_t *)(fmap[cat] + i->offset());
	if(cat==SC_10M || cat==SCC_10M)
		return ptr[0] >> 10;
	return ptr[0];
}

int64_t CStatClient::ReadSampleAverage(iterator i, unsigned int cat)
{
	if(cat > nfmap() || i->issample()==0)
		return 0;

	int64_t *ptr = (int64_t *)(fmap[cat] + i->offset());

	return ptr[1] ? ptr[0]/ptr[1] : 0;
}

int64_t CStatClient::ReadSampleCounter(iterator i, unsigned int cat, unsigned int n)
{
	if(cat > nfmap() || i->issample()==0 || n > i->count())
		return 0;
	int64_t *ptr = (int64_t *)(fmap[cat] + i->offset());
	if(cat==SC_10M || cat==SCC_10M)
		return ptr[n+1] >> 10;
	return ptr[n+1];
}

int CStatClient::InitStatInfo(const char *name, const char *fn)
{
	int ret = CStatManager::InitStatInfo(name, fn, 1);
	if(ret < 0)
		return ret;

	mprotect(fmap[SC_CUR], header->datasize, PROT_READ);
	mprotect(fmap[SC_10S], header->datasize, PROT_READ);
	mprotect(fmap[SC_10M], header->datasize, PROT_READ);
	mprotect(fmap[SC_ALL], header->datasize, PROT_READ);
	fmap[SCC_10S] = (char *)mmap(0, header->datasize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	fmap[SCC_10M] = (char *)mmap(0, header->datasize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	fmap[SCC_ALL] = (char *)mmap(0, header->datasize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	return ret;
}

int CStatClient::CheckPoint(void)
{
	memcpy(fmap[SCC_10S], fmap[SC_10S], header->datasize);
	memcpy(fmap[SCC_10M], fmap[SC_10M], header->datasize);
	memcpy(fmap[SCC_ALL], fmap[SC_ALL], header->datasize);
	return 0;
}

