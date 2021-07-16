
/*
 * =====================================================================================
 *
 *       Filename:  db_process_rocks.h 
 *
 *    Description:  Rocksdb invoker
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

#ifndef __DB_PROCESS_ROCKS_H__
#define __DB_PROCESS_ROCKS_H__

#include "db_process_base.h"
#include "rocksdb_conn.h"
#include "key_format.h"
#include "rocksdb_key_comparator.h"
#include "rocksdb_direct_context.h"
#include "rocksdb_orderby_unit.h"

class RocksdbProcess : public HelperProcessBase 
{
  private:
    int	ErrorNo;
    RocksDBConn* mDBConn;

    char	name[16];
    char	title[80];
    int	titlePrefixSize;

    DTCTableDefinition* mTableDef;
    int mCompoundKeyFieldNums;
    int mExtraValueFieldNums;
    std::vector<uint8_t> mFieldIndexMapping; // DTC field index map to rocksdb, index is 
            // the fieldid in rocksdb, and the value is the fieldid in DTC
    std::vector<uint8_t> mReverseFieldIndexMapping; // rocksdb idx map to dtc, value is rocks idx
    std::vector<int> mKeyfield_types;
    bool mNoBitmapKey;

    int	SelfGroupID;
    unsigned int procTimeout;
    
    // denoted finished key for replication
    std::string mReplEndKey;

    // for migrating
    std::string mPrevMigrateKey;    // rocks key without suffix
    std::string mCurrentMigrateKey; 
    int64_t mUncommitedMigId;
    
    RocksdbOrderByUnit* mOrderByUnit;

  // statistic info
  enum class TimeZone : unsigned char
  {
    eStatLevel0, // < 1ms
    eStatLevel1, // [1ms, 2ms)
    eStatLevel2, // < [2ms, 3ms)
    eStatLevel3, // < [3ms, 4ms)
    eStatLevel4, // < [4ms, 5ms)
    eStatLevel5, // >= 5ms
    eStatMax 
  };
  
  enum class OprType : unsigned char
  {
    eInsert,
    eUpdate,
    eDirectQuery,
    eQuery,
    eReplace,
    eDelete
  };

  std::vector<std::vector<int> > mOprTimeCost = {
    {0, 0, 0, 0, 0, 0}, // OprType::eInsert
    {0, 0, 0, 0, 0, 0}, // update
    {0, 0, 0, 0, 0, 0}, // direct query 
    {0, 0, 0, 0, 0, 0}  // query
  };
  int64_t mSTime;
  int64_t mETime;
  int64_t mTotalOpr;

  public:
    RocksdbProcess(RocksDBConn *conn);
    virtual ~RocksdbProcess();

    int Init(int GroupID, const DbConfig* Config, DTCTableDefinition *tdef, int slave);
    int check_table();

    int process_task (DTCTask* Task);

    void init_title(int m, int t);
    void set_title(const char *status);
    const char *Name(void) { return name; }
    void set_proc_timeout(unsigned int Seconds) { procTimeout = Seconds; }
  
    int process_direct_query(
        DirectRequestContext* reqCxt,
        DirectResponseContext* respCxt); 
  
  protected:
    void init_ping_timeout(void);
	void use_matched_rows(void);	

    inline int value2Str(const DTCValue* Value, int fieldId, std::string &strValue);
    inline int setdefault_value(int field_type, DTCValue& Value);
    inline int str2Value(const std::string &str, int field_type, DTCValue& Value);
    std::string valueToStr(const DTCValue *value, int fieldType);

    int saveRow(
        const std::string &compoundKey, 
        const std::string &compoundValue,
        bool countOnly,
        int &totalRows,
        DTCTask* Task);

    int save_direct_row(
        const std::string &prefixKey,
        const std::string &compoundKey, 
        const std::string &compoundValue,
        DirectRequestContext* reqCxt,
        DirectResponseContext* respCxt,
        int &totalRows);
    
    void build_direct_row(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values,
        DirectResponseContext* respCxt);

    int update_row(
        const std::string &prefixKey,
        const std::string &compoundKey, 
        const std::string &compoundValue,
        DTCTask* Task,
        std::string &newKey,
        std::string &newValue);

    int whether_update_key(
        const DTCFieldValue* UpdateInfo,
        bool &updateKey,
        bool &updateValue);

    int shrink_value(
        const std::vector<std::string> &values,
        std::string &rocksValue);

    int split_values(
        const std::string &compoundValue,
        std::vector<std::string> &values);

    int translate_field_idx(int dtcfid);

    int get_value_by_id(
        char* &valueHead,
        int fieldId,
        std::string &fieldValue);

    char* expand_buff(int len, char *oldPtr);

    int process_select(DTCTask* Task);
    int process_insert(DTCTask* Task);
    int process_update(DTCTask* Task);
    int process_delete(DTCTask* Task);
    int process_replace (DTCTask* Task);
    int process_reload_config(DTCTask *Task);
    int ProcessReplicate(DTCTask* Task);

    int value_add_to_str(
        const DTCValue* additionValue, 
        int ifield_type,
        std::string &baseValue);

    int condition_filter(
        const std::string &rocksValue,
        int fieldid, 
        int fieldType, 
        const DTCFieldValue *condition);
    
    int condition_filter(
        const std::string &rocksValue,
        const std::string& condValue,
        int fieldType, 
        int comparator);

    template<class... Args>
      bool is_matched_template(Args... len);

    template<class T>
      bool is_matched(
          const T lv,
          int comparator,
          const T rv);

    //template<char*>
      bool is_matched(
          const char *lv,
          int comparator,
          const char *rv,
          int lLen,
          int rLen,
          bool caseSensitive);
  
  private:
      int get_replicate_end_key();
      
      int encode_dtc_key(
          const std::string& oKey,
          std::string& codedKey);

      int encode_rocks_key(
          const std::vector<std::string> &keys,
          std::string &rocksKey);
      
      void encode_bitmap_keys(
          std::vector<std::string>& keys,
          std::string& keyBitmaps);
      
      void decodeBitmapKeys(
          const std::string& rocksValue,
          std::vector<std::string>& keys,
          int& bitmapLen);

      int decode_keys(
          const std::string& compoundKey,
          std::vector<std::string>& keys);

      bool convert_to_lower(
          std::string& key,
          std::vector<char>& keyCaseBitmap);
      
      void recover_to_upper(
          const std::string& rocksValue, 
          int& bitmapLen, 
          std::string& key);
      
      int get_key_bitmap_len(const std::string& rocksValue);

      // check dtc key whether match the prifix of the `rockskey`
      inline int key_matched(const std::string& dtcKey, const std::string& rocksKey)
      {
        int dtcKLen = dtcKey.length();
        int rockKLen = rocksKey.length();
        
        if ((dtcKLen == 0 && rockKLen != 0) || dtcKLen > rockKLen) return 1;

        // compare with case sensitive
        return rocksKey.compare(0, dtcKLen, dtcKey);
      }

      // check two rocksdb key whether equal or not
      int rocks_key_matched(const std::string& rocksKey1, const std::string& rocksKey2);
      int range_key_matched(const std::string& rocksKey, const std::vector<QueryCond>& keyConds);

      int analyse_primary_key_conds(DirectRequestContext* reqCxt, std::vector<QueryCond>& primaryKeyConds);
      
      void insert_stat(
          RocksdbProcess::OprType opr,
          int64_t timeElapse);

      void print_stat_info();

      int memcmp_ignore_case(const void* lv, const void* rv, int count);
};

#endif // __DB_PROCESS_ROCKS_H__
