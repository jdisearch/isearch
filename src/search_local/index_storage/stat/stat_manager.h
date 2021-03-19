/*
 * =====================================================================================
 *
 *       Filename:  stat_manager.h
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
#ifndef _STAT_MANAGER_H_
#define _STAT_MANAGER_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <map>
#include <vector>

#include "stat_info.h"
#include "atomic.h"

struct StatLock
{
public:
	class P
	{
	private:
		StatLock *ptr;

	public:
		P(StatLock *p)
		{
			ptr = p;
			pthread_mutex_lock(&ptr->lock);
		}
		~P() { pthread_mutex_unlock(&ptr->lock); }
	};

private:
	friend class P;
	pthread_mutex_t lock;

public:
	~StatLock() {}
	StatLock() { pthread_mutex_init(&lock, 0); }
};

#if HAS_ATOMIC8
struct StatItem
{
private:
	typedef int64_t V;
	static int64_t dummy;
	atomic8_t *ptr;

public:
	~StatItem(void) {}
	StatItem(void) { ptr = (atomic8_t *)&dummy; }
	StatItem(volatile V *p) : ptr((atomic8_t *)p) {}
	StatItem(const StatItem &f) : ptr(f.ptr) {}

	inline V get(void) const { return atomic8_read(ptr); }
	inline V set(V v)
	{
		atomic8_set(ptr, v);
		return v;
	}
	inline V add(V v) { return atomic8_add_return(v, ptr); }
	inline V sub(V v) { return atomic8_sub_return(v, ptr); }
	inline V clear(void) { return atomic8_clear(ptr); }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) const { return get(); }
	inline V operator=(V v) { return set(v); }
	inline V operator+=(V v) { return add(v); }
	inline V operator-=(V v) { return sub(v); }
	inline V operator++(void) { return inc(); }
	inline V operator--(void) { return dec(); }
	inline V operator++(int) { return inc() - 1; }
	inline V operator--(int) { return dec() + 1; }
};
typedef StatItem StatItemU32;
typedef StatItem StatItemS32;

#else
struct StatItemU32
{
private:
	typedef uint32_t V;
	static uint32_t dummy;
	atomic_t *ptr;

public:
	~StatItemU32(void) {}
	StatItemU32(void) { ptr = (atomic_t *)&dummy; }
	StatItemU32(volatile V *p) : ptr((atomic_t *)p) {}
	StatItemU32(const StatItemU32 &f) : ptr(f.ptr) {}

	inline V get(void) const { return atomic_read(ptr); }
	inline V set(V v)
	{
		atomic_set(ptr, v);
		return v;
	}
	inline V add(V v) { return atomic_add_return(v, ptr); }
	inline V sub(V v) { return atomic_sub_return(v, ptr); }
	inline V clear(void) { return atomic_clear(ptr); }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) const { return get(); }
	inline V operator=(V v) { return set(v); }
	inline V operator+=(V v) { return add(v); }
	inline V operator-=(V v) { return sub(v); }
	inline V operator++(void) { return inc(); }
	inline V operator--(void) { return dec(); }
	inline V operator++(int) { return inc() - 1; }
	inline V operator--(int) { return dec() + 1; }
};
struct StatItemS32
{
private:
	typedef int32_t V;
	static int32_t dummy;
	atomic_t *ptr;

public:
	~StatItemS32(void) {}
	StatItemS32(void) { ptr = (atomic_t *)&dummy; }
	StatItemS32(volatile V *p) : ptr((atomic_t *)p) {}
	StatItemS32(const StatItemS32 &f) : ptr(f.ptr) {}

	inline V get(void) const { return atomic_read(ptr); }
	inline V set(V v)
	{
		atomic_set(ptr, v);
		return v;
	}
	inline V add(V v) { return atomic_add_return(v, ptr); }
	inline V sub(V v) { return atomic_sub_return(v, ptr); }
	inline V clear(void) { return atomic_clear(ptr); }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) const { return get(); }
	inline V operator=(V v) { return set(v); }
	inline V operator+=(V v) { return add(v); }
	inline V operator-=(V v) { return sub(v); }
	inline V operator++(void) { return inc(); }
	inline V operator--(void) { return dec(); }
	inline V operator++(int) { return inc() - 1; }
	inline V operator--(int) { return dec() + 1; }
};

struct StatItemObject : private StatLock
{
private:
	typedef int64_t V;
	volatile V *ptr;

	StatItemObject(const StatItemObject &);

public:
	~StatItemObject(void) {}
	StatItemObject(void) {}
	StatItemObject(volatile V *p) : ptr(p) {}

	inline V get(void)
	{
		P a(this);
		return *ptr;
	}
	inline V set(V v)
	{
		P a(this);
		*ptr = v;
		return *ptr;
	}
	inline V add(V v)
	{
		P a(this);
		*ptr += v;
		return *ptr;
	}
	inline V sub(V v)
	{
		P a(this);
		*ptr -= v;
		return *ptr;
	}
	inline V clear(void)
	{
		P a(this);
		V v = *ptr;
		*ptr = 0;
		return v;
	}
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) { return get(); }
	inline V operator=(V v) { return set(v); }
	inline V operator+=(V v) { return add(v); }
	inline V operator-=(V v) { return sub(v); }
	inline V operator++(void) { return inc(); }
	inline V operator--(void) { return dec(); }
	inline V operator++(int) { return inc() - 1; }
	inline V operator--(int) { return dec() + 1; }
};

struct StatItem
{
private:
	typedef int64_t V;
	StatItemObject *ptr;
	static StatItemObject dummy;

public:
	~StatItem(void) {}
	StatItem(void) { ptr = &dummy; }
	StatItem(StatItemObject *p) : ptr(p) {}
	StatItem(const StatItem &f) : ptr(f.ptr) {}

	inline V get(void) { return ptr->get(); }
	inline V set(V v) { return ptr->set(v); }
	inline V add(V v) { return ptr->add(v); }
	inline V sub(V v) { return ptr->sub(v); }
	inline V clear(void) { return ptr->clear(); }
	inline V inc(void) { return add(1); }
	inline V dec(void) { return sub(1); }
	inline operator V(void) { return get(); }
	inline V operator=(V v) { return set(v); }
	inline V operator+=(V v) { return add(v); }
	inline V operator-=(V v) { return sub(v); }
	inline V operator++(void) { return inc(); }
	inline V operator--(void) { return dec(); }
	inline V operator++(int) { return inc() - 1; }
	inline V operator--(int) { return dec() + 1; }
};
#endif

struct StatSampleObject : private StatLock
{
private:
	const DTCStatInfo *info;
	int64_t *cnt;
	StatSampleObject(const StatSampleObject &);
	static int64_t dummyCnt[2];
	static const DTCStatInfo dummyInfo;

public:
	~StatSampleObject(void) {}
	StatSampleObject(void) : info(&dummyInfo), cnt(dummyCnt) {}
	StatSampleObject(const DTCStatInfo *i, int64_t *c) : info(i), cnt(c) {}

	int64_t count(unsigned int n = 0);
	int64_t sum(void);
	int64_t average(int64_t o);
	void push(int64_t v);
	void output(int64_t *v);
};

struct StatSample
{
private:
	StatSampleObject *ptr;
	static StatSampleObject dummy;

public:
	~StatSample(void) {}
	StatSample(void) { ptr = &dummy; }
	StatSample(StatSampleObject *p) : ptr(p) {}
	StatSample(const StatSample &f) : ptr(f.ptr) {}

	int64_t count(unsigned int n = 0) { return ptr->count(n); }
	int64_t sum(void) { return ptr->sum(); }
	int64_t average(int64_t o) { return ptr->average(o); }
	void push(int64_t v) { return ptr->push(v); }
	void operator<<(int64_t v) { return ptr->push(v); }
};

class StatManager : protected StatLock
{
public: // types
	struct StatExpr
	{
		unsigned int off0;
		unsigned int off1;
		int val;
		int64_t (*call)(StatExpr *info, const char *map);
	};

protected: // types
	struct StatInfo
	{
	private:
		friend class StatManager;

#if !HAS_ATOMIC8
		StatItemObject *vobj;
		int ltype;
#endif
		StatManager *owner;
		DTCStatInfo *si;
		StatSampleObject *sobj;
		StatExpr *expr;

		StatInfo() :
#if !HAS_ATOMIC8
					 vobj(0), ltype(0),
#endif
					 owner(0), si(0), sobj(0), expr(0)
		{
		}
		~StatInfo()
		{
#if !HAS_ATOMIC8
			if (vobj)
				delete vobj;
#endif
			if (sobj)
				delete sobj;
			if (expr)
				delete[] expr;
		}

#if !HAS_ATOMIC8
		int istype(int t)
		{
			return ltype == 0 || ltype == t;
		}
#endif
	public:
		const DTCStatInfo &info(void) { return *si; }
		inline unsigned id(void) const { return si->id; }
		inline unsigned int type(void) const { return si->type; }
		inline unsigned int unit(void) const { return si->unit; }
		inline int is_sample(void) const { return si->is_sample(); }
		inline int is_counter(void) const { return si->is_counter(); }
		inline int is_value(void) const { return si->is_value(); }
		inline int is_const(void) const { return si->is_const(); }
		inline int is_expr(void) const { return si->is_expr(); }
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
	DTCStatHeader *header;
	unsigned int indexsize;
	char *fmap[7];
	const unsigned int nfmap(void) const { return sizeof(fmap) / sizeof(*fmap); }
	int64_t &at(unsigned int cat, unsigned off, unsigned int n = 0) { return ((int64_t *)(fmap[cat] + off))[n]; }
	int64_t &at_cur(unsigned off, unsigned int n = 0) { return at(SC_CUR, off, n); }
	int64_t &at_10s(unsigned off, unsigned int n = 0) { return at(SC_10S, off, n); }
	int64_t &at_10m(unsigned off, unsigned int n = 0) { return at(SC_10M, off, n); }
	int64_t &at_all(unsigned off, unsigned int n = 0) { return at(SC_ALL, off, n); }
	int64_t &atc_10s(unsigned off, unsigned int n = 0) { return at(SCC_10S, off, n); }
	int64_t &atc_10m(unsigned off, unsigned int n = 0) { return at(SCC_10M, off, n); }
	int64_t &atc_all(unsigned off, unsigned int n = 0) { return at(SCC_ALL, off, n); }

	std::map<unsigned int, StatInfo *> idmap;
	std::vector<StatInfo *> expr;
	StatInfo *info;
	unsigned int numinfo;

private:
	StatManager(const StatManager &);

public: // public method
	StatManager(void);
	~StatManager(void);

	const char *error_message(void) const { return szErrMsg; }
	int init_stat_info(const char *, const char *, int isc = 0);
	static int CreateStatIndex(const char *, const char *, const DTCStatDefinition *, char *, int);

	StatItem get_item(unsigned int id);
	StatItem get10s_item(unsigned int id);
	int64_t get10_s_item_value(unsigned int id); /*get 10s static value , add by tomchen*/
#if HAS_ATOMIC8
	inline StatItemU32 get_item_u32(unsigned int id)
	{
		return get_item(id);
	}
	inline StatItemS32 get_item_s32(unsigned int id) { return get_item(id); }
#else
	StatItemU32 get_item_u32(unsigned int id);
	StatItemS32 get_item_s32(unsigned int id);
#endif
	StatSample get_sample(unsigned int id);

	int set_count_base(unsigned int id, const int64_t *v, int c);
	int get_count_base(unsigned int id, int64_t *v);

	int Lock(const char *type);
	int Unlock(void);
	void run_job_once(void);
	void clear(void);

protected:
	StatExpr *init_expr(unsigned int cnt, int64_t *arg);
	int64_t calc_expr(const char *, unsigned int, StatExpr *);
};

#endif
