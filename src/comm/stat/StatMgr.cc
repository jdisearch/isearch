#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memcheck.h>
#include <log.h>
#include <sys/file.h>

#include "unix_lock.h"
#include "StatMgr.h"

#if HAS_ATOMIC8
int64_t CStatItem::dummy;
#else
uint32_t CStatItemU32::dummy;
int32_t CStatItemS32::dummy;
CStatItemObject CStatItem::dummy;
#endif
int64_t CStatSampleObject::dummyCnt[2];
const TStatInfo CStatSampleObject::dummyInfo = { 0, 0, SA_SAMPLE, 0 };
CStatSampleObject CStatSample::dummy;

int64_t CStatSampleObject::count(unsigned int n)
{
	P a(this);
	if(n > info->cnt)
		return 0;
	return cnt[n];
}
int64_t CStatSampleObject::sum(void)
{
	P a(this);
	return cnt[0];
}
int64_t CStatSampleObject::average(int64_t o)
{
	P a(this);
	return cnt[1] ? cnt[0]/cnt[1] : o;
}
void CStatSampleObject::push(int64_t v)
{
	P a(this);
	cnt[0] += v;
	cnt[1] ++;
	for(unsigned int n = 0; n<info->cnt; n++)
		if(v >= info->vptr[n])
			cnt[2+n]++;

}
void CStatSampleObject::output(int64_t *v)
{
	P a(this);
	memcpy(v, cnt, (2+16)*sizeof(int64_t));
	memset(cnt, 0, (2+16)*sizeof(int64_t));
}

CStatManager::CStatManager()
{
	header = NULL;
	indexsize = 0;
	memset(fmap, 0, sizeof(fmap));
	info = NULL;
	numinfo = 0;
	lockdev = 0;
	lockino = 0;
	lockfd = -1;
}

CStatManager::~CStatManager()
{
	if(header)
	{
		for(unsigned int i=0; i<nfmap(); i++)
			if(fmap[i]) munmap(fmap[i], header->datasize);
		munmap(header, indexsize);
	}
	if(info)
		delete []info;
	if(lockfd >= 0)
		close(lockfd);
}

int CStatManager::InitStatInfo(const char *name, const char *indexfile, int isc)
{
	P __a(this);

	int fd;

	fd = open(indexfile, O_RDWR);
	if( fd < 0 )
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "cannot open index file");
		return -1;
	}

	struct stat st;
	if(fstat(fd, &st) != 0)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "fstat() failed on index file");
        close(fd);
		return -2;
	}
	lockdev = st.st_dev;
	lockino = st.st_ino;

	if(isc==1)
	{
		if(Lock("serv") < 0)
		{
			snprintf(szErrMsg, sizeof(szErrMsg), "stat data locked by other process");
			close(fd);
			return -2;
		}
	}

	indexsize = lseek(fd, 0L, SEEK_END);
	if(indexsize < sizeof(TStatHeader) + sizeof(TStatInfo))
	{
		// file too short
		close(fd);
		snprintf(szErrMsg, sizeof(szErrMsg), "index file too short");
		return -1;
	}

	header = (TStatHeader *)mmap(NULL, indexsize, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);

	if(header == MAP_FAILED)
	{
		header = NULL;
		snprintf(szErrMsg, sizeof(szErrMsg), "mmap index file failed");
		return -2;
	}

	if(header->signature != *(unsigned int *)"sTaT")
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "bad index file signature");
		return -1;
	}
	if(header->version != 1)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "bad index file version");
		return -1;
	}
	if(indexsize < header->indexsize) {
		// file too short
		snprintf(szErrMsg, sizeof(szErrMsg), "index file too short");
		return -1;
	}
	if(header->indexsize > (4<<20))
	{	// data too large
		snprintf(szErrMsg, sizeof(szErrMsg), "index size too large");
		return -1;
	}
	if(header->datasize > (1<<20))
	{	// data too large
		snprintf(szErrMsg, sizeof(szErrMsg), "data size too large");
		return -1;
	}
	if(strncmp(name, header->name, sizeof(header->name)) != 0)
	{
		// name mismatch
		snprintf(szErrMsg, sizeof(szErrMsg), "stat name mismatch");
		return -1;
	}

	if(header->numinfo==0)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "No Stat ID defined");
		return -1;
	}

	numinfo = header->numinfo;
	info = new CStatInfo[numinfo];
	TStatInfo *si = header->first();
	for(unsigned int i=0; i<numinfo; i++, si=si->next())
	{
		if(si->next() > header->last())
		{
			snprintf(szErrMsg, sizeof(szErrMsg), "index info exceed EOF");
			return -1;
		}

		if(si->off+si->datasize() > header->datasize)
		{
			snprintf(szErrMsg, sizeof(szErrMsg), "data offset exceed EOF");
			return -1;
		}

		// first 16 bytes reserved by header
		if(si->off < 16)
		{
			snprintf(szErrMsg, sizeof(szErrMsg), "data offset < 16");
			return -1;
		}

		if(si->cnt + si->cnt1 > 16)
		{
			snprintf(szErrMsg, sizeof(szErrMsg), "too many base value");
			return -1;
		}

		info[i].owner = this;
		info[i].si = si;


		idmap[si->id] = &info[i];
	}

	mprotect(header, indexsize, PROT_READ);

	char buf[strlen(indexfile)+10];
	strncpy(buf,indexfile, strlen(indexfile)+10);
	char *p = strrchr(buf, '.');
	if(p==NULL) p = buf + strlen(buf);

	 fmap[SC_CUR] = (char *)mapfile(NULL, header->datasize);
	strncpy(p, ".10s", 5); fmap[SC_10S] = (char *)mapfile(buf, header->datasize);
	strncpy(p, ".10m", 5); fmap[SC_10M] = (char *)mapfile(buf, header->datasize);
	strncpy(p, ".all", 5); fmap[SC_ALL] = (char *)mapfile(buf, header->datasize);

	at_cur(2*8) = header->createtime;
	if(isc==0)
	{
		int n;
		do {
			n = 0;
			for(unsigned int i=0; i<numinfo; i++)
			{
				if(!info[i].isexpr()) continue;
				if(info[i].expr != NULL) continue;
				unsigned int cnt = info[i].si->cnt + info[i].si->cnt1;
				if(cnt==0) continue;
				info[i].expr = InitExpr(cnt, info[i].si->vptr);
				if(info[i].expr)
				{
					expr.push_back(&info[i]);
					n++;
				}
			}
		} while(n>0);
		at_cur(3*8) = time(NULL); // startup time
		atomic_t *a0 = (atomic_t *)&at_cur(0);
		int v = atomic_add_return(1, a0);
		atomic_set(a0+1, v);
	}
	return 0;
}

static const TStatDefinition SysStatDefinition[] =
{
	{ STAT_CREATE_TIME,	"statinfo create time", SA_CONST, SU_DATETIME	},
	{ STAT_STARTUP_TIME,	"startup time"	, 	SA_CONST, SU_DATETIME	},
	{ STAT_CHECKPOINT_TIME,	"checkpoint time", 	SA_CONST, SU_DATETIME	},
};
#define NSYSID	(sizeof(SysStatDefinition)/sizeof(TStatDefinition))

int CStatManager::CreateStatIndex(
		const char *name,
		const char *indexfile,
		const TStatDefinition *statdef,
		char *szErrMsg, int iErrLen
	)
{
	if(access(indexfile, F_OK)==0)
	{
		snprintf(szErrMsg, iErrLen, "index file already exists");
		return -1;
	}

	int numinfo = NSYSID;
	int indexsize = sizeof(TStatHeader)+sizeof(TStatInfo)*NSYSID;
	int datasize = 16 + sizeof(int64_t)*NSYSID;

	unsigned int i;
	for(i=0; statdef[i].id; i++)
	{
		if(statdef[i].cnt+statdef[i].cnt1 > 16)
		{
			snprintf(szErrMsg, iErrLen, "too many argument counts");
			return -1;
		}
		numinfo++;
		indexsize += sizeof(TStatInfo);
		switch(statdef[i].type)
		{
		case SA_SAMPLE:
			indexsize += 16 * sizeof(int64_t);
			datasize  += 18 * sizeof(int64_t);
			break;
		case SA_EXPR:
			indexsize += (statdef[i].cnt+statdef[i].cnt1) * sizeof(int64_t);
		default:
			datasize += sizeof(int64_t);
		}
	}

	if(indexsize > (4<<20))
	{
		snprintf(szErrMsg, iErrLen, "index file size too large");
		return -1;
	}
	if(datasize > (1<<20))
	{
		snprintf(szErrMsg, iErrLen, "data file size too large");
		return -1;
	}

	TStatHeader *header = (TStatHeader *)mapfile(indexfile, indexsize);
	if (header == NULL)
	{
			snprintf(szErrMsg, iErrLen, "map stat file error");
			return -1;
	}

	header->signature = *(int *)"sTaT";
	header->version = 1;
	header->numinfo = numinfo;
	header->indexsize = indexsize;
	header->datasize = datasize;
	strncpy(header->name, name, sizeof(header->name));

	TStatInfo *si = header->first();
	unsigned int off = 16;

	for(i=0; i<NSYSID; i++, si=si->next())
	{
		si->id   = SysStatDefinition[i].id;
		si->type = SysStatDefinition[i].type;
		si->unit = SysStatDefinition[i].unit;
		si->off  = off;
		si->cnt  = 0;
		si->cnt1  = 0;
		strncpy(si->name, SysStatDefinition[i].name, sizeof(si->name));
		off += si->datasize();
	}
	for(i=0; statdef[i].id; i++, si=si->next())
	{
		si->id   = statdef[i].id;
		si->type = statdef[i].type;
		si->unit = statdef[i].unit;
		si->off  = off;
		si->cnt  = 0;
		si->cnt1  = 0;
		strncpy(si->name, statdef[i].name, sizeof(si->name));
		off += si->datasize();

		if(si->issample())
		{
			unsigned char &j = si->cnt;
			for(j=0; j<16 && statdef[i].arg[j]; j++)
				si->vptr[j] = statdef[i].arg[j];
		} else if(si->isexpr())
		{
			si->cnt = statdef[i].cnt;
			si->cnt1 = statdef[i].cnt1;
			for(int j=0; j<si->cnt+si->cnt1; j++)
				si->vptr[j] = statdef[i].arg[j];
		}
	}
	header->createtime = time(NULL); // create time
	munmap(header, indexsize);
	char buf[strlen(indexfile)+10];
	strncpy(buf, indexfile, strlen(indexfile)+10);
	char *p = strrchr(buf, '.');
	if(p==NULL) p = buf + strlen(buf);

	strncpy(p, ".dat", 5); unlink(buf);
	strncpy(p, ".10s", 5); unlink(buf);
	strncpy(p, ".10m", 5); unlink(buf);
	strncpy(p, ".all", 5); unlink(buf);
	return 0;
}

void *CStatManager::mapfile(const char *fn, int size)
{
	int fd = open(fn, O_RDWR|O_CREAT, 0666);
	void *map = NULL;
	if(fd >= 0)
	{
		if(size > 0)
			ftruncate(fd, size);
		else
			size = lseek(fd, 0L, SEEK_END);

		if(size > 0)
			map = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
	} else if(size > 0) {
		map = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	}
	if(map == MAP_FAILED)
	{
		map = NULL;
	}
	return map;
}

#if HAS_ATOMIC8
CStatItem CStatManager::GetItem(unsigned int id)
{
	//P __a(this);

	CStatInfo *i = idmap[id];
	if(i==NULL || i->issample())
	{
		CStatItem v;
		return v;
	}
	return (CStatItem)&at_cur(i->offset());
}


CStatItem CStatManager::Get10sItem(unsigned int id)
{
	
	CStatInfo *i = idmap[id];
	if(i==NULL || i->issample())
	{
		CStatItem v;
		return v;
	}
	return (CStatItem)&at_10s(i->offset());
}
#else

CStatItemS32 CStatManager::GetItemS32(unsigned int id)
{
	P __a(this);

	CStatInfo *i = idmap[id];
	if(i==NULL || i->issample() || !i->istype(1))
	{
		CStatItemS32 v;
		return v;
	}
	i->ltype = 1;
	return (CStatItemS32)(int32_t *)&at_cur(i->offset());
}

CStatItemU32 CStatManager::GetItemU32(unsigned int id)
{
	P __a(this);

	CStatInfo *i = idmap[id];
	if(i==NULL || i->issample() || i->istype(2))
	{
		CStatItemU32 v;
		return v;
	}
	i->ltype = 2;
	return (CStatItemU32)(uint32_t *)&at_cur(i->offset());
}

CStatItem CStatManager::GetItem(unsigned int id)
{
	P __a(this);

	CStatInfo *i = idmap[id];
	if(i==NULL || i->issample() || !i->istype(3))
	{
		CStatItem v;
		return v;
	}
	i->ltype = 3;

	if(i->vobj == NULL)
		i->vobj = new CStatItemObject(&at_cur(i->offset()));
	return CStatItem(i->vobj);
}

CStatItem CStatManager::Get10sItem(unsigned int id)
{
	P __a(this);

	CStatInfo *i = idmap[id];
	if(i==NULL || i->issample() || !i->istype(3))
	{
		CStatItem v;
		return v;
	}
	i->ltype = 3;

	if(i->vobj == NULL)
		i->vobj = new CStatItemObject(&at_10s(i->offset()));
	return CStatItem(i->vobj);
}

#endif
int64_t CStatManager::Get10SItemValue(unsigned int id)
{
	P __a(this);
	int64_t ddwValue = 0;
	CStatInfo *i = idmap[id];
	if(i==NULL || i->issample() )
	{
		return ddwValue;
	}
	ddwValue = at_10s(i->offset());
	return ddwValue;
}

CStatSample CStatManager::GetSample(unsigned int id)
{
	P __a(this);
	CStatInfo *i = idmap[id];
	if(i==NULL || !i->issample())
	{
		CStatSample v;
		return v;
	}

	if(i->sobj == NULL)
		i->sobj = new CStatSampleObject(i->si, &at_cur(i->offset()));

	CStatSample v(i->sobj);
	return v;
}

int CStatManager::SetCountBase(unsigned int id, const int64_t *v, int c)
{
	if(c < 0)
		c = 0;
	else if(c > 16)
		c = 16;

	P __a(this);
	CStatInfo *i = idmap[id];
	if(i==NULL || !i->issample())
		return -1;

	mprotect(header, indexsize, PROT_READ|PROT_WRITE);
	if(c > 0)
		memcpy(i->si->vptr, v, sizeof(int64_t)*c);
	i->si->cnt = c;
	mprotect(header, indexsize, PROT_READ);
	return c;

}

int CStatManager::GetCountBase(unsigned int id, int64_t *v)
{
	P __a(this);
	CStatInfo *i = idmap[id];
	if(i==NULL || !i->issample())
		return -1;

	if(i->si->cnt > 0)
		memcpy(v, i->si->vptr, sizeof(int64_t)*i->si->cnt);

	return i->si->cnt;

}

int64_t call_imm(CStatManager::CStatExpr *info, const char *map)
{
	return info->val;
}

int64_t call_id(CStatManager::CStatExpr *info, const char *map)
{
	return *(const int64_t *)(map+info->off0);
}

int64_t call_negative(CStatManager::CStatExpr *info, const char *map)
{
	return - *(const int64_t *)(map+info->off0);
}

int64_t call_shift_1(CStatManager::CStatExpr *info, const char *map)
{
	return *(const int64_t *)(map+info->off0) << 1;
}

int64_t call_shift_2(CStatManager::CStatExpr *info, const char *map)
{
	return *(const int64_t *)(map+info->off0) << 2;
}

int64_t call_shift_3(CStatManager::CStatExpr *info, const char *map)
{
	return *(const int64_t *)(map+info->off0) << 3;
}

int64_t call_shift_4(CStatManager::CStatExpr *info, const char *map)
{
	return *(const int64_t *)(map+info->off0) << 4;
}

int64_t call_id_multi(CStatManager::CStatExpr *info, const char *map)
{
	return *(const int64_t *)(map+info->off0) * info->val;
}

int64_t call_id_multi2(CStatManager::CStatExpr *info, const char *map)
{
	return *(const int64_t *)(map+info->off0) * *(const int64_t *)(map+info->off1);
}

CStatManager::CStatExpr *CStatManager::InitExpr(unsigned int cnt, int64_t *arg)
{
	CStatInfo *info;
	CStatExpr *expr = new CStatExpr[cnt];
	int val, id;
	int sub;
	for(unsigned int i=0; i<cnt; i++)
	{
		val = arg[i] >> 32;
		id = arg[i] & 0xFFFFFFFF;
		if(id==0)
		{
			expr[i].val = val;
			expr[i].off0 = 0;
			expr[i].off1 = 0;

			expr[i].call = &call_imm;
		} else if((id&0x80000000)==0)
		{
			expr[i].val = val;

			sub = id % 20;
			id /= 20;
			info = idmap[id];
			if(info==NULL || (sub>0 && !info->issample()))
			{
				goto bad;
			}
			// not yet  initialized
			if(info->isexpr() && info->expr==NULL)
				goto bad;
			expr[i].off0 = info->offset()+ sub * sizeof(int64_t);
			expr[i].off1 = 0;

			switch(val)
			{
			case -1: expr[i].call = &call_negative; break;
			case 0: expr[i].call = &call_imm; break;
			case 1: expr[i].call = &call_id; break;
			case 2: expr[i].call = &call_shift_1; break;
			case 4: expr[i].call = &call_shift_2; break;
			case 8: expr[i].call = &call_shift_3; break;
			case 16: expr[i].call = &call_shift_4; break;
			default: expr[i].call = &call_id_multi; break;
			}
		} else {
			expr[i].val = 0;

			id &= 0x7FFFFFFF;
			sub = id % 20;
			id /= 20;
			info = idmap[id];
			if(info==NULL || (sub>0 && !info->issample()))
			{
				goto bad;
			}
			// not yet  initialized
			if(info->isexpr() && info->expr==NULL)
				goto bad;
			expr[i].off0 = info->offset()+ sub * sizeof(int64_t);

			id = val & 0x7FFFFFFF;
			sub = id % 20;
			id /= 20;
			info = idmap[id];
			if(info==NULL || (sub>0 && !info->issample()))
			{
				goto bad;
			}
			// not yet  initialized
			if(info->isexpr() && info->expr==NULL)
				goto bad;
			expr[i].off1 = info->offset()+ sub * sizeof(int64_t);

			expr[i].call = &call_id_multi2;
		}
	}
	return expr;
bad:
	delete []expr;
	return NULL;
}

int64_t CStatManager::CalcExpr(const char *map, unsigned int cnt, CStatExpr *expr)
{
	int64_t v = 0;

	for(unsigned int i=0; i<cnt; i++)
		v += expr[i].call(&expr[i], map);
	return v;
}

static inline void trend(int64_t &m, int64_t s) { m = (m*63+(s<<10))>>6; }
static inline void ltrend(int64_t &m, int64_t s) { m = (m*255+s)>>8; }

void CStatManager::RunJobOnce(void)
{
	atomic_t *a0 = (atomic_t *)&at_cur(0);
	int a0v = atomic_add_return(1, a0);
	at_cur(4*8) = time(NULL); // checkpoint time

	for(unsigned i=0; i<numinfo; i++)
	{
		unsigned off = info[i].offset();
		switch(info[i].type())
		{
		case SA_SAMPLE:
			if(info[i].sobj)
			{
				info[i].sobj->output(&at_10s(off));
			} else {
				memcpy(&at_10s(off), &at_cur(off), 18*sizeof(int64_t));
				memset(&at_cur(off), 0, 18*sizeof(int64_t));
			}

			for(unsigned n=0; n<18; n++)
			{
				// count all
				at_all(off, n) += at_10s(off, n);
				// 10m
				trend(at_10m(off, n), at_10s(off, n));
			}
			break;
		case SA_COUNT:
#if !HAS_ATOMIC8
			if(info[i].ltype==1)
			{
				CStatItemS32 vi((int32_t *)&at_cur(off));
				at_10s(off) = vi.clear();
			} else
#else
			{
				CStatItem vi(&at_cur(off));
				at_10s(off) = vi.clear();
			}
#endif
			// count all
			at_all(off) += at_10s(off);

			// 10m
			trend(at_10m(off), at_10s(off));
			break;
		case SA_VALUE:
#if !HAS_ATOMIC8
			if(info[i].ltype==1)
			{
				CStatItemS32 vi((int32_t *)&at_cur(off));
				at_10s(off) = vi.get();
			} else
#else
			{
				CStatItem vi(&at_cur(off));
				at_10s(off) = vi.get();
			}
#endif
			// 10m
			trend(at_10m(off), at_10s(off));
			ltrend(at_all(off), at_10m(off));
			break;
		case SA_CONST:
			at_10s(off) = at_cur(off);
			at_10m(off) = at_cur(off);
			at_all(off) = at_cur(off);
			break;
		}
	}

	// calculate expression
	for(unsigned i=0; i<expr.size(); i++)
	{
		CStatInfo &e = *(expr[i]);
		unsigned off = e.offset();
		int64_t sum, div;

		if(e.si->cnt1==0)
		{
			at_10s(off) = CalcExpr(fmap[SC_10S], e.si->cnt, e.expr);
			at_all(off) = CalcExpr(fmap[SC_ALL], e.si->cnt, e.expr);
		} else {
			div = CalcExpr(fmap[SC_10S], e.si->cnt1, e.expr+e.si->cnt);
			if(div != 0) {
				sum = CalcExpr(fmap[SC_10S], e.si->cnt, e.expr);
				at_10s(off) = sum/div;
			}
			div = CalcExpr(fmap[SC_ALL], e.si->cnt1, e.expr+e.si->cnt);
			if(div != 0) {
				sum = CalcExpr(fmap[SC_ALL], e.si->cnt, e.expr);
				at_all(off) = sum/div;
			}
		}
		trend(at_10m(off), at_10s(off));
	}
	atomic_set(a0+1, a0v);
}

int CStatManager::Lock(const char *type)
{
	if(lockfd >= 0)
		return 0;

	lockfd = UnixSocketLock("tlock-stat-%s-%llu-%llu",
		type, (long long)lockdev, (long long)lockino);
	return lockfd >= 0 ? 0 : -1;
}

void CStatManager::clear(void)
{
	for(unsigned i=0; i<numinfo; i++)
	{
		unsigned off = info[i].offset();
		switch(info[i].type())
		{
		case SA_SAMPLE:
			break;
		case SA_COUNT:
			at_cur(off) =0;
			at_10s(off) =0;
			at_10m(off) =0;
//			at_all(off) =0;
			break;
		case SA_VALUE:
			break;
		case SA_CONST:
			break;
		}
	}
}
