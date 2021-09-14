#ifndef __DB_INSTANCE_H__
#define __DB_INSTANCE_H__

#include <string>

#define list_add my_list_add
// #include "mysql.h"
#include <mysql.h>
#undef list_add

typedef struct
{
  std::string sDBHostName; /*DB host*/
	unsigned int sDBPort;/*DB port*/
  std::string sDBUserName;/*DB user*/
  std::string sPassword; /*DB password*/
  std::string sDBName;/*DB name*/
} DBInfo_t;

typedef struct
{
	MYSQL sMysqlInstance;
	MYSQL_RES* sMyRes;
	MYSQL_ROW sMyRow;
	// MYSQL_RES* sRes[MAX_MYSQL_RES];
	// MYSQL_ROW sRows[MAX_MYSQL_RES];
	DBInfo_t sDBInfo;
	// int sConnectTimeOut;
	// int sReadTimeOut;
	// int WriteTimeOut;
	bool hasConnected;
	bool inTransaction;
} DBConnection_t;

class DBInstance
{
private:
  DBConnection_t mDBConnection;

public:
	DBInstance();
	~DBInstance();
	
public:
  bool initDB(
      const std::string& dbHostName, 
      const int port, 
      const std::string& dbName, 
      const std::string& dbUser, 
      const std::string& dbPassword, 
      const int iConnectTimeOut,
      const int iReadTimeOut, 
      const int iWriteTimeOut);

	int execSQL(const char *sql);
	int getAffectedRows();
	void closeDB();
	bool connectDB();
	int fetchRow();
	void freeResult();
  my_ulonglong getInsertID();
	int getNumFields();
	int getNumRows();
	bool startTransaction();
	bool rollBackTransaction();
	bool commitTransaction();
  char* getFieldValue(const int fieldIndex);

private:
  char* skipWhiteSpace(const char* sStr);
};

#endif // __DB_INSTANCE_H__
