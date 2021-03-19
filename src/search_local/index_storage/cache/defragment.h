/*
 * =====================================================================================
 *
 *       Filename:  defragment.h
 *
 *    Description:  memory clear up.
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
#include "pt_malloc.h"
#include "dtc_api.h"

class Defragment
{
public:
    Defragment()
    {
        _mem = NULL;
        _pstChunk = NULL;
        _keysize = -1;
        _s = NULL;
        _error_count = 0;
        _skip_count = 0;
        _ok_count = 0;
        _bulk_per_ten_microscoends = 1;
    }

    ~Defragment()
    {
    }
    int Attach(const char *key, int keysize, int step);
    char *get_key_by_handle(INTER_HANDLE_T handle, int *len);
    int proccess(INTER_HANDLE_T handle);
    int dump_mem(bool verbose = false);
    int dump_mem_new(const char *filename, uint64_t &memsize);
    int defrag_mem(int level, DTC::Server *s);
    int defrag_mem_new(int level, DTC::Server *s, const char *filename, uint64_t memsize);
    int proccess_handle(const char *filename, DTC::Server *s);
    void frequency_limit(void);

private:
    DTCBinMalloc *_mem;
    MallocChunk *_pstChunk;
    int _keysize;
    DTC::Server *_s;

    //stat
    uint64_t _error_count;
    uint64_t _skip_count;
    uint64_t _ok_count;
    int _bulk_per_ten_microscoends;
};

#define SEARCH 0
#define MATCH 1
class DefragMemAlgo
{
public:
    DefragMemAlgo(int level, Defragment *master);
    ~DefragMemAlgo();
    int Push(INTER_HANDLE_T handle, int used);

private:
    int _status;
    INTER_HANDLE_T *_queue;
    Defragment *_master;
    int _count;
    int _level;
};
