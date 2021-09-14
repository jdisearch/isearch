/////////////////////////////////////////////////////////////////
//
//   Handle access to mysql database 
//       created by qiuyu on Feb 21, 2019
////////////////////////////////////////////////////////////////
#include "DBInstance.h"
#include "DtcMonitorConfigMgr.h"
#include "log.h"

#include <errmsg.h>
#include <string.h>

DBInstance::DBInstance()
{
}

DBInstance::~DBInstance()
{
	closeDB();
}

void DBInstance::freeResult()
{
		mysql_free_result(mDBConnection.sMyRes);
    mDBConnection.sMyRes = NULL;
    mDBConnection.sMyRow = NULL;
}

// initialize the db connection in this instance
bool DBInstance::initDB(
    const std::string& dbHostName, 
    const int port, 
    const std::string& dbName, 
    const std::string& dbUser, 
    const std::string& dbPassword, 
    const int iConnectTimeOut,
    const int iReadTimeOut, 
    const int iWriteTimeOut)
{
  monitor_log_error("Database initilized info. host:%s, port:%d, dbName:%s, dbUser:%s,\
      dbPassword:%s", dbHostName.c_str(), port, dbName.c_str(), dbUser.c_str(), dbPassword.c_str());

  if (dbHostName.empty() || port <= 0 || dbName.empty() || dbUser.empty() || dbPassword.empty())
  {
    monitor_log_error("invalid database initilized info.");
    return false;
  }

  mDBConnection.sMyRes = NULL;
  mDBConnection.hasConnected = false;
  mDBConnection.inTransaction = false;
  mDBConnection.sDBInfo.sDBHostName = dbHostName;
  mDBConnection.sDBInfo.sDBPort = port;
  mDBConnection.sDBInfo.sDBUserName = dbUser;
  mDBConnection.sDBInfo.sPassword = dbPassword;
  mDBConnection.sDBInfo.sDBName = dbName;
  
  // init mysql
  mysql_init(&mDBConnection.sMysqlInstance);

  // hard code
  mysql_options(&mDBConnection.sMysqlInstance, MYSQL_SET_CHARSET_NAME, "utf8");
  
  // reconnect if connection lost from Mysql for alive timeout 
  bool reconnect = true;
  mysql_options(&mDBConnection.sMysqlInstance, MYSQL_OPT_RECONNECT, &reconnect);

  mysql_options(&mDBConnection.sMysqlInstance, MYSQL_OPT_CONNECT_TIMEOUT,(const char *)&iConnectTimeOut);
  mysql_options(&mDBConnection.sMysqlInstance, MYSQL_OPT_READ_TIMEOUT,(const char *)&iReadTimeOut);
  mysql_options(&mDBConnection.sMysqlInstance, MYSQL_OPT_WRITE_TIMEOUT,(const char *)&iWriteTimeOut);

  return true;
}

void DBInstance::closeDB()
{
	monitor_log_error("closing database connection");
  
  freeResult();
	// mDBConnection.sMyRes = NULL;
	// mDBConnection.sMyRow = NULL;
	mysql_close(&(mDBConnection.sMysqlInstance));
	mDBConnection.hasConnected = false;
	mDBConnection.inTransaction = false;
  
  return;
}

bool DBInstance::connectDB()
{
	if (mDBConnection.hasConnected)
  {
    monitor_log_error("has connected to mysql yet, check it.");
    return true;
		// closeDB();
  }

	monitor_log_error("connecting to db, dbHost:%s, user:%s, dbName:%s",
      mDBConnection.sDBInfo.sDBHostName.c_str(), mDBConnection.sDBInfo.sDBUserName.c_str(),
      mDBConnection.sDBInfo.sDBName.c_str());
	
  MYSQL *pMysql = &(mDBConnection.sMysqlInstance);
	if (!mysql_real_connect(pMysql, mDBConnection.sDBInfo.sDBHostName.c_str(),
							mDBConnection.sDBInfo.sDBUserName.c_str(), mDBConnection.sDBInfo.sPassword.c_str(),
							mDBConnection.sDBInfo.sDBName.c_str(),
							mDBConnection.sDBInfo.sDBPort, NULL, 0))
	{
		monitor_log_error("connect to db failed:%s", mysql_error(pMysql));
		return false;
	}
	mDBConnection.hasConnected = true;
	monitor_log_error("connected to db success");

	return true;
}

bool DBInstance::startTransaction()
{
	if(mDBConnection.inTransaction)
	{
		monitor_log_error("has unfinished transaction in this process");
		return false;
	}

	const char *sql="BEGIN;";
	int ret = execSQL(sql);
	if (ret < 0)
	{
		monitor_log_error("start transaction error");
	}
	else
	{
		mDBConnection.inTransaction = true;  //a transaction start in this connection
	}
	
  return ret;
}

bool DBInstance::rollBackTransaction()
{
	if (!mDBConnection.inTransaction)
	{
		monitor_log_error("no started transaction");
		return false;
	}

	const char *sql="ROLLBACK;";
	int ret = execSQL(sql);
	if (ret < 0)
	{
		monitor_log_error("rollback transaction failed");
	}
	mDBConnection.inTransaction = false;  //a transaction has been rollbacked

	return true;
}

bool DBInstance::commitTransaction()
{
	if (!mDBConnection.inTransaction)
	{
		monitor_log_error("no transaction for committed");
		return false;
	}

	const char *sql="COMMIT;";
	int ret = execSQL(sql);
	if (ret < 0)
	{
		monitor_log_error("committed transaction failed");
	}
	mDBConnection.inTransaction = false;  //a transaction rollback in mysql connection

	return true;
}

int DBInstance::execSQL(const char *sql)
{
	if (sql == NULL)
	{
		monitor_log_error("sql is NULL");
		return -1;
	}
  
  monitor_log_error("execute sql. sql:%s", sql);

  static int reConnect = 0;

RETRY:
	if (!mDBConnection.hasConnected)
	{
		monitor_log_error("mysql db has not connected");

		if (!mDBConnection.inTransaction)
		{
			monitor_log_error("not in transaction, try to reconnect");
			if (!connectDB())
			{
				monitor_log_error("connect to mysql db failed");
        reConnect = 0;
				return -1;
			}
		}
		else
		{
			monitor_log_error("has an unfinished transaction, not connect automatic");
			reConnect = 0;
      return -1;
		}
	}
  
	MYSQL* pMysql = &(mDBConnection.sMysqlInstance);
	if (mDBConnection.sDBInfo.sDBName.empty())
  {
    monitor_log_error("database name can not be empty");
    reConnect = 0;
    closeDB();
    return -1;
  }

  if (mysql_select_db(pMysql, mDBConnection.sDBInfo.sDBName.c_str()) != 0)
  {
    if (mysql_errno(pMysql) == CR_SERVER_GONE_ERROR && reConnect < 10)
    {
      // server have closed the connection for alive time elapsed, reconnect it
      monitor_log_error("retry connect, round:%d", reConnect);
      
      // close the unclosed socket first
      closeDB();
      reConnect++;
      goto RETRY;
    }
    monitor_log_error("switch to db failed:%s", mysql_error(pMysql));
    reConnect = 0;
    closeDB();
    return -1;
  }
  reConnect = 0;
  
  // mysql_query return 0 when excute success
	if (mysql_query(pMysql, sql))
	{
		monitor_log_error("query failed:%s", mysql_error(pMysql));
    
		if (mysql_errno(pMysql) == CR_SERVER_GONE_ERROR || mysql_errno(pMysql) == CR_SERVER_LOST)
			closeDB();
		return -mysql_errno(pMysql);
	}
  
  bool isSelect = false;
	if (!strncasecmp(skipWhiteSpace(sql), "select", 6)) isSelect = true;

  // function `mysql_store_result` store the result to local and return the pointer point to 
  // the it 
  //
  // it's meaningless to call mysql_store_result when the query type is not select
	if (isSelect && !(mDBConnection.sMyRes = mysql_store_result(pMysql)))
	{
		monitor_log_error("mysql return a NULL result set:%s", mysql_error(pMysql));
		return -2;
	}

	return 0;
}

// return the pointer point to the value
char* DBInstance::getFieldValue(const int fieldIndex)
{
  if (NULL == mDBConnection.sMyRes || NULL == mDBConnection.sMyRow)
  {
    monitor_log_error("empty dataset. errno:%s", mysql_error(&mDBConnection.sMysqlInstance));
    return NULL;
  }

  int numFields = getNumFields(); 
  if (fieldIndex < 0 || fieldIndex >= numFields)
  {
    monitor_log_error("field index out of boundary. totalFields:%d,\
        fieldIndex:%d", numFields, fieldIndex);
    return NULL;
  }

  return mDBConnection.sMyRow[fieldIndex];
}

// get the next row result which has been stored in the sMyRes with function
// mysql_store_result()
// Return:1. reach to the end or fetch failed, return NULL
//        2. otherwise return the result
int DBInstance::fetchRow()
{
  if (!mDBConnection.hasConnected)
    return -1;

  if ((mDBConnection.sMyRow = mysql_fetch_row(mDBConnection.sMyRes)) != 0) return 0;
  
  return -1;
}

// return number of rows those affected in the previous operation
// work for insert, delete, update sql command
// return:
//   1. > 0 : number of rows been touched
//   2. = 0 : previous operation do nothing
//   3. -1 : error 
int DBInstance::getAffectedRows()
{
	return mysql_affected_rows(&(mDBConnection.sMysqlInstance));
}

// return number of rows in the dataset
// this command is only work for select sql
// Note: if the table is empty, mysql_num_rows will return 1, not 0
int DBInstance::getNumRows()
{
  if (!mDBConnection.sMyRes) {
  	return 0;
  }

	MYSQL_RES* pRes = mDBConnection.sMyRes;

	return mysql_num_rows(pRes);
}

// get the field number in one record
int DBInstance::getNumFields()
{
  if (!mDBConnection.sMyRes || NULL == mDBConnection.sMyRow) return 0;
	
  MYSQL_RES* pRes = mDBConnection.sMyRes;

	return mysql_num_fields(pRes);
}
/*
 * get auto increment id
 */
my_ulonglong DBInstance::getInsertID()
{
	return mysql_insert_id(&(mDBConnection.sMysqlInstance));
}

char* DBInstance::skipWhiteSpace(const char* sStr)
{
  char *p = (char *)sStr;
  while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  
  return p;
}
