/*
 * =====================================================================================
 *
 *       Filename:  dtc_global.h
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
#ifndef _DTC_GLOBAL_H_
#define _DTC_GLOBAL_H_
#include "non_copyable.h"

#define TABLE_CONF_NAME "../conf/table.conf"
#define CACHE_CONF_NAME "../conf/cache.conf"
#define ALARM_CONF_FILE "../conf/dtcalarm.conf"
class DTCGlobal : private noncopyable
{
public:
    DTCGlobal(void);
    ~DTCGlobal(void);

public:
    static int _pre_alloc_NG_num;
    static int _min_chunk_size;
    static int _pre_purge_nodes;
};
#endif
