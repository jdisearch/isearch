/*
 * =====================================================================================
 *
 *       Filename:  container.h
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
#ifndef __DTC_CONTAINER_HEADER__
#define __DTC_CONTAINER_HEADER__

class DTCTableDefinition;
class TaskRequest;
class NCRequest;
class NCResultInternal;
union DTCValue;

#if GCC_MAJOR >= 4
#pragma GCC visibility push(default)
#endif
class IInternalService
{
public:
	virtual ~IInternalService(void) {}

	virtual const char *query_version_string(void) = 0;
	virtual const char *query_service_type(void) = 0;
	virtual const char *query_instance_name(void) = 0;
};

class IDTCTaskExecutor
{
public:
	virtual ~IDTCTaskExecutor(void) {}

	virtual NCResultInternal *task_execute(NCRequest &, const DTCValue *) = 0;
};

class IDTCService : public IInternalService
{
public:
	virtual ~IDTCService(void) {}

	virtual DTCTableDefinition *query_table_definition(void) = 0;
	virtual DTCTableDefinition *query_admin_table_definition(void) = 0;
	virtual IDTCTaskExecutor *query_task_executor(void) = 0;
	virtual int match_listening_ports(const char *, const char * = 0) = 0;
};
#if GCC_MAJOR >= 4
#pragma GCC visibility pop
#endif

IInternalService *query_internal_service(const char *name, const char *instance);

#endif
