/*
 * =====================================================================================
 *
 *       Filename:  parse_cluster_config.h
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
#ifndef PARSE_CLUSTER_CONFIG_H__
#define PARSE_CLUSTER_CONFIG_H__

#include <vector>
#include <string>
#include <map>
namespace ClusterConfig
{
#define CLUSTER_CONFIG_FILE "../conf/ClusterConfig.xml"

	struct ClusterNode
	{
		std::string name;
		std::string addr;
		bool self;
	};

	bool parse_cluster_config(std::string &strSelfName, std::vector<ClusterNode> *result, const char *buf, int len);
	bool parse_cluster_config(std::vector<ClusterNode> *result);
	bool save_cluster_config(std::vector<ClusterNode> *result, std::string &strSelfName);
	bool check_and_create(const char *filename = NULL);
	bool change_node_address(std::string servername, std::string newaddress);
	bool get_local_ip_by_ip_file();
	bool parse_cluster_config(std::string &strSelfName, std::map<std::string, std::string> &dtcClusterMap, const char *buf, int len);
} // namespace ClusterConfig

#endif
