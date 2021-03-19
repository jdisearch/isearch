/*
 * =====================================================================================
 *
 *       Filename:  helper_collect.cc
 *
 *    Description:  database collection helper.
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
#include <algorithm>
#include "list.h"
#include "dbconfig.h"
#include "helper_group.h"
#include "helper_collect.h"
#include "request_base.h"
#include "task_request.h"
#include "log.h"
#include "key_guard.h"
#include "stat_dtc.h"
#include "protocol.h"
#include "dtc_global.h"
#include "watchdog_listener.h"
#include "watchdog_helper.h"
#include "unix_socket.h"

extern const char *HelperName[];

class GuardNotify : public ReplyDispatcher<TaskRequest>
{
public:
	GuardNotify(GroupCollect *o) : owner(o) {}
	~GuardNotify() {}
	virtual void reply_notify(TaskRequest *);

private:
	GroupCollect *owner;
};

void GuardNotify::reply_notify(TaskRequest *task)
{
	if (task->result_code() >= 0)
		owner->guard->add_key(task->barrier_key(), task->packed_key());
	task->reply_notify();
}

GroupCollect::GroupCollect() : TaskDispatcher<TaskRequest>(NULL),
							   dbConfig({NULL, NULL}),
							   hasDummyMachine(0),
							   groups({NULL, NULL}),
							   groupMap({NULL, NULL}),
							   guardReply(NULL),
							   tableNo(0),
							   guard(NULL)
{
	/*总队列的统计，暂时还有意义，暂时保留*/
	statQueueCurCount = statmgr.get_item_u32(CUR_QUEUE_COUNT);
	statQueueMaxCount = statmgr.get_item_u32(MAX_QUEUE_COUNT);

	/*新增的四个组中最大的队列长度统计项，用来进行告警监控*/
	statReadQueueCurMaxCount = statmgr.get_item_u32(HELPER_READ_GROUR_CUR_QUEUE_MAX_SIZE);
	statWriteQueueMaxCount = statmgr.get_item_u32(HELPER_WRITE_GROUR_CUR_QUEUE_MAX_SIZE);
	statCommitQueueCurMaxCount = statmgr.get_item_u32(HELPER_COMMIT_GROUR_CUR_QUEUE_MAX_SIZE);
	statSlaveReadQueueMaxCount = statmgr.get_item_u32(HELPER_SLAVE_READ_GROUR_CUR_QUEUE_MAX_SIZE);
}

GroupCollect::~GroupCollect()
{
	if (groups[0])
	{
		for (int i = 0; i < dbConfig[0]->machineCnt * GROUPS_PER_MACHINE; i++)
			DELETE(groups[0][i]);

		FREE_CLEAR(groups[0]);
	}

	FREE_CLEAR(groupMap[0]);
	DELETE(guard);
	DELETE(guardReply);
}

HelperGroup *GroupCollect::select_group(TaskRequest *task)
{

	const DTCValue *key = task->request_key();
	uint64_t uk;
	/* key-hash disable */
	if (dbConfig[0]->keyHashConfig.keyHashEnable == 0 || key == NULL)
	{
		if (NULL == key)
			uk = 0;
		else if (key->s64 < 0)
			uk = 0 - key->s64;
		else
			uk = key->s64;
	}
	else
	{
		switch (task->field_type(0))
		{
		case DField::Signed:
		case DField::Unsigned:
			uk = dbConfig[0]->keyHashConfig.keyHashFunction((const char *)&(key->u64),
															sizeof(key->u64),
															dbConfig[0]->keyHashConfig.keyHashLeftBegin,
															dbConfig[0]->keyHashConfig.keyHashRightBegin);
			break;
		case DField::String:
		case DField::Binary:
			uk = dbConfig[0]->keyHashConfig.keyHashFunction(key->bin.ptr,
															key->bin.len,
															dbConfig[0]->keyHashConfig.keyHashLeftBegin,
															dbConfig[0]->keyHashConfig.keyHashRightBegin);
			break;
		default:
			uk = 0;
		}
	}

	if (dbConfig[1])
	{
		int idx = uk / dbConfig[1]->dbDiv % dbConfig[1]->dbMod;
		int machineId = groupMap[1][idx];
		HelperGroup *ptr = groups[1][machineId * GROUPS_PER_MACHINE];
		if (ptr != NULL && task->request_code() != DRequest::Get)
			return GROUP_READONLY;
	}

	int idx = uk / dbConfig[0]->dbDiv % dbConfig[0]->dbMod;

	int machineId = groupMap[0][idx];
	if (machineId == GMAP_NONE)
		return NULL;
	if (machineId == GMAP_DUMMY)
		return GROUP_DUMMY;

	HelperGroup **ptr = &groups[0][machineId * GROUPS_PER_MACHINE];

	if (task->request_code() == DRequest::Get && ptr[GROUPS_PER_ROLE] &&
		false == guard->in_set(task->barrier_key(), task->packed_key()))
	{
		int role = 0;
		switch (dbConfig[0]->mach[machineId].mode)
		{
		case BY_SLAVE:
			role = 1;
			break;

		case BY_DB:
			role = (uk / dbConfig[0]->dbDiv) & 1;

		case BY_TABLE:
			role = (uk / dbConfig[0]->tblDiv) & 1;

		case BY_KEY:
			role = task->barrier_key() & 1;
		}

		return ptr[role * GROUPS_PER_ROLE];
	}

	int g = task->request_type();

	while (--g >= 0)
	{
		if (ptr[g] != NULL)
		{
			return ptr[g];
		}
	}
	return NULL;
}

bool GroupCollect::is_commit_full(TaskRequest *task)
{
	if (task->request_code() != DRequest::Replace)
		return false;

	HelperGroup *helperGroup = select_group(task);
	if (helperGroup == NULL || helperGroup == GROUP_DUMMY || helperGroup == GROUP_READONLY)
		return false;

	if (helperGroup->queue_full())
	{
		log_warning("NO FREE COMMIT QUEUE SLOT");
		helperGroup->dump_state();
	}
	return helperGroup->queue_full() ? true : false;
}

int GroupCollect::Cleanup()
{
	newDb.clear();
	new2old.clear();
	return 0;
}

int GroupCollect::Cleanup2()
{
	if (groups[1])
	{
		for (int i = 0; i < dbConfig[1]->machineCnt; ++i)
		{
			std::vector<int>::iterator it = find(newDb.begin(), newDb.end(), i);
			if (it != newDb.end())
			{
				for (int j = 0; j < GROUPS_PER_MACHINE; ++j)
				{
					DELETE(groups[1][j]);
				}
			}
		}
		FREE_CLEAR(groups[1]);
	}
	FREE_CLEAR(groupMap[1]);
	if (dbConfig[1])
	{
		dbConfig[1]->Destroy();
		dbConfig[1] = NULL;
	}
	return 0;
}

int GroupCollect::build_helper_object(int idx)
{
	if (groups[idx] != NULL)
	{
		log_error("groups[%d] exists", idx);
		return -1;
	}
	groups[idx] = (HelperGroup **)CALLOC(sizeof(HelperGroup *), dbConfig[idx]->machineCnt * GROUPS_PER_MACHINE);
	if (!groups[idx])
	{
		log_error("malloc failed, %m");
		return -1;
	}

	/* build helper object */
	for (int i = 0; i < dbConfig[idx]->machineCnt; i++)
	{
		if (dbConfig[idx]->mach[i].helperType == DUMMY_HELPER)
			continue;
		if (idx == 1 && find(newDb.begin(), newDb.end(), i) == newDb.end())
		{
			// if not new db mach, just continue, copy old mach when switch
			continue;
		}
		for (int j = 0; j < GROUPS_PER_MACHINE; j++)
		{
			if (dbConfig[idx]->mach[i].gprocs[j] == 0)
				continue;

			char name[24];
			snprintf(name, sizeof(name), "%d%c%d", i, MACHINEROLESTRING[j / GROUPS_PER_ROLE], j % GROUPS_PER_ROLE);
			groups[idx][i * GROUPS_PER_MACHINE + j] =
				new HelperGroup(
					dbConfig[idx]->mach[i].role[j / GROUPS_PER_ROLE].path,
					name,
					dbConfig[idx]->mach[i].gprocs[j],
					dbConfig[idx]->mach[i].gqueues[j],
					DTC_SQL_USEC_ALL);
			if (j >= GROUPS_PER_ROLE)
				groups[idx][i * GROUPS_PER_MACHINE + j]->fallback =
					groups[idx][i * GROUPS_PER_MACHINE];
			log_debug("start worker %s", name);
		}
	}

	return 0;
}

int GroupCollect::build_master_group_mapping(int idx)
{
	if (groupMap[idx] != NULL)
	{
		log_error("groupMap[%d] exist", idx);
		return -1;
	}
	groupMap[idx] = (short *)MALLOC(sizeof(short) * dbConfig[idx]->dbMax);
	if (groupMap[idx] == NULL)
	{
		log_error("malloc error for groupMap[%d]", idx);
		return -1;
	}
	for (int i = 0; i < dbConfig[idx]->dbMax; i++)
		groupMap[idx][i] = GMAP_NONE;

	/* build master group mapping */
	for (int i = 0; i < dbConfig[idx]->machineCnt; i++)
	{
		int gm_id = i;
		if (dbConfig[idx]->mach[i].helperType == DUMMY_HELPER)
		{
			gm_id = GMAP_DUMMY;
			hasDummyMachine = 1;
		}
		else if (dbConfig[idx]->mach[i].procs == 0)
		{
			continue;
		}
		for (int j = 0; j < dbConfig[idx]->mach[i].dbCnt; j++)
		{
			const int db = dbConfig[idx]->mach[i].dbIdx[j];
			if (groupMap[idx][db] >= 0)
			{
				log_error("duplicate machine, db %d machine %d %d",
						  db, groupMap[idx][db] + 1, i + 1);
				return -1;
			}
			groupMap[idx][db] = gm_id;
		}
	}
	for (int i = 0; i < dbConfig[idx]->dbMax; ++i)
	{
		if (groupMap[idx][i] == GMAP_NONE)
		{
			log_error("db completeness check error, db %d not found", i);
			return -1;
		}
	}
	return 0;
}

DbConfig *GroupCollect::get_db_config(TaskRequest *task)
{
	RowValue row(task->table_definition());
	DTCConfig *config = NULL;
	DbConfig *newdb = NULL;
	// parse db config
	if (!task->request_operation())
	{
		log_error("table.conf not found when migrate db");
		task->set_error(-EC_DATA_NEEDED, "group collect", "migrate db need table.conf");
		return NULL;
	}
	task->update_row(row);
	log_debug("strlen: %ld, row[3].bin.ptr: %s", strlen(row[3].bin.ptr), row[3].bin.ptr);
	char *buf = row[3].bin.ptr;
	config = new DTCConfig();
	if (config->parse_buffered_config(buf, NULL, "DB_DEFINE", false) != 0)
	{
		log_error("table.conf illeagl when migrate db, parse error");
		task->set_error(-EC_ERR_MIGRATEDB_ILLEGAL, "group collect", "table.conf illegal, parse error");
		delete config;
		return NULL;
	}
	if ((newdb = DbConfig::Load(config)) == NULL)
	{
		log_error("table.conf illeagl when migrate db, load error");
		task->set_error(-EC_ERR_MIGRATEDB_ILLEGAL, "group collect", "table.conf illegal, load error");
		return NULL;
	}
	return newdb;
}

int GroupCollect::migrate_db(TaskRequest *task)
{
	int ret = 0;
	DbConfig *newDbConfig = get_db_config(task);
	if (newDbConfig == NULL)
		return -2;
	if (dbConfig[1])
	{
		bool same = dbConfig[1]->Compare(newDbConfig, true);
		newDbConfig->Destroy();
		if (!same)
		{
			log_error("new table.conf when migrating db");
			task->set_error(-EC_ERR_MIGRATEDB_MIGRATING, "group collect", "new table.conf when migrating db");
			return -2;
		}
		log_notice("duplicate table.conf when migrating db");
		task->set_error(-EC_ERR_MIGRATEDB_DUPLICATE, "group collect", "duplicate table.conf when migrating db");
		return 0;
	}
	// check are others fields same
	if (!newDbConfig->Compare(dbConfig[0], false))
	{
		newDbConfig->Destroy();
		log_error("new table.conf does not match old one");
		task->set_error(-EC_ERR_MIGRATEDB_DISTINCT, "group collect", "new table.conf does not match old one");
		return -2;
	}
	// set read only on new db
	dbConfig[1] = newDbConfig;
	// find new db
	dbConfig[1]->find_new_mach(dbConfig[0], newDb, new2old);
	log_debug("found %ld new db machine", newDb.size());
	if (newDb.size() == 0)
	{
		log_error("table.conf does not contain new db when migrate db");
		task->set_error(-EC_DATA_NEEDED, "group collect", "table.conf does not contain new db");
		return -1;
	}
	// check db completeness of new db config
	if (build_master_group_mapping(1) != 0)
	{
		log_error("table.conf db mapping is not complete");
		task->set_error(-EC_DATA_NEEDED, "group collect", "table.conf db mapping is not complete");
		return -1;
	}

	// save new table.conf as table%d.conf
	char tableName[64];
	snprintf(tableName, 64, "../conf/table%d.conf", tableNo);
	log_debug("table.conf: %s", tableName);
	if (dbConfig[1]->cfgObj->Dump(tableName, true) != 0)
	{
		log_error("save table.conf as table2.conf error");
		task->set_error(-EC_SERVER_ERROR, "group collect", "save table.conf as table2.conf error");
		return -1;
	}

	// start listener, connect db, check access, start worker
	if ((ret = start_listener(task)) != 0)
		return ret;
	++tableNo;

	// start worker and create class member variable
	if (build_helper_object(1) != 0)
	{
		log_error("verify connect error: %m");
		task->set_error(-EC_ERR_MIGRATEDB_HELPER, "group collect", "start helper worker error");
		return -1;
	}

	// disable commit as none async
	disable_commit_group(1);
	set_timer_handler(recvList, connList, retryList, 1);

	return 0;
}

int GroupCollect::switch_db(TaskRequest *task)
{
	if (!dbConfig[1])
	{
		log_notice("migrate db not start");
		task->set_error(-EC_ERR_MIGRATEDB_NOT_START, "group collect", "migrate db not start");
		return -2;
	}
	DbConfig *newDbConfig = get_db_config(task);
	if (newDbConfig == NULL)
		return -2;
	// check is table same
	bool same = newDbConfig->Compare(dbConfig[1], true);
	newDbConfig->Destroy();
	if (!same)
	{
		log_error("switch db with different table.conf");
		task->set_error(-EC_ERR_MIGRATEDB_DISTINCT, "group collect", "switch db with different table.conf");
		return -2;
	}
	// start worker helper
	Attach(NULL, 1);
	// switch to new, unset read only
	std::swap(dbConfig[0], dbConfig[1]);
	std::swap(groups[0], groups[1]);
	std::swap(groupMap[0], groupMap[1]);
	// copy old client
	for (int i = 0; i < dbConfig[0]->machineCnt; ++i)
	{
		if (dbConfig[0]->mach[i].helperType == DUMMY_HELPER)
			continue;
		if (find(newDb.begin(), newDb.end(), i) != newDb.end())
			continue;
		memmove(groups[0] + i * GROUPS_PER_MACHINE, groups[1] + new2old[i] * GROUPS_PER_MACHINE, sizeof(HelperGroup *) * GROUPS_PER_MACHINE);
		log_debug("copy old client ptr: %p", *(groups[0] + i * GROUPS_PER_MACHINE));
	}
	// release old
	FREE_CLEAR(groupMap[1]);
	FREE_CLEAR(groups[1]);
	dbConfig[1]->Destroy();
	dbConfig[1] = NULL;
	// write conf file
	dbConfig[0]->cfgObj->Dump("../conf/table.conf", false);
	Cleanup();

	return 0;
}

int GroupCollect::notify_watch_dog(StartHelperPara *para)
{
	char buf[16];
	if (sizeof(*para) > 15)
		return -1;
	char *env = getenv(ENV_WATCHDOG_SOCKET_FD);
	int fd = env == NULL ? -1 : atoi(env);
	if (fd > 0)
	{
		memset(buf, 0, 16);
		buf[0] = WATCHDOG_INPUT_HELPER;
		log_debug("sizeof(*para): %d", sizeof(*para));
		memcpy(buf + 1, para, sizeof(*para));
		send(fd, buf, sizeof(buf), 0);
		return 0;
	}
	else
	{
		return -2;
	}
}

int GroupCollect::start_listener(TaskRequest *task)
{
	int ret = 0;
	log_debug("starting new db listener...");
	int nh = 0;
	dbConfig[1]->set_helper_path(getppid());
	for (std::vector<int>::iterator it = newDb.begin(); it != newDb.end(); ++it)
	{
		// start listener
		HELPERTYPE t = dbConfig[1]->mach[*it].helperType;
		log_debug("helper type = %d", t);
		if (DTC_HELPER >= t)
			continue;
		for (int r = 0; r < ROLES_PER_MACHINE; ++r)
		{
			int i, n = 0;
			for (i = 0; i < GROUPS_PER_ROLE && (r * GROUPS_PER_ROLE + i) < GROUPS_PER_MACHINE; ++i)
				n += dbConfig[1]->mach[*it].gprocs[r * GROUPS_PER_ROLE + i];
			if (n <= 0)
				continue;
			StartHelperPara para;
			para.type = t;
			para.backlog = n + 1;
			para.mach = *it;
			para.role = r;
			para.conf = DBHELPER_TABLE_NEW;
			para.num = tableNo;
			if ((ret = notify_watch_dog(&para)) < 0)
			{
				log_error("notify watchdog error for group %d role %d, ret: %d", *it, r, ret);
				return -1;
			}
			++nh;
		}
	}
	log_info("%d helper listener started", nh);
	return 0;
}

void GroupCollect::task_notify(TaskRequest *task)
{
	if (DRequest::ReloadConfig == task->request_code() && TaskTypeHelperReloadConfig == task->request_type())
	{
		collect_notify_helper_reload_config(task);
		return;
	}

	int ret = 0;
	if (task->request_code() == DRequest::SvrAdmin)
	{
		switch (task->requestInfo.admin_code())
		{
		case DRequest::ServerAdminCmd::MigrateDB:
			log_debug("GroupCollect::task_notify DRequest::SvrAdmin::MigrateDB");
			ret = migrate_db(task);
			if (ret == -1)
			{
				Cleanup2();
				Cleanup();
			}
			task->reply_notify();
			return;

		case DRequest::ServerAdminCmd::MigrateDBSwitch:
			log_debug("GroupCollect::task_notify DRequest::SvrAdmin::MigrateDBSwitch");
			ret = switch_db(task);
			task->reply_notify();
			return;
		default:
			// this should not happen
			log_error("unknown admin code: %d", task->requestInfo.admin_code());
			task->set_error(-EC_SERVER_ERROR, "helper collect", "unkown admin code");
			task->reply_notify();
			return;
		}
	}

	HelperGroup *helperGroup = select_group(task);

	if (helperGroup == NULL)
	{
		log_error("Key not belong to this server");
		task->set_error(-EC_OUT_OF_KEY_RANGE, "GroupCollect::task_notify", "Key not belong to this server");
		task->reply_notify();
	}
	else if (helperGroup == GROUP_DUMMY)
	{
		task->mark_as_black_hole();
		task->reply_notify();
	}
	else if (helperGroup == GROUP_READONLY)
	{
		log_debug("try to do non read op on a key which belongs db which is migrating");
		task->set_error(-EC_SERVER_ERROR, "helper collect", "try to do non read op on a key which belongs db which is migrating");
		task->reply_notify();
	}
	else
	{
		if (task->request_type() == TaskTypeWrite && guardReply != NULL)
			task->push_reply_dispatcher(guardReply);
		helperGroup->task_notify(task);
	}

	stat_helper_group_queue_count(groups[0], dbConfig[0]->machineCnt * GROUPS_PER_MACHINE);
	stat_helper_group_cur_max_queue_count(task->request_type());
}

int GroupCollect::load_config(DbConfig *cfg, int keysize, int idx)
{
	dbConfig[0] = cfg;
	int ret = 0;

	if ((ret = build_master_group_mapping(idx)) != 0)
	{
		log_error("build master group map error, ret: %d", ret);
		return ret;
	}
	if ((ret = build_helper_object(idx)) != 0)
	{
		log_error("build helper object error, ret: %d", ret);
		return ret;
	}

	if (dbConfig[0]->slaveGuard)
	{
		guard = new KeyGuard(keysize, dbConfig[0]->slaveGuard);
		guardReply = new GuardNotify(this);
	}
	return 0;
}

int GroupCollect::renew_config(struct DbConfig *cfg)
{
	dbConfig[1] = cfg;
	std::swap(dbConfig[0], dbConfig[1]);
	dbConfig[1]->Destroy();
	dbConfig[1] = NULL;
	return 0;
}

int GroupCollect::Attach(PollThread *thread, int idx)
{
	if (idx == 0)
		TaskDispatcher<TaskRequest>::attach_thread(thread);
	for (int i = 0; i < dbConfig[idx]->machineCnt * GROUPS_PER_MACHINE; i++)
	{
		if (groups[idx][i])
			groups[idx][i]->Attach(owner, &taskQueueAllocator);
	}
	return 0;
}

void GroupCollect::set_timer_handler(TimerList *recv, TimerList *conn, TimerList *retry, int idx)
{
	if (idx == 0)
	{
		recvList = recv;
		connList = conn;
		retryList = retry;
	}
	for (int i = 0; i < dbConfig[idx]->machineCnt; i++)
	{
		if (dbConfig[idx]->mach[i].helperType == DUMMY_HELPER)
			continue;
		for (int j = 0; j < GROUPS_PER_MACHINE; j++)
		{
			if (dbConfig[idx]->mach[i].gprocs[j] == 0)
				continue;
			if (groups[idx][i * GROUPS_PER_MACHINE + j])
				groups[idx][i * GROUPS_PER_MACHINE + j]->set_timer_handler(recv, conn, retry);
		}
	}
}

int GroupCollect::disable_commit_group(int idx)
{
	if (groups[idx] == NULL)
		return 0;
	for (int i = 2;
		 i < dbConfig[idx]->machineCnt * GROUPS_PER_MACHINE;
		 i += GROUPS_PER_MACHINE)
	{
		DELETE(groups[idx][i]);
	}
	return 0;
}

void GroupCollect::stat_helper_group_queue_count(HelperGroup **groups, unsigned group_count)
{
	unsigned total_queue_count = 0;
	unsigned total_queue_max_count = 0;

	for (unsigned i = 0; i < group_count; ++i)
	{
		if (groups[i])
		{
			total_queue_count += groups[i]->queue_count();
			total_queue_max_count += groups[i]->queue_max_count();
		}
	}

	statQueueCurCount = total_queue_count;
	statQueueMaxCount = total_queue_max_count;
	return;
}

int GroupCollect::get_queue_cur_max_count(int iColumn)
{
	int maxcount = 0;
	if ((iColumn < 0) || (iColumn >= GROUPS_PER_MACHINE))
	{
		return maxcount;
	}

	for (int row = 0; row < dbConfig[0]->machineCnt; row++)
	{
		/*read组是在group矩阵的第一列*/
		HelperGroup *readGroup = groups[0][GROUPS_PER_MACHINE * row + iColumn];
		if (NULL == readGroup)
		{
			continue;
		}
		if (readGroup->queue_count() > maxcount)
		{

			maxcount = readGroup->queue_count();
			log_debug("the group queue maxcount is %d ", maxcount);
		}
	}
	return maxcount;
}
/*传入请求类型，每次只根据请求类型统计响应的值*/
void GroupCollect::stat_helper_group_cur_max_queue_count(int iRequestType)
{
	/*根据请求类型分辨不出来是主读还是备读(和Workload配置有关)，只好同时即统计主读组又统计备读组了*/
	/*除非遍历group矩阵里的指针值和selectgroup后的group指针比较，然后再对比矩阵列，这个更复杂*/
	if (TaskTypeRead == iRequestType)
	{
		statReadQueueCurMaxCount = get_queue_cur_max_count(MASTER_READ_GROUP_COLUMN);
		statSlaveReadQueueMaxCount = get_queue_cur_max_count(SLAVE_READ_GROUP_COLUMN);
	}
	if (TaskTypeWrite == iRequestType)
	{
		statWriteQueueMaxCount = get_queue_cur_max_count(MASTER_WRITE_GROUP_COLUMN);
	}

	if (TaskTypeCommit == iRequestType)
	{
		statCommitQueueCurMaxCount = get_queue_cur_max_count(MASTER_COMMIT_GROUP_COLUMN);
	}
}

void GroupCollect::collect_notify_helper_reload_config(TaskRequest *task)
{
	unsigned int uiGroupNum = 0;
	unsigned int uiNullGroupNum = 0;

	for (int machineid = 0; machineid < dbConfig[0]->machineCnt; ++machineid)
	{
		HelperGroup **ptr = &groups[0][machineid * GROUPS_PER_MACHINE];
		for (int groupid = 0; groupid < GROUPS_PER_MACHINE; ++groupid)
		{
			++uiGroupNum;
			HelperGroup *pHelperGroup = ptr[groupid];
			if (NULL == pHelperGroup || GROUP_DUMMY == pHelperGroup || GROUP_READONLY == pHelperGroup)
			{
				++uiNullGroupNum;
				continue;
			}
			pHelperGroup->task_notify(task);
		}
	}
	if (uiGroupNum == uiNullGroupNum)
	{
		log_error("not have available helpergroup, please check!");
		task->set_error(-EC_NOT_HAVE_AVAILABLE_HELPERGROUP, "helper collect", "not have available helpergroup");
	}
	log_error("groupcollect notify work helper reload config finished!");
	task->reply_notify();
}
