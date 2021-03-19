/*
 * =====================================================================================
 *
 *       Filename:  task_const.cc
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
#include "task_base.h"

// convert stage to result code
const DecodeResult DTCTask::stage2result[] = {
	/*DecodeStageFatalError */ DecodeFatalError,
	/*DecodeStageDataError  */ DecodeDataError,
	/*DecodeStageIdle       */ DecodeIdle,
	/*DecodeStageWaitHeader */ DecodeWaitData,
	/*DecodeStageWaitData   */ DecodeWaitData,
	/*DecodeStageDiscard    */ DecodeWaitData,
	/*DecodeStageDone       */ DecodeDone,
};

const uint32_t DTCTask::validcmds[] = {
	//00000001 NOP
	//00000002 RESULTCODE
	//00000004 RESULTSET
	//00000008 SVRADMIN
	//00000010 GET
	//00000020 PURGE
	//00000040 INSERT
	//00000080 UPDATE
	//00000100 DELETE
	//00000200 OBSOLETED
	//00000400 OBSOLETED
	//00000800 OBSOLETED
	//00001000 REPLACE
	//00002000 FLUSH
	//00004000 Invalidate
	//00008000 bit15
	0x000071F9 /* Client --> Server/Helper */,
	0x00000006 /* Server --> Client */,
	0x00000006 /* Helper --> Server */,
};

const uint16_t DTCTask::cmd2type[] = {
	TaskTypeAdmin,				// NOP
	TaskTypeAdmin,				// RESULTCODE
	TaskTypeAdmin,				// RESULTSET
	TaskTypeAdmin,				// SVRADMIN
	TaskTypeRead,				// GET
	TaskTypeAdmin,				// PURGE
	TaskTypeWrite,				// INSERT
	TaskTypeWrite,				// UPDATE
	TaskTypeWrite,				// DELETE
	TaskTypeAdmin,				// OBSOLETED
	TaskTypeAdmin,				// OBSOLETED
	TaskTypeAdmin,				// OBSOLETED
								//	TaskTypeCommit, // REPLACE
	TaskTypeWrite,				// REPLACE
	TaskTypeWrite,				// Flush
	TaskTypeWrite,				// Invalidate
	TaskTypeAdmin,				// Monitor,此条是为了占位
	TaskTypeHelperReloadConfig, //Reload
};

const uint16_t DTCTask::validsections[][2] = {
	//0001 VersionInfo
	//0002 DataDefinition
	//0004 RequestInfo
	//0008 ResultInfo
	//0010 UpdateInfo
	//0020 ConditionInfo
	//0040 FieldSet
	//0080 DTCResultSet
	{0x0000, 0x0005}, // NOP
	{0x0008, 0x000B}, // RESULTCODE
	{0x0089, 0x008B}, // RESULTSET
	{0x0005, 0x0075}, // SVRADMIN
	{0x0005, 0x0065}, // GET
	{0x0005, 0x0025}, // PURGE
	{0x0001, 0x0015}, // INSERT
	{0x0015, 0x0035}, // UPDATE
	{0x0005, 0x0025}, // DELETE
	{0x0001, 0x0051}, // OBSOLETED
	{0x0015, 0x0055}, // OBSOLETED
	{0x0005, 0x0045}, // OBSOLETED
	{0x0001, 0x0015}, // REPLACE
	{0x0005, 0x0025}, // Flush
	{0x0005, 0x0025}, // Invalidate
};

/* [clientkey][serverkey] */
const uint8_t DTCTask::validktype[DField::TotalType][DField::TotalType] = {
	/* -, d, u, f, s, b */
	{0, 0, 0, 0, 0, 0}, /* - */
	{0, 1, 1, 0, 0, 0}, /* d */
	{0, 1, 1, 0, 0, 0}, /* u */
	{0, 0, 0, 0, 0, 0}, /* f */
	{0, 0, 0, 0, 1, 1}, /* s */
	{0, 0, 0, 0, 1, 1}, /* b */
};

#if 0
/* [fieldtype][op] */
const uint8_t DTCTask::validops[DField::TotalType][DField::TotalOperation] = {
	/* =, +, b */
	{  0, 0, 0 }, /* - */
	{  1, 1, 1 }, /* d */
	{  1, 1, 1 }, /* u */
	{  1, 1, 0 }, /* f */
	{  1, 0, 1 }, /* s */
	{  1, 0, 1 }, /* b */
};
#endif

/* [fieldtype][ valuetype] */
const uint8_t DTCTask::validxtype[DField::TotalOperation][DField::TotalType][DField::TotalType] = {
	//set
	{
		/* -, d, u, f, s, b */
		{0, 0, 0, 0, 0, 0}, /* - */
		{0, 1, 1, 0, 0, 0}, /* d */
		{0, 1, 1, 0, 0, 0}, /* u */
		{0, 1, 1, 1, 0, 0}, /* f */
		{0, 0, 0, 0, 1, 0}, /* s */
		{0, 0, 0, 0, 1, 1}, /* b */
	},
	//add
	{
		/* -, d, u, f, s, b */
		{0, 0, 0, 0, 0, 0}, /* - */
		{0, 1, 1, 0, 0, 0}, /* d */
		{0, 1, 1, 0, 0, 0}, /* u */
		{0, 1, 1, 1, 0, 0}, /* f */
		{0, 0, 0, 0, 0, 0}, /* s */
		{0, 0, 0, 0, 0, 0}, /* b */
	},
	//setbits
	{
		/* -, d, u, f, s, b */
		{0, 0, 0, 0, 0, 0}, /* - */
		{0, 1, 1, 0, 0, 0}, /* d */
		{0, 1, 1, 0, 0, 0}, /* u */
		{0, 0, 0, 0, 0, 0}, /* f */
		{0, 1, 1, 0, 0, 0}, /* s */
		{0, 1, 1, 0, 0, 0}, /* b */
	},
	//OR
	{
		/* -, d, u, f, s, b */
		{0, 0, 0, 0, 0, 0}, /* - */
		{0, 1, 1, 0, 0, 0}, /* d */
		{0, 1, 1, 0, 0, 0}, /* u */
		{0, 0, 0, 0, 0, 0}, /* f */
		{0, 0, 0, 0, 0, 0}, /* s */
		{0, 0, 0, 0, 0, 0}, /* b */
	},
};

/* [fieldtype][cmp] */
const uint8_t DTCTask::validcomps[DField::TotalType][DField::TotalComparison] = {
	/* EQ,NE,LT,LE,GT,GE */
	{0, 0, 0, 0, 0, 0}, /* - */
	{1, 1, 1, 1, 1, 1}, /* d */
	{1, 1, 1, 1, 1, 1}, /* u */
	{0, 0, 0, 0, 0, 0}, /* f */
	{1, 1, 0, 0, 0, 0}, /* s */
	{1, 1, 0, 0, 0, 0}, /* b */
};
