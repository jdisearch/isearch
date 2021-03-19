#ifndef _STAT_MANAGER_H_
#define _STAT_MANAGER_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <map>
#include <vector>

#include "StatInfo.h"
#include "atomic.h"

struct CStatLock
{
public:
	class P {
	private:
		CStatLock *ptr;
	public:
		P(CStatLock *p) { ptr = p; pthread_mutex_lock(&ptr->lock); }
		~P() { pthread_mutex_unlock(&ptr->lock); }
	};
private:
	friend class P;
	pthread_mutex_t lock;
public:
	~CStatLock(){}
	CStatLock(){ pthread_mutex_init(&lock, 0); }
};

#if HAS_ATOMIC8
struct CStatItem
{
private:
	typedef int64_t V;
	static int64_t dummy;
	atomic8_t *ptr;
public:
	~CStatItem(void) { }
	CStatItem(void) { ptr = (atomic8_t *)&dummy; }
	CStatItem(volatile V *p) : ptr((atomic8_t *)p) { }
	CStatItem(const CStatItem &f) : ptr(f.ptr) { }

	inline V get(void) const { return atomic8_read(ptr); }
	inline V set(V v) { atomic8_set(ptr, v); return v; }
	inline V add(V v) { return atomic8_add_return(v, ptr); }
	inline V sub(V v) { return atomic8_sub_return(v, ptr); }
	inline V clear(void) { return atomic8_clear(ptr);  }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) const { return get(); }
	inline V operator= (V v) { return set(v); }
	inline V operator+= (V v) { return add(v); }
	inline V operator-= (V v) { return sub(v); }
	inline V operator++ (void) { return inc(); }
	inline V operator-- (void) { return dec(); }
	inline V operator++ (int) { return inc()-1; }
	inline V operator-- (int) { return dec()+1; }
};
typedef CStatItem CStatItemU32;
typedef CStatItem CStatItemS32;

#else
struct CStatItemU32
{
private:
	typedef uint32_t V;
	static uint32_t dummy;
	atomic_t *ptr;
public:
	~CStatItemU32(void) { }
	CStatItemU32(void) { ptr = (atomic_t *)&dummy; }
	CStatItemU32(volatile V *p) : ptr((atomic_t *)p) { }
	CStatItemU32(const CStatItemU32 &f) : ptr(f.ptr) { }

	inline V get(void) const { return atomic_read(ptr); }
	inline V set(V v) { atomic_set(ptr, v); return v; }
	inline V add(V v) { return atomic_add_return(v, ptr); }
	inline V sub(V v) { return atomic_sub_return(v, ptr); }
	inline V clear(void) { return atomic_clear(ptr);  }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) const { return get(); }
	inline V operator= (V v) { return set(v); }
	inline V operator+= (V v) { return add(v); }
	inline V operator-= (V v) { return sub(v); }
	inline V operator++ (void) { return inc(); }
	inline V operator-- (void) { return dec(); }
	inline V operator++ (int) { return inc()-1; }
	inline V operator-- (int) { return dec()+1; }
};
struct CStatItemS32
{
private:
	typedef int32_t V;
	static int32_t dummy;
	atomic_t *ptr;
public:
	~CStatItemS32(void) { }
	CStatItemS32(void) { ptr = (atomic_t *)&dummy; }
	CStatItemS32(volatile V *p) : ptr((atomic_t *)p) { }
	CStatItemS32(const CStatItemS32 &f) : ptr(f.ptr) { }

	inline V get(void) const { return atomic_read(ptr); }
	inline V set(V v) { atomic_set(ptr, v); return v; }
	inline V add(V v) { return atomic_add_return(v, ptr); }
	inline V sub(V v) { return atomic_sub_return(v, ptr); }
	inline V clear(void) { return atomic_clear(ptr);  }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) const { return get(); }
	inline V operator= (V v) { return set(v); }
	inline V operator+= (V v) { return add(v); }
	inline V operator-= (V v) { return sub(v); }
	inline V operator++ (void) { return inc(); }
	inline V operator-- (void) { return dec(); }
	inline V operator++ (int) { return inc()-1; }
	inline V operator-- (int) { return dec()+1; }
};

struct CStatItemObject: private CStatLock
{
private:
	typedef int64_t V;
	volatile V *ptr;

	CStatItemObject(const CStatItemObject &);
public:
	~CStatItemObject(void) { }
	CStatItemObject(void) { }
	CStatItemObject(volatile V *p) : ptr(p) { }

	inline V get(void) { P a(this); return *ptr; }
	inline V set(V v) { P a(this); *ptr = v;  return *ptr; }
	inline V add(V v) { P a(this); *ptr += v; return *ptr; }
	inline V sub(V v) { P a(this); *ptr -= v; return *ptr; }
	inline V clear(void) { P a(this); V v = *ptr; *ptr=0; return v; }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) { return get(); }
	inline V operator= (V v) { return set(v); }
	inline V operator+= (V v) { return add(v); }
	inline V operator-= (V v) { return sub(v); }
	inline V operator++ (void) { return inc(); }
	inline V operator-- (void) { return dec(); }
	inline V operator++ (int) { return inc()-1; }
	inline V operator-- (int) { return dec()+1; }
};

struct CStatItem
{
private:
	typedef int64_t V;
	CStatItemObject *ptr;
	static CStatItemObject dummy;

public:
	~CStatItem(void) { }
	CStatItem(void) { ptr = &dummy; }
	CStatItem(CStatItemObject *p) : ptr(p) { }
	CStatItem(const CStatItem &f) : ptr(f.ptr) { }

	inline V get(void) { return ptr->get(); }
	inline V set(V v) { return ptr->set(v); }
	inline V add(V v) { return ptr->add(v); }
	inline V sub(V v) { return ptr->sub(v); }
	inline V clear(void) { return ptr->clear();  }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) { return get(); }
	inline V operator= (V v) { return set(v); }
	inline V operator+= (V v) { return add(v); }
	inline V operator-= (V v) { return sub(v); }
	inline V operator++ (void) { return inc(); }
	inline V operator-- (void) { return dec(); }
	inline V operator++ (int) { return inc()-1; }
	inline V operator-- (int) { return dec()+1; }
};
#endif

struct CStatSampleObject : private CStatLock
{
private:
	const TStatInfo *info;
	int64_t *cnt;
	CStatSampleObject(const CStatSampleObject&);
	static int64_t dummyCnt[2];
	static const TStatInfo dummyInfo;
public:
	~CStatSampleObject(void) { }
	CStatSampleObject(void) : info(&dummyInfo), cnt(dummyCnt) { }
	CStatSampleObject(const TStatInfo *i, int64_t *c) : info(i), cnt(c) { }

	int64_t count(unsigned int n=0);
	int64_t sum(void);
	int64_t average(int64_t o);
	void push(int64_t v);
	void output(int64_t *v);
};

struct CStatSample
{
private:
	CStatSampleObject *ptr;
	static CStatSampleObject dummy;
public:
	~CStatSample(void) { }
	CStatSample(void) { ptr = &dummy; }
	CStatSample(CStatSampleObject *p) : ptr(p) { }
	CStatSample(const CStatSample &f) : ptr(f.ptr) { }

	int64_t count(unsigned int n=0) { return ptr->count(n); }
	int64_t sum(void) { return ptr->sum(); }
	int64_t average(int64_t o) { return ptr->average(o); }
	void push(int64_t v) { return ptr->push(v); }
	void operator<<(int64_t v) { return ptr->push(v); }
};

class CStatManager: protected CStatLock
{
public: // types
	struct CStatExpr {
		unsigned int off0;
		unsigned int off1;
		int val;
		int64_t (*call)(CStatExpr *info, const char *map);
	};
protected: // types
	struct CStatInfo {
	private:
		friend class CStatManager;

#if !HAS_ATOMIC8
		CStatItemObject *vobj;
		int ltype;
#endif
		CStatManager *owner;
		TStatInfo *si;
		CStatSampleObject *sobj;
		CStatExpr *expr;
		
		CStatInfo() :
#if !HAS_ATOMIC8
			vobj(0), ltype(0),
#endif
			owner(0), si(0), sobj(0), expr(0) {}
		~CStatInfo() {
#if !HAS_ATOMIC8
			if(vobj) delete vobj;
#endif
			if(sobj) delete sobj;
			if(expr) delete []expr;
		}

#if !HAS_ATOMIC8
		int istype(int t) { return ltype==0 || ltype==t; }
#endif
	public:
		const TStatInfo &info(void) { return *si; }
		inline unsigned id(void) const { return si->id; }
		inline unsigned int type(void) const { return si->type; }
		inline unsigned int unit(void) const { return si->unit; }
		inline int issample(void) const { return si->issample(); }
		inline int iscounter(void) const { return si->iscounter(); }
		inline int isvalue(void) const { return si->isvalue(); }
		inline int isconst(void) const { return si->isconst(); }
		inline int isexpr(void) const { return si->isexpr(); }
		inline unsigned offset(void) const { return si->off; }
		inline unsigned count(void) const { return si->cnt; }
		inline const char *name(void) const { return si->name; }
	};


	static void *mapfile(const char *fn, int size);
protected: // members
	int lockfd;
	dev_t lockdev;
	ino_t lockino;

	char szErrMsg[128];
	TStatHeader *header;
	unsigned int indexsize;
	char *fmap[7];
	const unsigned int nfmap(void) const { return sizeof(fmap)/sizeof(*fmap); }
	int64_t &at(unsigned int cat, unsigned off, unsigned int n=0) { return ((int64_t *)(fmap[cat]+off))[n]; }
	int64_t &at_cur(unsigned off, unsigned int n=0) { return at(SC_CUR, off, n); }
	int64_t &at_10s(unsigned off, unsigned int n=0) { return at(SC_10S, off, n); }
	int64_t &at_10m(unsigned off, unsigned int n=0) { return at(SC_10M, off, n); }
	int64_t &at_all(unsigned off, unsigned int n=0) { return at(SC_ALL, off, n); }
	int64_t &atc_10s(unsigned off, unsigned int n=0) { return at(SCC_10S, off, n); }
	int64_t &atc_10m(unsigned off, unsigned int n=0) { return at(SCC_10M, off, n); }
	int64_t &atc_all(unsigned off, unsigned int n=0) { return at(SCC_ALL, off, n); }


	std::map<unsigned int, CStatInfo *> idmap;
	std::vector<CStatInfo *>expr;
	CStatInfo *info;
	unsigned int numinfo;
	
private:
	CStatManager(const CStatManager &);
public: // public method
	CStatManager(void);
	~CStatManager(void);

	const char *ErrorMessage(void) const { return szErrMsg; }
	int InitStatInfo(const char *, const char *, int isc=0);
	static int CreateStatIndex(const char *, const char *, const TStatDefinition *, char *, int);

	CStatItem GetItem(unsigned int id);
	CStatItem Get10sItem(unsigned int id);
	int64_t Get10SItemValue(unsigned int id);/*get 10s static value , add by tomchen*/
#if HAS_ATOMIC8
	inline CStatItemU32 GetItemU32(unsigned int id) { return GetItem(id); }
	inline CStatItemS32 GetItemS32(unsigned int id) { return GetItem(id); }
#else
	CStatItemU32 GetItemU32(unsigned int id);
	CStatItemS32 GetItemS32(unsigned int id);
#endif
	CStatSample GetSample(unsigned int id);

	int SetCountBase(unsigned int id, const int64_t *v, int c);
	int GetCountBase(unsigned int id, int64_t *v);

	int Lock(const char *type);
	int Unlock(void);
	void RunJobOnce(void);
	void clear(void);

protected:
	CStatExpr *InitExpr(unsigned int cnt, int64_t *arg);
	int64_t CalcExpr(const char *, unsigned int, CStatExpr *);
	
};

#endif
