/*
 * =====================================================================================
 *
 *       Filename:  container_dtcd.cc
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
#include <stdio.h>
#include <map>

#include "compiler.h"
#include "container.h"
#include "version.h"
#include "table_def.h"
#include "buffer_error.h"
#include "listener_pool.h"
#include "request_threading.h"
#include "task_multiplexer.h"
#include "../api/c_api/dtc_int.h"
#include "proxy_listen_pool.h"
#include "table_def_manager.h"

class DTCTaskExecutor : public IDTCTaskExecutor, public ThreadingOutputDispatcher<TaskRequest>
{
public:
	virtual NCResultInternal *task_execute(NCRequest &rq, const DTCValue *kptr);
};

NCResultInternal *DTCTaskExecutor::task_execute(NCRequest &rq, const DTCValue *kptr)
{
	NCResultInternal *res = new NCResultInternal(rq.tdef);
	if (res->Copy(rq, kptr) < 0)
		return res;
	res->set_owner_info(this, 0, NULL);
	switch (ThreadingOutputDispatcher<TaskRequest>::execute((TaskRequest *)res))
	{
	case 0: // OK
		res->process_internal_result(res->Timestamp());
		break;
	case -1: // no side effect
		res->set_error(-EC_REQUEST_ABORTED, "API::sending", "Server Shutdown");
		break;
	case -2:
	default: // result unknown, leak res by purpose
			 //new NCResult(-EC_REQUEST_ABORTED, "API::recving", "Server Shutdown");
		log_error("(-EC_REQUEST_ABORTED, API::sending, Server Shutdown");
		break;
	}
	return res;
}

class DTCInstance : public IDTCService
{
public:
	AgentListenPool *ports;
	DTCTaskExecutor *executor;
	int mypid;

public:
	DTCInstance();
	virtual ~DTCInstance();

	virtual const char *query_version_string(void);
	virtual const char *query_service_type(void);
	virtual const char *query_instance_name(void);

	virtual DTCTableDefinition *query_table_definition(void);
	virtual DTCTableDefinition *query_admin_table_definition(void);
	virtual IDTCTaskExecutor *query_task_executor(void);
	virtual int match_listening_ports(const char *, const char * = NULL);

	int IsOK(void) const
	{
		return this != NULL &&
			   ports != NULL &&
			   executor != NULL &&
			   getpid() == mypid;
	}
};

extern ListenerPool *listener;
DTCInstance::DTCInstance(void)
{
	ports = NULL;
	executor = NULL;
	mypid = getpid();
}

DTCInstance::~DTCInstance(void)
{
}

const char *DTCInstance::query_version_string(void)
{
	return version_detail;
}

const char *DTCInstance::query_service_type(void)
{
	return "dtcd";
}

const char *DTCInstance::query_instance_name(void)
{
	return TableDefinitionManager::Instance()->get_cur_table_def()->table_name();
}

DTCTableDefinition *DTCInstance::query_table_definition(void)
{
	return TableDefinitionManager::Instance()->get_cur_table_def();
}

DTCTableDefinition *DTCInstance::query_admin_table_definition(void)
{
	return TableDefinitionManager::Instance()->get_hot_backup_table_def();
}

IDTCTaskExecutor *DTCInstance::query_task_executor(void)
{
	return executor;
}

int DTCInstance::match_listening_ports(const char *host, const char *port)
{
	return ports->Match(host, port);
}

struct nocase
{
	bool operator()(const char *const &a, const char *const &b) const
	{
		return strcasecmp(a, b) < 0;
	}
};
typedef std::map<const char *, DTCInstance, nocase> instmap_t;
static instmap_t instMap;

extern "C" __EXPORT
	IInternalService *
	_QueryInternalService(const char *name, const char *instance)
{
	instmap_t::iterator i;

	if (!name || !instance)
		return NULL;

	if (strcasecmp(name, "dtcd") != 0)
		return NULL;

	/* not found */
	i = instMap.find(instance);
	if (i == instMap.end())
		return NULL;

	DTCInstance &inst = i->second;
	if (inst.IsOK() == 0)
		return NULL;

	return &inst;
}

void InitTaskExecutor(const char *name, AgentListenPool *listener, TaskDispatcher<TaskRequest> *output)
{
	if (NCResultInternal::verify_class() == 0)
	{
		log_error("Inconsistent class NCResultInternal detected, internal API disabled");
		return;
	}
	// this may cause memory leak, but this is small
	char *tablename = (char *)malloc(strlen(name) + 1);
	memset(tablename, 0, strlen(name) + 1);
	strncpy(tablename, name, strlen(name));

	DTCInstance &inst = instMap[tablename];
	inst.ports = listener;
	DTCTaskExecutor *executor = new DTCTaskExecutor();
	TaskMultiplexer *batcher = new TaskMultiplexer(output->owner_thread());
	batcher->bind_dispatcher(output);
	executor->bind_dispatcher(batcher);
	inst.executor = executor;
	log_info("Internal Task Executor initialized");
}

void StopTaskExecutor(void)
{
	instmap_t::iterator i;
	for (i = instMap.begin(); i != instMap.end(); i++)
	{
		if (i->second.executor)
			i->second.executor->Stop();
	}
}
