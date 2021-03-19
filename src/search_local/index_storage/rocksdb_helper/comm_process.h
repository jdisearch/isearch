/*
 * =====================================================================================
 *
 *       Filename: comm_process.h 
 *
 *    Description:  
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
#ifndef __COMM_PROCESS_H__
#define __COMM_PROCESS_H__

#include <value.h>
#include <field.h>
#include <namespace.h>
#include "helper_log_api.h"

DTC_BEGIN_NAMESPACE

class CommHelper;
class HelperMain;
class DTCTableDefinition;
class DbConfig;
class RowValue;
class PacketHeader;
class DTCVersionInfo;
class DTCConfig;
union DTCValue;
class DTCFieldValue;
class DTCFieldSet;

typedef CommHelper *(*CreateHandle)(void);

/* helper����Ļ��� 
ʹ������Ҫ�̳�����࣬��ʵ������������global_init()��Init()��Execute()��process_get()��virtual�����ӿڡ�
*/
class CommHelper
{
protected:
	int GroupID;
	int Role;
	int Timeout;

public:
	// ���캯���������Ҫ���������server�����ӣ������ڹ��캯�����ӡ���Ҫ��Init����Execute��ִ�С�
	CommHelper();
	virtual ~CommHelper();

	// ������Ҫʵ�ֵ�3������
	/* helper��һ�������顣���������fork֮ǰ���õ� */
	virtual int global_init(void);

	/* ���������fork֮����õ� */
	virtual int Init(void);

	/* ÿ���������һ������������� */
	virtual int Execute();

	/* ����һ������ĳ�ʱʱ�� ����λΪ��*/
	void set_proc_timeout(int timeout) { Timeout = timeout; }

	friend class HelperMain;

protected:
	/* ����Get����Ľӿں��� */
	virtual int process_get() = 0;

	/* ����Insert����ӿں��� */
	virtual int process_insert() = 0;

	/* ����Delete����ӿں��� */
	virtual int process_delete() = 0;

	/* ����Update����ӿں��� */
	virtual int process_update() = 0;

	/* ����Replace����ӿں��� */
	virtual int process_replace() = 0;

	/* ��ѯ��������ַ */
	const char *get_server_address(void) const;

	/* ��ѯ�����ļ� */
	int get_int_val(const char *sec, const char *key, int def = 0);
	unsigned long long get_size_val(const char *sec, const char *key, unsigned long long def = 0, char unit = 0);
	int get_idx_val(const char *, const char *, const char *const *, int = 0);
	const char *get_str_val(const char *sec, const char *key);
	bool has_section(const char *sec);
	bool has_key(const char *sec, const char *key);

	/* ��ѯ���� */
	DTCTableDefinition *Table(void) const;
	int field_type(int n) const;
	int field_size(int n) const;
	int field_id(const char *n) const;
	const char *field_name(int id) const;
	int num_fields(void) const;
	int key_fields(void) const;

	/* ��ȡ�����ͷ��Ϣ */
	const PacketHeader *Header() const;

	/* ��ȡ�����������Ϣ */
	const DTCVersionInfo *version_info() const;

	/* ���������� */
	int request_code() const;

	/* �����Ƿ���key��ֵ��������key��insert����û��keyֵ�� */
	int has_request_key() const;

	/* �����keyֵ */
	const DTCValue *request_key() const;

	/* ����key��ֵ */
	unsigned int int_key() const;

	/* �����where���� */
	const DTCFieldValue *request_condition() const;

	/* update����insert����replace����ĸ�����Ϣ */
	const DTCFieldValue *request_operation() const;

	/* get����select���ֶ��б� */
	const DTCFieldSet *request_fields() const;

	/* ���ô�����Ϣ�������롢�������ط�����ϸ������Ϣ */
	void set_error(int err, const char *from, const char *msg);

	/* �Ƿ�ֻ��select count(*)��������Ҫ���ؽ���� */
	int count_only(void) const;

	/* �Ƿ�û��where���� */
	int all_rows(void) const;

	/* �����������еĸ�������row��ͨ��������������rowһһ��������ӿڸ��£�Ȼ�����±�������ݲ㡣������Լ�����request_operation���£�����Ҫ������ӿڣ�*/
	int update_row(RowValue &row);

	/* �Ƚ����е���row���Ƿ�������������������ָ��ֻ�Ƚ�ǰ��n���ֶ� */
	int compare_row(const RowValue &row, int iCmpFirstNField = 256) const;

	/* ������������һ������ǰ�������ȵ�������ӿڳ�ʼ�� */
	int prepare_result(void);

	/* ��key��ֵ���Ƶ�r */
	void update_key(RowValue *r);

	/* ��key��ֵ���Ƶ�r */
	void update_key(RowValue &r);

	/* ����������������select * from table limit m,n������total-rowsһ��Ƚ������������ */
	int set_total_rows(unsigned int nr);

	/* ����ʵ�ʸ��µ����� */
	int set_affected_rows(unsigned int nr);

	/* ����������һ������ */
	int append_row(const RowValue &r);
	int append_row(const RowValue *r);

private:
	void *addr;
	long check;
	char _name[16];
	char _title[80];
	int _titlePrefixSize;
	const char *_server_string;
	const DbConfig *_dbconfig;
	DTCConfig *_config;
	DTCTableDefinition *_tdef;

private:
	CommHelper(CommHelper &);
	void Attach(void *);

	void init_title(int group, int role);
	void set_title(const char *status);
	const char *Name(void) const { return _name; }

public:
	HelperLogApi logapi;
};

DTC_END_NAMESPACE

#endif
