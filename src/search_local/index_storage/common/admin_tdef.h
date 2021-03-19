/*
 * =====================================================================================
 *
 *       Filename:  admin_tdef.h
 *
 *    Description:  table definition.
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
#ifndef __ADMIN_TDEF_H__
#define __ADMIN_TDEF_H__

class DTCTableDefinition;
class DTCHotBackup
{
public:
    //type
    enum
    {
        SYNC_LRU = 1,
        SYNC_INSERT = 2,
        SYNC_UPDATE = 4,
        SYNC_PURGE = 8,
        SYNC_DELETE = 16,
        SYNC_CLEAR = 32,
        SYNC_COLEXPAND = 64,
        SYNC_COLEXPAND_CMD = 128
    };

    //flag
    enum
    {
        NON_VALUE = 1,
        HAS_VALUE = 2,
        EMPTY_NODE = 4,
        KEY_NOEXIST = 8,
    };
};

class DTCMigrate
{
public:
    //type
    enum
    {
        FROM_CLIENT = 1,
        FROM_SERVER = 2
    };

    //flag
    enum
    {
        NON_VALUE = 1,
        HAS_VALUE = 2,
        EMPTY_NODE = 4,
        KEY_NOEXIST = 8,
    };
};
extern DTCTableDefinition *build_hot_backup_table(void);

#define _DTC_HB_COL_EXPAND_ "_dtc_hb_col_expand_"
#define _DTC_HB_COL_EXPAND_DONE_ "_dtc_hb_col_expand_done_"

#endif
