/*
 * =====================================================================================
 *
 *       Filename:  dtc_tools.h
 *
 *    Description:  DTCTools class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef SRC_INDEX_GEN_DTC_TOOLS_H_
#define SRC_INDEX_GEN_DTC_TOOLS_H_
#include "index_conf.h"
#include "dtcapi.h"
#include <map>

class DTCTools{
public:
	DTCTools(){

	}
	static DTCTools *Instance()
	{
		return CSingleton<DTCTools>::Instance();
	}

	static void Destroy()
	{
		CSingleton<DTCTools>::Destroy();
	}
	int init_servers(DTC::DTCServers &servers, SDTCHost &dtc_config);
	int init_dtc_server(DTC::Server &server, const char *ip_str, const char *dtc_port,SDTCHost &dtc_config);
	DTC::Server *find_dtc_server(u_int64_t ip_port_key){
		if(dtc_handle.find(ip_port_key) != dtc_handle.end()){
			return &dtc_handle[ip_port_key];
		}else{
			return NULL;
		}
	}
	bool insert_dtc_server(u_int64_t ip_port_key, const char *ip_str, const char *port_str, SDTCHost &dtc_config);
private:
	map<u_int64_t, DTC::Server> dtc_handle;
};

struct IntelligentInfo {
	IntelligentInfo() {
		int i = 0;
		for (; i < 8; i++) {
			charact_id[i] = 0;
		}
		for (i = 0; i < 8; i++) {
			phonetic_id[i] = 0;
		}
		for (i = 0; i < 16; i++) {
			initial_char[i] = "";
		}
	}
	uint16_t charact_id[8];
	uint16_t phonetic_id[8];
	string initial_char[16];
};

struct Content {
	uint32_t type;
	string str;
};


string gen_dtc_key_string(uint32_t appid, string type, string key);
string gen_dtc_key_string(uint32_t appid, string type, uint32_t key);
string gen_dtc_key_string(uint32_t appid, string type, int64_t key);
string gen_dtc_key_string(uint32_t appid, string type, double key);
void split_func(string pinyin, string &split_str);
void get_intelligent(string str, vector<IntelligentInfo> &info_vec, bool &flag);
void convert_intelligent(const vector<Content> &result, vector<IntelligentInfo> &info_vec, bool &flag);
void convert_intelligent_alpha_num(const vector<Content> &result, vector<IntelligentInfo> &info_vec, bool &flag);
vector<string> combination(vector<vector<string> > &dimensionalArr);
vector<int> splitInt(const string& src, string separate_character);
bool noChinese(string str);
bool allChinese(string str);

#endif /* SRC_INDEX_GEN_DTC_TOOLS_H_ */
