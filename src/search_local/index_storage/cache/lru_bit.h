/*
 * =====================================================================================
 *
 *       Filename:  lru_bit.h
 *
 *    Description:  lru bitmap restore function. 
 * 					recording master lru change infomation in order to improve slave hit rate.
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

#ifndef __LRU_BIT_H
#define __LRU_BIT_H

#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include "bitsop.h"
#include "timer_list.h"
#include "logger.h"
#include "raw_data.h"
#include "data_chunk.h"
#include "admin_tdef.h"
#include "sys_malloc.h"

#define IDX_SIZE (4 << 10)	 //4K
#define BLK_SIZE (256 << 10) //256K
#define LRU_BITS (2 << 10)	 //2k

#define BBLK_OFF(v) (v >> 21)
#define IDX_BYTE_OFF(v) ((v >> 9) & 0xFFF)
#define IDX_BYTE_SHIFT(v) ((v >> 6) & 0x7)
#define BLK_8_BYTE_OFF(v) ((v >> 6) & 0x7FFF)
#define BLK_BYTE_OFF(v) ((v >> 3) & 0x3FFFF)
#define BLK_BYTE_SHIFT(v) (v & 0x7)

/*
 *  Node ID 位图储存表
 *
 *====================================================================================
 *| 11 b      | 12 b         | 3 b           | 3 b                 |     3 b       | 
 *| bblk off  | idx byte off | idx byte shift| 
 *            |        blk  8-bytes off      |
 *            |                   blk byte off                     | blk byte shift|
 *====================================================================================
 */

typedef struct lru_bit
{
	char _idx[IDX_SIZE];
	char _blk[BLK_SIZE];

	lru_bit()
	{
		bzero(_idx, sizeof(_idx));
		bzero(_blk, sizeof(_blk));
	}

	~lru_bit() {}

	/* 如果set命中返回1，否则返回0 */
	int set(unsigned int v, int b)
	{
		int hit = 0;
		uint32_t byte_shift = BLK_BYTE_SHIFT(v);
		uint32_t byte_offset = BLK_BYTE_OFF(v);

		if (b)
		{
			if (ISSET_B(byte_shift, _blk + byte_offset))
			{
				hit = 1;
			}
			else
			{

				SET_B(byte_shift, _blk + byte_offset);
				SET_B(IDX_BYTE_SHIFT(v), _idx + IDX_BYTE_OFF(v));
			}
		}
		else
		{
			CLR_B(byte_shift, _blk + byte_offset);
		}

		return hit;
	}

	/* return total clear bits */
	int clear(int idx_off)
	{
		int clear_bits = COUNT_B(_blk + (idx_off << 6), 1 << 6);

		/* 1 byte idx */
		memset(_idx + idx_off, 0x00, 1);

		/* 64 bytes blk */
		memset(_blk + (idx_off << 6), 0x00, 1 << 6);

		return clear_bits;
	}

	uint64_t read(int idx_off, int idx_shift)
	{
		unsigned char *ix = (unsigned char *)_idx + idx_off;

		if (ISSET_B(idx_shift, ix))
		{

			uint64_t *p = (uint64_t *)_blk;
			return p[(idx_off << 3) + idx_shift];
		}
		else
		{
			return 0;
		}
	}
} lru_bit_t;

/* 
 *
 * 扫描频率、速度控制 
 * 1. 不能影响正常的update同步
 * 2. 尽量在控制时间内完成一趟扫描
 *
 */
#define LRU_SCAN_STOP_UNTIL 20 //20
#define LRU_SCAN_INTERVAL 10   //10ms

class RawData;
class LruWriter
{
public:
	LruWriter(BinlogWriter *);
	virtual ~LruWriter();

	int Init();
	int Write(unsigned int id);
	int Commit(void);

private:
	BinlogWriter *_log_writer;
	RawData *_raw_data;
};

class LruBitUnit;
class LruBitObj : private TimerObject
{
public:
	LruBitObj(LruBitUnit *);
	~LruBitObj();

	int Init(BinlogWriter *, int stop_until = LRU_SCAN_STOP_UNTIL);
	int SetNodeID(unsigned int v, int b);

private:
	int Scan(void);
	virtual void timer_notify(void);

private:
	lru_bit_t *_lru_bits[LRU_BITS];
	uint16_t _max_lru_bit;
	uint16_t _scan_lru_bit;
	uint16_t _scan_idx_off;
	uint16_t _scan_stop_until;

	LruWriter *_lru_writer;
	LruBitUnit *_owner;

	friend class LruBitUnit;

private:
	/* statistic */
	uint32_t scan_tm;
	StatItemU32 lru_scan_tm;

	StatItemU32 total_bits;
	StatItemU32 total_1_bits;

	StatItemU32 lru_set_count;
	StatItemU32 lru_set_hit_count;
	StatItemU32 lru_clr_count;
};

class LruBitUnit
{
public:
	LruBitUnit(TimerUnit *);
	~LruBitUnit();

	int Init(BinlogWriter *);
	void enable_log(void);
	void disable_log(void);
	int check_status() { return _is_start; } // 0：不启动， 1：启动
	int Set(unsigned int v);
	int Unset(unsigned int v);

private:
	TimerList *_scan_timerlist;
	LruBitObj *_lru_bit_obj;
	int _is_start;
	TimerUnit *_owner;

	friend class LruBitObj;
};

#endif
