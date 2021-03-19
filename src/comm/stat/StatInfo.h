#ifndef __H_STAT_INFO_INFO_H_
#define __H_STAT_INFO_INFO_H_

#include <stddef.h>
#include <stdint.h>

enum {
	SA_VALUE	= 0,
	SA_COUNT,
	SA_SAMPLE,
	SA_CONST,
	SA_EXPR,
};

enum {
	SC_CUR	= 0,
	SC_10S,
	SC_10M,
	SC_ALL,
	SCC_10S,
	SCC_10M,
	SCC_ALL,
};

enum {
	SU_HIDE = 0,
	SU_INT,
	SU_INT_1,
	SU_INT_2,
	SU_INT_3,
	SU_INT_4,
	SU_INT_5,
	SU_INT_6,
	SU_DATETIME,
	SU_VERSION,
	SU_DATE,
	SU_TIME,
	SU_MSEC,
	SU_USEC,
	SU_PERCENT,
	SU_PERCENT_1,
	SU_PERCENT_2,
	SU_PERCENT_3,
        SU_BOOL,
};

#define STAT_CREATE_TIME	1
#define STAT_STARTUP_TIME	2
#define STAT_CHECKPOINT_TIME	3

#define MIN_STAT_ID		10
#define MAX_STAT_ID		100000000

#define EXPR_NUM(x)		(((int64_t)(x))<<32)
#define EXPR_ID_(x,y)		((x)*20+(y))
#define EXPR_IDV(x,y,z)		(EXPR_ID_(x,y)+EXPR_NUM(z))
#define EXPR_ID2(x,y,x1,y1)	(EXPR_ID_(x,y)+0x80000000+EXPR_NUM(EXPR_ID_(x1,y1)))

struct TStatInfo
{
	unsigned int id;      // stat id
	unsigned int off;     // offset from data file
	unsigned char type;   // stat attr
	unsigned char unit;   // item count
	unsigned char cnt;    // item count
	unsigned char cnt1;   // item count
	unsigned int  resv;   // item count
	char name[32];
	int64_t vptr[0];

	int issample(void) const { return type==SA_SAMPLE; }
	int iscounter(void) const { return type==SA_COUNT; }
	int isvalue(void) const { return type==SA_VALUE; }
	int isconst(void) const { return type==SA_CONST; }
	int isexpr(void) const { return type==SA_EXPR; }
	int datasize(void) const {
		return issample()?sizeof(int64_t)*(16+2):sizeof(int64_t);
	}
	TStatInfo *next(void) const {
		return (TStatInfo *)((char *)this +
				offsetof(TStatInfo, vptr) +
				(issample()?16*sizeof(int64_t):0) +
				(isexpr()?(cnt+cnt1)*sizeof(int64_t):0)
			);
	}
};

struct TStatHeader
{
	unsigned int signature;
	unsigned char version;
	unsigned char zero[3];
	unsigned int numinfo;
	unsigned int indexsize;
	unsigned int datasize;
	unsigned int createtime;
	unsigned int reserved[2];
	char name[64];

	TStatInfo *first(void) const { return (TStatInfo *)(this+1); }
	TStatInfo *last(void) const { return (TStatInfo *)((char *)this + indexsize); }
};

struct TStatDefinition
{
	unsigned int id;
	const char *name;
	unsigned char type;
	unsigned char unit;
	unsigned char cnt;
	unsigned char cnt1;
	int64_t arg[16];
};

#endif
