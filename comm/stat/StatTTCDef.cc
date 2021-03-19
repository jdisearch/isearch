#include <stddef.h>

#include "StatInfo.h"
#include "StatTTCDef.h"

TStatDefinition StatDefinition[] =
{
	// ��ͬ��ͳ����Ŀ
	{ S_VERSION,"server version",	SA_CONST, SU_VERSION	},
	{ C_TIME,   "compile time", 	SA_CONST, SU_DATETIME},

	{ LOG_COUNT_0,	"logcount - emerg(0)",	SA_COUNT, SU_INT	},
	{ LOG_COUNT_1,	"logcount - alert(1)",	SA_COUNT, SU_INT	},
	{ LOG_COUNT_2,	"logcount - crit(2)",	SA_COUNT, SU_INT	},
	{ LOG_COUNT_3,	"logcount - error(3)",	SA_COUNT, SU_INT	},
	{ LOG_COUNT_4,	"logcount - warning(4)",SA_COUNT, SU_INT	},
	{ LOG_COUNT_5,	"logcount - notice(5)",	SA_COUNT, SU_INT	},
	{ LOG_COUNT_6,	"logcount - info(6)",	SA_COUNT, SU_INT	},
	{ LOG_COUNT_7,	"logcount - debug(7)",	SA_COUNT, SU_INT	},


	{ REQ_USEC_ALL,	"request usec - ALL",	SA_SAMPLE, SU_USEC, 0, 0,
		{ 100, 200, 300, 400, 500, 600, 800, 1200, 2000, 10000, 20000, 100000, 200000, 1000000, 2000000, 10000000} },
//	{ REQ_USEC_GET,	"request usec - select",SA_SAMPLE, SU_USEC	},
//	{ REQ_USEC_INS,	"request usec - insert",SA_SAMPLE, SU_USEC	},
//	{ REQ_USEC_UPD,	"request usec - update",SA_SAMPLE, SU_USEC	},
//	{ REQ_USEC_DEL,	"request usec - delete",SA_SAMPLE, SU_USEC	},
//	{ REQ_USEC_FLUSH,"request usec - flush",SA_SAMPLE, SU_USEC	},
//	{ REQ_USEC_HIT,	"request usec - hit",	SA_SAMPLE, SU_USEC	},
//	{ REQ_USEC_REPLACE,	"request usec - replace",	SA_SAMPLE, SU_USEC	},

	{ ACCEPT_COUNT,	"network - accept reqs",SA_COUNT, SU_INT	},
	{ CONN_COUNT,	"network - cur connect",SA_VALUE, SU_INT	},

//	{ CUR_QUEUE_COUNT,"queue - cur count",SA_VALUE, SU_INT	},
//	{ MAX_QUEUE_COUNT,"queue - max count",SA_VALUE, SU_INT	},
//	{ QUEUE_EFF,	  "queue - cur efficiency",SA_EXPR, SU_PERCENT_2, 1, 1,
//		{
//			EXPR_IDV(CUR_QUEUE_COUNT, 0, 10000),
//			EXPR_IDV(MAX_QUEUE_COUNT, 0, 1),
//		}
//	},
	{AGENT_ACCEPT_COUNT, "network - accept reqs", SA_COUNT, SU_INT },
	{AGENT_CONN_COUNT, "network - cur connect", SA_VALUE, SU_INT},

	{ INDEX_SUGGEST_CNT, "index - suggest reqs", SA_COUNT, SU_INT },
	{ INDEX_SEARCH_CNT, "index - search reqs", SA_COUNT, SU_INT },
	{ INDEX_CLICK_INFO_CNT, "index - click info reqs", SA_COUNT, SU_INT },
	{ INDEX_RELATE_CNT, "index - relate reqs", SA_COUNT, SU_INT },
	{ INDEX_HOT_QUERY_CNT, "index - hot query reqs", SA_COUNT, SU_INT },
	{ INDEX_UNKNOWN_CMD_CNT, "index - unknown cmd reqs", SA_COUNT, SU_INT },
	{ INDEX_SEARCH_HIT_CACHE, "index - search hit cache", SA_COUNT, SU_INT },
	{ INDEX_REQ_ERROR_CNT, "index - error count", SA_COUNT, SU_INT },

	{ INDEX_TOTAL_REQUEST, "index - total requests", SA_COUNT, SU_INT },


//        { SERVER_READONLY,	"server - readonly",SA_CONST, SU_BOOL	},
//        { SERVER_OPENNING_FD,	"server - openning fd",SA_CONST, SU_INT },
//	{ SUPER_GROUP_ENABLE, "server - super_group enable",SA_CONST, SU_BOOL	},

	/************************** ttc ***********************/
//	{ TTC_CACHE_SIZE,	"cache - mem size",		SA_CONST, SU_INT	},
//	{ TTC_CACHE_KEY,	"cache - shm key",		SA_CONST, SU_INT	},
//	{ TTC_CACHE_VERSION,	"cache - version",	SA_CONST, SU_VERSION	},
//	{ TTC_UPDATE_MODE,	"cache - sync mode",	SA_CONST, SU_BOOL	},
//	{ TTC_EMPTY_FILTER,	"cache - empty filter",	SA_CONST, SU_BOOL	},
//
//	{ TTC_USED_NGS,		"cache - total NGs", 	SA_VALUE, SU_INT	},
//	{ TTC_USED_NODES,	"cache - total nodes", 	SA_VALUE, SU_INT	},
//	{ TTC_DIRTY_NODES,	"cache - dirty nodes", 	SA_VALUE, SU_INT	},
//	{ TTC_USED_ROWS, 	"cache - total rows",	SA_VALUE, SU_INT	},
//	{ TTC_DIRTY_ROWS,	"cache - dirty rows",	SA_VALUE, SU_INT	},
//	{ TTC_BUCKET_TOTAL,	"cache - total bucket",	SA_CONST, SU_INT	},
//	{ TTC_FREE_BUCKET,	"cache - free bucket",	SA_VALUE, SU_INT	},
//	{ TTC_DIRTY_AGE,	"cache - dirty age",	SA_VALUE, SU_TIME	},
//	{ TTC_DIRTY_ELDEST,	"cache - dirty eldest",	SA_CONST, SU_DATETIME	}, // no really const
//
//	{ TTC_CHUNK_TOTAL,      "cache - chunk total",  SA_VALUE, SU_INT        },
//	{ TTC_DATA_SIZE,        "cache - data bytes",   SA_VALUE, SU_INT        },
//	{ TTC_DATA_EFF, "cache - data efficiency",      SA_EXPR,  SU_PERCENT_2, 1, 1,
//		{
//			EXPR_IDV(TTC_DATA_SIZE, 0, 10000),
//			EXPR_IDV(TTC_CACHE_SIZE, 0, 1)
//		}
//	},
//	{ TTC_DROP_COUNT,	"cache - drop nodes",	SA_COUNT, SU_INT	},
//	{ TTC_DROP_ROWS,	"cache - drop rows",	SA_COUNT, SU_INT	},
//	{ TTC_FLUSH_COUNT,	"cache - flush nodes",	SA_COUNT, SU_INT	},
//	{ TTC_FLUSH_ROWS,	"cache - flush rows",	SA_COUNT, SU_INT	},
//	{ TTC_GET_COUNT,	"cache - select reqs",	SA_COUNT, SU_INT	},
//	{ TTC_GET_HITS,		"cache - select hits",	SA_COUNT, SU_INT	},
//	{ TTC_INSERT_COUNT,	"cache - insert reqs",	SA_COUNT, SU_INT	},
//	{ TTC_INSERT_HITS,	"cache - insert hits",	SA_COUNT, SU_INT	},
//	{ TTC_UPDATE_COUNT,	"cache - update reqs",	SA_COUNT, SU_INT	},
//	{ TTC_UPDATE_HITS,	"cache - update hits",	SA_COUNT, SU_INT	},
//	{ TTC_DELETE_COUNT,	"cache - delete reqs",	SA_COUNT, SU_INT	},
//	{ TTC_DELETE_HITS,	"cache - delete hits",	SA_COUNT, SU_INT	},
//	{ TTC_PURGE_COUNT,	"cache - purge reqs",	SA_COUNT, SU_INT	},
//
//	{ TTC_HIT_RATIO,	"cache - select hit ratio",	SA_EXPR,  SU_PERCENT_2, 1, 1, {
//		EXPR_IDV(TTC_GET_HITS, 0, 10000),
//		EXPR_IDV(TTC_GET_COUNT, 0, 1),
//	}},
//	{ TTC_ASYNC_RATIO,	"cache - async hit ratio",	SA_EXPR,  SU_PERCENT_2, 5, 3, {
//		EXPR_IDV(TTC_INSERT_HITS, 0, 10000),
//		EXPR_IDV(TTC_UPDATE_HITS, 0, 10000),
//		EXPR_IDV(TTC_DELETE_HITS, 0, 10000),
//		EXPR_IDV(TTC_FLUSH_ROWS, 0, -10000),
//		EXPR_IDV(TTC_DROP_ROWS, 0, -10000),
//		EXPR_IDV(TTC_INSERT_COUNT, 0, 1),
//		EXPR_IDV(TTC_UPDATE_COUNT, 0, 1),
//		EXPR_IDV(TTC_DELETE_COUNT, 0, 1),
//	}},
//
//	{ TTC_SQL_USEC_ALL,	"helper usec - ALL",	SA_SAMPLE, SU_USEC, 0, 0,
//		{ 100, 200, 300, 400, 500, 600, 800, 1200, 2000, 10000, 20000, 100000, 200000, 1000000, 2000000, 10000000} },
//	{ TTC_SQL_USEC_GET,	"helper usec - select",	SA_SAMPLE, SU_USEC	},
//	{ TTC_SQL_USEC_INS,	"helper usec - insert",	SA_SAMPLE, SU_USEC	},
//	{ TTC_SQL_USEC_UPD,	"helper usec - update",	SA_SAMPLE, SU_USEC	},
//	{ TTC_SQL_USEC_DEL,	"helper usec - delete",	SA_SAMPLE, SU_USEC	},
//	{ TTC_SQL_USEC_FLUSH,	"helper usec - flush",	SA_SAMPLE, SU_USEC	},
//
//    { TTC_FORWARD_USEC_ALL,	"forward usec - ALL",	SA_SAMPLE, SU_USEC, 0, 0,
//		{ 100, 200, 300, 400, 500, 600, 800, 1200, 2000, 10000, 20000, 100000, 200000, 1000000, 2000000, 10000000} },
//	{ TTC_FORWARD_USEC_GET,	"forward usec - select",	SA_SAMPLE, SU_USEC	},
//	{ TTC_FORWARD_USEC_INS,	"forward usec - insert",	SA_SAMPLE, SU_USEC	},
//	{ TTC_FORWARD_USEC_UPD,	"forward usec - update",	SA_SAMPLE, SU_USEC	},
//	{ TTC_FORWARD_USEC_DEL,	"forward usec - delete",	SA_SAMPLE, SU_USEC	},
//	{ TTC_FORWARD_USEC_FLUSH, "forward usec - flush",	SA_SAMPLE, SU_USEC	},
//
//	{ TTC_EMPTY_NODES,	"cache - empty nodes", 	SA_VALUE, SU_INT	},
//	{ TTC_MEMORY_TOP,	"cache - memory topline", 	SA_VALUE, SU_INT	},
//	{ TTC_MAX_FLUSH_REQ,	"cache - max flush request", 	SA_VALUE, SU_INT	},
//	{ TTC_CURR_FLUSH_REQ,	"cache - curr flush request", 	SA_VALUE, SU_INT	},
//	{ TTC_OLDEST_DIRTY_TIME,	"cache - oldest dirty node time", 	SA_VALUE, SU_INT	},
//
//    /***********************purge alerting************************/
//    {LAST_PURGE_NODE_MOD_TIME,    "cache - last purge data modify",   SA_CONST, SU_DATETIME},
//    {DATA_EXIST_TIME,    "cache - data exist time",   SA_CONST,SU_INT },
//    {DATA_SIZE_AVG_RECENT,    "cache - average alloc(recently)", SA_VALUE,SU_INT },
//    {TTC_ASYNC_FLUSH_COUNT, "cache - flush db",  SA_COUNT, SU_INT },
//
//	/***************** key expire time **************************/
//	{ DTC_KEY_EXPIRE_USER_COUNT, "cache - user key expire count",SA_COUNT, SU_INT        },
//	{ DTC_KEY_EXPIRE_DTC_COUNT, "cache - dtc key expire count",SA_COUNT, SU_INT        },

	/************************** bitmapsvr ***********************/
//	{ BTM_INDEX_1,          "Mem - index(1)",SA_COUNT, SU_INT        },
//	{ BTM_INDEX_2,          "Mem - index(2)",SA_COUNT, SU_INT        },
//	{ BTM_INDEX_3,          "Mem - index(3)",SA_COUNT, SU_INT        },
//	{ BTM_DATA,             "Mem - data",    SA_COUNT, SU_INT        },
//	{ BTM_DATA_DELETE,      "Mem - delete",  SA_COUNT, SU_INT        },

//	{HBP_LRU_SCAN_TM,	"hbp - lru scan time(msec)",  SA_SAMPLE,SU_MSEC},
//        {HBP_LRU_TOTAL_BITS,	"hbp - lru total bits",    SA_VALUE,    SU_INT},
//        {HBP_LRU_TOTAL_1_BITS,	"hbp - lru total 1 bits",  SA_VALUE,	SU_INT},
//        {HBP_LRU_SET_COUNT,	"hbp - lru set op count",  SA_COUNT,	SU_INT},
//        {HBP_LRU_SET_HIT_COUNT,	"hbp - lru set hit count", SA_COUNT, 	SU_INT},
//        {HBP_LRU_CLR_COUNT,	"hbp - lru clr op count",  SA_COUNT, 	SU_INT},

//      {HBP_INC_SYNC_STEP,          "hbp - inc-sync step",     SA_SAMPLE,   SU_INT, 0, 0,
//           { 1, 2, 5, 10, 20, 50, 100, 500, 1000, 2000, 5000, 10000} },

//	{BLACKLIST_CURRENT_SLOT,"blacklist - current count", SA_VALUE, SU_INT},
//	{BLACKLIST_SIZE,        "blacklist - size distribution", SA_SAMPLE,   SU_INT, 0, 0,
//		{ 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144}},
//
//	/* TryPurgeSize ÿ��purge�Ľڵ���� */
//	{TRY_PURGE_COUNT,       "try purge - count distribution", SA_SAMPLE,   SU_INT, 0, 0,
//		{ 10, 20, 40, 80, 120, 200, 400, 800, 1000, 2000, 4000, 8000}},
//	{TRY_PURGE_NODES,       "try purge - auto purged nodes", SA_COUNT,   SU_INT, 0, 0},
//	{PLUGIN_REQ_USEC_ALL,  "request sb usec - ALL",    SA_SAMPLE, SU_USEC, 0, 0,
//		{ 100, 200, 300, 400, 500, 600, 800, 1200, 2000, 10000, 20000, 100000, 200000, 1000000, 2000000, 10000000} },
//
//	{INC_THREAD_CPU_STAT_0, "incomming thread 0 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_1, "incomming thread 1 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_2, "incomming thread 2 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_3, "incomming thread 3 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_4, "incomming thread 4 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_5, "incomming thread 5 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_6, "incomming thread 6 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_7, "incomming thread 7 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_8, "incomming thread 8 cpu", SA_VALUE, SU_PERCENT_2},
//	{INC_THREAD_CPU_STAT_9, "incomming thread 9 cpu", SA_VALUE, SU_PERCENT_2},
//	{CACHE_THREAD_CPU_STAT, "cache thread cpu",       SA_VALUE, SU_PERCENT_2},
//	{DATA_SOURCE_CPU_STAT,  "data source thread cpu", SA_VALUE, SU_PERCENT_2},
//	{WORKER_THREAD_CPU_STAT,"worker thread cpu",      SA_VALUE, SU_PERCENT_2},
//	{DTC_FRONT_BARRIER_COUNT, "front barrier number", SA_VALUE, SU_INT},
//	{DTC_FRONT_BARRIER_MAX_TASK, "front barrier max task number", SA_VALUE, SU_INT},
//	{DTC_BACK_BARRIER_COUNT, "end barrier number", SA_VALUE, SU_INT},
//	{DTC_BACK_BARRIER_MAX_TASK, "end barrier max task number", SA_VALUE, SU_INT},
//	{DATA_SIZE_HISTORY_STAT,        "history data - size distribution", SA_SAMPLE,   SU_INT, 0, 0,
//	{  64, 128, 256, 512, 1024, 2048, 4096}},
//	{ROW_SIZE_HISTORY_STAT,        "history row - size distribution", SA_SAMPLE,   SU_INT, 0, 0,
//		{  1, 2, 3, 4, 8, 16, 32}},
//		{DATA_SURVIVAL_HOUR_STAT,        "data survival time(By hour)", SA_SAMPLE,   SU_INT, 0, 0,
//	{  1, 2, 4, 8, 16, 24, 36, 48, 72, 96, 192, 360, 720, 1440}},
//	{PURGE_CREATE_UPDATE_STAT, "purge size for create and update distribution", SA_SAMPLE, SU_INT, 0, 0,
//		{  100, 200, 400, 800, 1200, 1600, 2000, 2500}},
//	{ HELPER_READ_GROUR_CUR_QUEUE_MAX_SIZE,"read group queue max count",SA_COUNT, SU_INT	},
//	{ HELPER_WRITE_GROUR_CUR_QUEUE_MAX_SIZE,"write group queue max count",SA_COUNT, SU_INT	},
//	{ HELPER_COMMIT_GROUR_CUR_QUEUE_MAX_SIZE,"commit grour queue max count",SA_COUNT, SU_INT	},
//	{ HELPER_SLAVE_READ_GROUR_CUR_QUEUE_MAX_SIZE,"slave read group queue max count",SA_COUNT, SU_INT	},
	{INCOMING_EXPIRE_REQ, "incoming -expire req(send rsp)", SA_COUNT, SU_INT},
//	{CACHE_EXPIRE_REQ, "cache -expire req ", SA_COUNT, SU_INT},
	{DATA_SOURCE_EXPIRE_REQ, "data source -expire req ", SA_COUNT, SU_INT},
	{TASK_CLIENT_TIMEOUT, "task client timeout", SA_VALUE, SU_INT},
	{ 0 }
};
