/*
 * =====================================================================================
 *
 *       Filename:  key_route.h
 *
 *    Description:  routing from key to agent/dtc.
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
#ifndef KEY_ROUTE_H__
#define KEY_ROUTE_H__

#include <map>
#include <string>
#include <vector>

#include "request_base.h"
#include "task_request.h"
#include "consistent_hash_selector.h"
#include "parse_cluster_config.h"

class FileBackedKeySet;

class KeyRoute : public TaskDispatcher<TaskRequest>
{
public:
    KeyRoute(PollThread *owner, int keyFormat);

    void bind_remote_helper(TaskDispatcher<TaskRequest> *r)
    {
        m_remoteOutput.bind_dispatcher(r);
    }
    void bind_cache(TaskDispatcher<TaskRequest> *c)
    {
        m_cacheOutput.bind_dispatcher(c);
    }
    int key_migrated(const char *key);
    int key_migrating(const char *key);

    void Init(const std::vector<ClusterConfig::ClusterNode> &nodes);
    void task_notify(TaskRequest *t);
    int load_node_state_if_any();

private:
    RequestOutput<TaskRequest> m_cacheOutput;
    RequestOutput<TaskRequest> m_remoteOutput;
    int m_keyFormat;

    void process_reload(TaskRequest *t);
    void process_node_state_change(TaskRequest *t);
    void process_change_node_address(TaskRequest *t);
    void process_migrate(TaskRequest *t);
    void process_get_cluster_state(TaskRequest *t);
    bool accept_key(const std::string &node, const char *key, int len);
    bool is_migrating(const std::string &node, const char *key, int len);
    bool is_same_cluster_config(std::map<std::string, std::string> &dtcClusterMap, const std::string &strDtcName);
    const char *name_to_addr(const std::string &node)
    {
        return m_serverMigrationState[node].addr.c_str();
    }

    std::string key_list_file_name(const std::string &name)
    {
        return "../data/" + name + ".migrated";
    }

    std::string select_node(const char *key);

    bool migration_inprogress();
    void save_state_to_file();
    void process_cascade(TaskRequest *t);

    enum MigrationState
    {
        MS_NOT_STARTED,
        MS_MIGRATING,
        MS_MIGRATE_READONLY,
        MS_COMPLETED,
    };

    struct ServerMigrationState
    {
        std::string addr;
        int state;
        FileBackedKeySet *migrated;

        ServerMigrationState() : state(MS_NOT_STARTED), migrated(NULL) {}
    };

    typedef std::map<std::string, ServerMigrationState> MigrationStateMap;
    MigrationStateMap m_serverMigrationState;
    std::string m_selfName;
    ConsistentHashSelector m_selector;

    //级联状态枚举值，不可与上面的迁移状态枚举值重复
    enum CascadeState
    {
        CS_NOT_STARTED = 100,
        CS_CASCADING,
        CS_MAX,
    };
    //本DTC级联状态
    int m_iCSState;
    //级联对端DTC地址，例：10.191.147.188:12000/tcp
    std::string m_strCSAddr;
};

#endif
