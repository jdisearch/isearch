/*
 * =====================================================================================
 *
 *       Filename:  tree_data.h
 *
 *    Description:  T-tree data struct operation. For user invoke.
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

#ifndef TREE_DATA_H
#define TREE_DATA_H

#include "raw_data.h"
#include "t_tree.h"
#include "protocol.h"
#include "task_request.h"
#include "value.h"
#include "field.h"
#include "section.h"
#include "table_def.h"

typedef enum _TreeCheckResult
{
	CHK_CONTINUE, // 继续访问这棵子树
	CHK_SKIP,		// 忽略这棵子树，继续访问其他节点
	CHK_STOP,		// 终止访问循环
	CHK_DESTROY	// 销毁这棵子树
} TreeCheckResult;

#define TTREE_INDEX_POS 1

typedef TreeCheckResult (*CheckTreeFunc)(Mallocator &stMalloc, uint8_t uchIndexCnt, uint8_t uchCurIdxCnt, const RowValue *pstIndexValue, const uint32_t uiTreeRowNum, void *pCookie);
typedef int (*VisitRawData)(Mallocator &stMalloc, uint8_t uchIndexCnt, const RowValue *pstIndexValue, ALLOC_HANDLE_T &hHandle, int64_t &llRowNumInc, void *pCookie);
class TreeData;
typedef int (TreeData::*SubRowProcess)(TaskRequest &stTask, MEM_HANDLE_T hRecord);

class DTCFlushRequest;

/************************************************************
  Description:    t-tree根节点的数据结构
  Version:         DTC 3.0
***********************************************************/
struct _RootData
{
	unsigned char m_uchDataType;
	uint32_t m_treeSize;
	uint32_t m_uiTotalRawSize; //所有RawData总和，不包含Header
	uint32_t m_uiNodeCnts;	   //索引T树中Node总计个数
	uint32_t m_uiRowCnt;	   //索引T树中总计行数
	uint8_t m_uchGetCount;
	uint16_t m_LastAccessHour;
	uint16_t m_LastUpdateHour;
	uint16_t m_CreateHour;
	MEM_HANDLE_T m_hRoot;
	char m_achKey[0];
} __attribute__((packed));
typedef struct _RootData RootData;

class DTCTableDefinition;
typedef struct _CmpCookie
{
	const DTCTableDefinition *m_pstTab;
	uint8_t m_uchIdx;
	_CmpCookie(const DTCTableDefinition *pstTab, uint8_t uchIdx)
	{
		m_pstTab = pstTab;
		m_uchIdx = uchIdx;
	}
} CmpCookie;

typedef struct _pCookie
{
	MEM_HANDLE_T *m_handle;
	uint32_t nodesGot; //已经遍历到的节点个数
	uint32_t nodesNum; //需要遍历的节点个数，0代表不限
	uint32_t rowsGot;  //已经遍历到的数据行数
	_pCookie() : m_handle(NULL), nodesGot(0), nodesNum(0), rowsGot(0) {}
} pResCookie;

typedef enum _CondType
{
	COND_VAL_SET, // 查询特定的值列表
	COND_RANGE,	// 查询value[0] ~ Key-value[0]<=value[1].s64
	COND_GE,		// 查询大于等于value[0]的key
	COND_LE,		// 查询小于等于value[0]的key
	COND_ALL		// 遍历所有key
}CondType;

typedef enum _Order
{
	ORDER_ASC, // 升序
	ORDER_DEC, // 降序
	ORDER_POS, // 后序访问
} Order;

/************************************************************
  Description:    查找数据的条件
  Version:         DTC 3.0
***********************************************************/
typedef struct
{
	unsigned char m_uchCondType;
	unsigned char m_uchOrder;
	unsigned int m_uiValNum;
	DTCValue *m_pstValue;
} TtreeCondition;

class TreeData
{
private:
	RootData *m_pstRootData; // 注意：地址可能会因为realloc而改变
	Ttree m_stTree;
	DTCTableDefinition *m_pstTab;
	uint8_t m_uchIndexDepth;
	int m_iTableIdx;
	char m_szErr[100];

	ALLOC_SIZE_T m_uiNeedSize; // 最近一次分配内存失败需要的大小
	uint64_t m_ullAffectedrows;

	MEM_HANDLE_T _handle;
	uint32_t _size;
	uint32_t _root_size;
	Mallocator *_mallocator;
	Node *m_pstNode;
	bool m_async;
	int64_t m_llRowsInc;
	int64_t m_llDirtyRowsInc;

	int m_iKeySize;
	uint8_t m_uchKeyIdx;
	int m_iExpireId;
	int m_iLAId;
	int m_iLCmodId;
	ALLOC_SIZE_T m_uiLAOffset;

	ALLOC_SIZE_T m_uiOffset;
	ALLOC_SIZE_T m_uiRowOffset;
	char *m_pchContent;

	bool m_IndexPartOfUniqField;
	MEM_HANDLE_T m_hRecord;

	/************************************************************
	  Description:    递归查找数据的cookie参数
	  Version:         DTC 3.0
	***********************************************************/
	typedef struct
	{
		TreeData *m_pstTree;
		uint8_t m_uchCondIdxCnt;
		uint8_t m_uchCurIndex;
		MEM_HANDLE_T m_hHandle;
		int64_t m_llAffectRows;
		const int *piInclusion;
		KeyComparator m_pfComp;
		const RowValue *m_pstCond;
		RowValue *m_pstIndexValue;
		VisitRawData m_pfVisit;
		void *m_pCookie;
	} CIndexCookie;

	typedef struct
	{
		TreeData *m_pstTree;
		uint8_t m_uchCurCond;
		MEM_HANDLE_T m_hHandle;
		int64_t m_llAffectRows;
		const TtreeCondition *m_pstCond;
		KeyComparator m_pfComp;
		RowValue *m_pstIndexValue;
		CheckTreeFunc m_pfCheck;
		VisitRawData m_pfVisit;
		void *m_pCookie;
	} CSearchCookie;

	int set_data_size(unsigned int data_size);
	int set_row_count(unsigned int count);
	unsigned int get_data_size();
	unsigned int get_row_count();

protected:
	template <class T>
	T *Pointer(void) const { return reinterpret_cast<T *>(_mallocator->handle_to_ptr(_handle)); }

	template <class T>
	T *Pointer(MEM_HANDLE_T handle) const { return reinterpret_cast<T *>(_mallocator->handle_to_ptr(handle)); }

	int encode_to_private_area(RawData &raw, RowValue &value, unsigned char value_flag);

	inline int pack_key(const RowValue &stRow, uint8_t uchKeyIdx, int &iKeySize, char *&pchKey, unsigned char achKeyBuf[]);
	inline int pack_key(const DTCValue *pstVal, uint8_t uchKeyIdx, int &iKeySize, char *&pchKey, unsigned char achKeyBuf[]);
	inline int unpack_key(char *pchKey, uint8_t uchKeyIdx, RowValue &stRow);

	int insert_sub_tree(uint8_t uchCurIndex, uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T hRoot);
	int insert_sub_tree(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T hRoot);
	int insert_sub_tree(uint8_t uchCondIdxCnt, KeyComparator pfComp, ALLOC_HANDLE_T hRoot);
	int insert_row_flag(uint8_t uchCurIndex, const RowValue &stRow, KeyComparator pfComp, unsigned char uchFlag);
	int Find(CIndexCookie *pstIdxCookie);
	int Find(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T &hRecord);
	int Find(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T *&hRecord);
	static int search_visit(Mallocator &stMalloc, ALLOC_HANDLE_T &hRecord, void *pCookie);
	int Search(CSearchCookie *pstSearchCookie);
	int Delete(CIndexCookie *pstIdxCookie);
	int Delete(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T &hRecord);

public:
	TreeData(Mallocator *pstMalloc);
	~TreeData();

	const char *get_err_msg() { return m_szErr; }
	MEM_HANDLE_T get_handle() { return _handle; }
	int Attach(MEM_HANDLE_T hHandle);
	int Attach(MEM_HANDLE_T hHandle, uint8_t uchKeyIdx, int iKeySize, int laid = -1, int lcmodid = -1, int expireid = -1);

	const MEM_HANDLE_T get_tree_root() const { return m_stTree.Root(); }

	/*************************************************
	  Description:	新分配一块内存，并初始化
	  Input:		 iKeySize	key的格式，0为变长，非0为定长长度
				pchKey	为格式化后的key，变长key的第0字节为长度
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int Init(int iKeySize, const char *pchKey);
	int Init(uint8_t uchKeyIdx, int iKeySize, const char *pchKey, int laId = -1, int expireId = -1, int nodeIdx = -1);
	int Init(const char *pchKey);

	const char *Key() const { return m_pstRootData ? m_pstRootData->m_achKey : NULL; }
	char *Key() { return m_pstRootData ? m_pstRootData->m_achKey : NULL; }

	unsigned int total_rows() { return m_pstRootData->m_uiRowCnt; }
	uint64_t get_affectedrows() { return m_ullAffectedrows; }
	void set_affected_rows(int num) { m_ullAffectedrows = num; }

	/*************************************************
	  Description:	最近一次分配内存失败所需要的内存大小
	  Input:		
	  Output:		
	  Return:		返回所需要的内存大小
	*************************************************/
	ALLOC_SIZE_T need_size() { return m_uiNeedSize; }

	/*************************************************
	  Description:	销毁uchLevel以及以下级别的子树
	  Input:		uchLevel	销毁uchLevel以及以下级别的子树，显然uchLevel应该在1到uchIndexDepth之间
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	//	int Destroy(uint8_t uchLevel=1);
	int Destroy();

	/*************************************************
	  Description:	插入一行数据
	  Input:		stRow	包含index字段以及后面字段的值
				pfComp	用户自定义的key比较函数
				uchFlag	行标记
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int insert_row_flag(const RowValue &stRow, KeyComparator pfComp, unsigned char uchFlag);

	/*************************************************
	  Description:	插入一行数据
	  Input:		stRow	包含index字段以及后面字段的值
				pfComp	用户自定义的key比较函数
				isDirty	是否脏数据
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int insert_row(const RowValue &stRow, KeyComparator pfComp, bool isDirty);

	/*************************************************
	  Description:	查找一行数据
	  Input:		stCondition	包含各级index字段的值
				pfComp	用户自定义的key比较函数
				
	  Output:		hRecord	查找到的一个指向CRawData的句柄
	  Return:		0为找不到，1为找到数据
	*************************************************/
	int Find(const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T &hRecord);

	/*************************************************
	  Description:	按索引条件查找
	  Input:		pstCond	一个数组，而且大小刚好是uchIndexDepth
				pfComp	用户自定义的key比较函数
				pfVisit	当查找到记录时，用户自定义的访问数据函数
				pCookie	访问数据函数使用的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int Search(const TtreeCondition *pstCond, KeyComparator pfComp, VisitRawData pfVisit, CheckTreeFunc pfCheck, void *pCookie);

	/*************************************************
	  Description:	从小到大遍历所有数据
	  Input:		pfComp	用户自定义的key比较函数
				pfVisit	当查找到记录时，用户自定义的访问数据函数
				pCookie	访问数据函数使用的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_forward(KeyComparator pfComp, VisitRawData pfVisit, void *pCookie);

	/*************************************************
	  Description:	根据指定的index值，删除符合条件的所有行（包括子树）
	  Input:		uchCondIdxCnt	条件index的数量
				stCondition		包含各级index字段的值
				pfComp		用户自定义的key比较函数
				
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int delete_sub_row(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp);

	/*************************************************
	  Description:	将某个级别的index值修改为另外一个值
	  Input:		uchCondIdxCnt	条件index的数量
				stCondition		包含各级index字段的值
				pfComp		用户自定义的key比较函数
				pstNewValue	对应最后一个条件字段的新index值
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int update_index(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, const DTCValue *pstNewValue);
	unsigned ask_for_destroy_size(void);

	DTCTableDefinition *get_node_table_def() { return m_pstTab; }

	void change_mallocator(Mallocator *pstMalloc)
	{
		_mallocator = pstMalloc;
	}

	int expand_tree_chunk(MEM_HANDLE_T *pRecord, ALLOC_SIZE_T tExpSize);

	/*************************************************
	  Description:	destroy data in t-tree
	  Output:		
	*************************************************/
	int destroy_sub_tree();

	/*************************************************
	  Description:	copy data from raw to t-tree
	  Output:		
	*************************************************/
	int copy_tree_all(RawData *pstRawData);

	/*************************************************
	  Description:	copy data from t-tree to raw
	  Output:		
	*************************************************/
	int copy_raw_all(RawData *pstRawData);

	/*************************************************
	  Description:	get tree data from t-tree
	  Output:		
	*************************************************/
	int decode_tree_row(RowValue &stRow, unsigned char &uchRowFlags, int iDecodeFlag = 0);

	/*************************************************
	  Description:	set tree data from t-tree
	  Output:		
	*************************************************/
	int encode_tree_row(const RowValue &stRow, unsigned char uchOp);

	/*************************************************
	  Description: compare row data value	
	  Output:		
	*************************************************/
	int compare_tree_data(RowValue *stpNodeRow);

	/*************************************************
	  Description:	get data in t-tree
	  Output:		
	*************************************************/
	int get_tree_data(TaskRequest &stTask);

	/*************************************************
	  Description:	flush data in t-tree
	  Output:		
	*************************************************/
	int flush_tree_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt);

	/*************************************************
	  Description:	get data in t-tree
	  Output:		
	*************************************************/
	int delete_tree_data(TaskRequest &stTask);

	/*************************************************
	  Description:	获得T树中的Raw类型的每一行的数据
	  Output:		
	*************************************************/
	int get_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord);

	/*************************************************
	  Description:	删除T树中的Raw类型的行的数据
	  Output:		
	*************************************************/
	int delete_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord);

	/*************************************************
	  Description:	修改T树中的Raw类型的行的数据
	  Output:		
	*************************************************/
	int update_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord);

	/*************************************************
	  Description:	替换T树中的Raw类型的行的数据，如没有此行则创建
	  Output:		
	*************************************************/
	int replace_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord);

	/*************************************************
	  Description:	处理T树中平板类型业务
	  Output:		
	*************************************************/
	int get_sub_raw(TaskRequest &stTask, unsigned int nodeCnt, bool isAsc, SubRowProcess subRowProc);

	/*************************************************
	  Description:	匹配索引
	  Output:		
	*************************************************/
	int match_index_condition(TaskRequest &stTask, unsigned int rowCnt, SubRowProcess subRowProc);

	/*************************************************
	  Description:	update data in t-tree
	  Output:		
	*************************************************/
	int update_tree_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows);

	/*************************************************
	  Description:	replace data in t-tree
	  Output:		
	*************************************************/
	int replace_tree_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, unsigned char &RowFlag, bool setrows);

	/*************************************************
	  Description:	calculate row data size
	  Output:		
	*************************************************/
	ALLOC_SIZE_T calc_tree_row_size(const RowValue &stRow, int keyIdx);

	/*************************************************
	  Description:	get expire time
	  Output:		
	*************************************************/
	int get_expire_time(DTCTableDefinition *t, uint32_t &expire);

	/*************************************************
	  Description:	替换当前行
	  Input:		stRow	仅使用row的字段类型等信息，不需要实际数据
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int replace_cur_row(const RowValue &stRow, bool isDirty, MEM_HANDLE_T *hRecord);

	/*************************************************
	  Description:	删除当前行
	  Input:		stRow	仅使用row的字段类型等信息，不需要实际数据
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int delete_cur_row(const RowValue &stRow);

	/*************************************************
	  Description:	调到下一行
	  Input:		stRow	仅使用row的字段类型等信息，不需要实际数据
	  Output:		m_uiOffset会指向下一行数据的偏移
	  Return:		0为成功，非0失败
	*************************************************/
	int skip_row(const RowValue &stRow);

	/*************************************************
    Description: 
    Output: 
    *************************************************/
	int64_t dirty_rows_inc() { return m_llDirtyRowsInc; }

	/*************************************************
	  Description:	查询本次操作增加的行数（可以为负数）
	  Input:		
	  Output:		
	  Return:		行数
	*************************************************/
	int64_t rows_inc() { return m_llRowsInc; }

	int set_cur_row_flag(unsigned char uchFlag);

	int dirty_rows_in_node();
};

#endif
