/*
 * =====================================================================================
 *
 *       Filename:  parse_cluster_config.cc
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
#include <errno.h>
#include <stdio.h>
#include "config.h"
#include <set>

#include "parse_cluster_config.h"
#include "markup_stl.h"
#include "log.h"
#include "net_addr.h"
#include <fstream>

using namespace std;

extern int errno;
extern DTCConfig *gConfig;

namespace ClusterConfig
{

	bool parse_cluster_config(std::string &strSelfName, std::vector<ClusterNode> *result, const char *buf, int len)
	{
		log_debug("%.*s", len, buf);
		//配置中不允许有相同servername的节点出现，本set用于检查重复servername
		std::set<std::string> filter;
		pair<set<std::string>::iterator, bool> ret;

		string xmldoc(buf, len);
		MarkupSTL xml;
		if (!xml.set_doc(xmldoc.c_str()))
		{
			log_error("parse config file error");
			return false;
		}
		xml.reset_main_pos();

		if (!xml.find_elem("serverlist"))
		{
			log_error("no serverlist info");
			return -1;
		}
		strSelfName = xml.get_attrib("selfname");
		log_debug("strSelfName is %s", strSelfName.c_str());

		while (xml.find_child_elem("server"))
		{
			if (xml.into_elem())
			{
				struct ClusterNode node;
				node.name = xml.get_attrib("name");
				node.addr = xml.get_attrib("address");
				if (node.name.empty() || node.addr.empty())
				{
					log_error("name:%s addr:%s may error\n", node.name.c_str(), node.addr.c_str());
					return false;
				}

				if (strSelfName == node.name)
				{
					node.self = true;
				}
				else
				{
					node.self = false;
				}

				ret = filter.insert(node.addr);
				if (ret.second == false)
				{
					log_error("server address duplicate. name:%s addr:%s \n", node.name.c_str(), node.addr.c_str());
					return false;
				}
				result->push_back(node);
				xml.out_of_elem();
			}
		}
		if (result->empty())
			log_notice("ClusterConfig is empty,dtc runing in normal mode");
		return true;
	}

	//读取配置文件
	bool parse_cluster_config(std::vector<ClusterNode> *result)
	{
		bool bResult = false;
		FILE *fp = fopen(CLUSTER_CONFIG_FILE, "rb");
		if (fp == NULL)
		{
			log_error("load %s failed:%m", CLUSTER_CONFIG_FILE);
			return false;
		}

		// Determine file length
		fseek(fp, 0L, SEEK_END);
		int nFileLen = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		// Load string
		allocator<char> mem;
		std::string strSelfName;
		allocator<char>::pointer pBuffer = mem.allocate(nFileLen + 1, NULL);
		if (fread(pBuffer, nFileLen, 1, fp) == 1)
		{
			pBuffer[nFileLen] = '\0';
			bResult = parse_cluster_config(strSelfName, result, pBuffer, nFileLen);
		}
		fclose(fp);
		mem.deallocate(pBuffer, 1);
		return bResult;
	}

	bool parse_cluster_config(std::string &strSelfName, std::map<std::string, std::string> &dtcClusterMap, const char *buf, int len)
	{
		log_debug("%.*s", len, buf);

		string xmldoc(buf, len);
		MarkupSTL xml;
		if (!xml.set_doc(xmldoc.c_str()))
		{
			log_error("parse config file error");
			return false;
		}
		xml.reset_main_pos();

		if (!xml.find_elem("serverlist"))
		{
			log_error("no serverlist info");
			return -1;
		}
		strSelfName = xml.get_attrib("selfname");
		log_debug("strSelfName is %s", strSelfName.c_str());

		while (xml.find_child_elem("server"))
		{
			if (xml.into_elem())
			{

				std::string name = xml.get_attrib("name");
				std::string addr = xml.get_attrib("address");
				if (name.empty() || addr.empty())
				{
					log_error("name:%s addr:%s may error\n", name.c_str(), addr.c_str());
					return false;
				}

				std::pair<std::map<std::string, std::string>::iterator, bool> ret;
				ret = dtcClusterMap.insert(std::pair<std::string, std::string>(name, addr));
				if (ret.second)
				{
					log_debug("insert dtcClusterMap success: name[%s]addr[%s]", name.c_str(), addr.c_str());
				}
				else
				{
					log_error("insert dtcClusterMap fail,maybe dtc name duplicated: name[%s]addr[%s]", name.c_str(), addr.c_str());
				}

				xml.out_of_elem();
			}
		}
		return true;
	}

	//保存vector到配置文件
	bool save_cluster_config(std::vector<ClusterNode> *result, std::string &strSelfName)
	{
		MarkupSTL xml;
		xml.set_doc("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
		xml.add_elem("serverlist");
		xml.add_attrib("selfname", strSelfName.c_str());
		xml.into_elem();

		std::vector<ClusterNode>::iterator it;
		for (it = result->begin(); it < result->end(); it++)
		{
			xml.add_elem("server");
			xml.add_attrib("name", it->name.c_str());
			xml.add_attrib("address", it->addr.c_str());
		}
		return xml.Save(CLUSTER_CONFIG_FILE);
	}

	//检查是否有配置文件，如果没有则生成配置文件
	bool check_and_create(const char *filename)
	{
		if (filename == NULL)
			filename = CLUSTER_CONFIG_FILE;

		if (access(filename, F_OK))
		{
			//配置文件不存在,创建配置文件
			if (errno == ENOENT)
			{
				log_notice("%s didn't exist,create it.", filename);
				MarkupSTL xml;
				xml.set_doc("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
				xml.add_elem("serverlist");
				xml.add_attrib("selfname", "");
				xml.into_elem();
				return xml.Save(filename);
			}
			log_error("access %s error:%m\n", filename);
			return false;
		}
		else
			return true;
	}

	bool change_node_address(std::string servername, std::string newaddress)
	{
		MarkupSTL xml;
		if (!xml.Load(CLUSTER_CONFIG_FILE))
		{
			log_error("load config file: %s error", CLUSTER_CONFIG_FILE);
			return false;
		}
		xml.reset_main_pos();
		while (xml.find_child_elem("server"))
		{
			if (!xml.into_elem())
			{
				log_error("xml.into_elem() error");
				return false;
			}

			string name = xml.get_attrib("name");
			string address = xml.get_attrib("address");
			if (address == newaddress)
			{
				log_error("new address:%s is exist in Cluster Config", newaddress.c_str());
				return false;
			}

			if (!xml.out_of_elem())
			{
				log_error("xml.OutElem() error");
				return false;
			}
		}

		xml.reset_main_pos();
		while (xml.find_child_elem("server"))
		{
			if (!xml.into_elem())
			{
				log_error("xml.into_elem() error");
				return false;
			}

			string name = xml.get_attrib("name");
			if (name != servername)
			{
				if (!xml.out_of_elem())
				{
					log_error("xml.OutElem() error");
					return false;
				}
				continue;
			}

			string oldaddress = xml.get_attrib("address");
			if (oldaddress == newaddress)
			{
				log_error("server name:%s ip address:%s not change",
						  servername.c_str(), newaddress.c_str());
				return false;
			}

			if (!xml.set_attrib("address", newaddress.c_str()))
			{
				log_error("change address error.server name: %s old address: %s new address: %s",
						  servername.c_str(), oldaddress.c_str(), newaddress.c_str());
				return false;
			}
			return xml.Save(CLUSTER_CONFIG_FILE);
		}
		log_error("servername: \"%s\" not found", servername.c_str());
		return false;
	}
} // namespace ClusterConfig
