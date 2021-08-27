#include "consistent_hash_selector.h"
#include "log.h"

#include <alloca.h>
#include <cstring>
#include <cstdio>
#include <cassert>

#include <algorithm>

const std::string &CConsistentHashSelector::Select(uint32_t hash)
{
    static std::string empty;
    if(m_nodes.empty())
        return empty;

    std::map<uint32_t, int>::iterator iter = m_nodes.upper_bound(hash);
    if(iter != m_nodes.end())
        return m_nodeNames[iter->second];
    return m_nodeNames[m_nodes.begin()->second];
}

void CConsistentHashSelector::AddNode(const char *name)
{
    log_debug("add consistent hash node %s", name);
    if(find(m_nodeNames.begin(), m_nodeNames.end(), name) != m_nodeNames.end())
	{
			log_crit("duplicate node name: %s in ClusterConfig", name);
			abort();
	}
    m_nodeNames.push_back(name);
    int index = m_nodeNames.size() - 1;
    char *buf = (char *)alloca(strlen(name) + 16);
    for(int i = 0; i < VIRTUAL_NODE_COUNT; ++i)
    {
        snprintf(buf, strlen(name) + 16, "%s#%d", name, i);
        uint32_t value = Hash(buf, strlen(buf));
        std::map<uint32_t, int>::iterator iter = m_nodes.find(value);
        if(iter != m_nodes.end())
        {
            //hash值冲突，选取字符串中较小者
            if(m_nodeNames[iter->second] < name)
                continue;
        }
        m_nodes[value] = index;
    }
}

