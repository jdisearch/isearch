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
#include "singleton.h"
#include "noncopyable.h"

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

class CommonHelper : private noncopyable
{
public:
    CommonHelper();
    virtual ~CommonHelper();

public:
    static CommonHelper* Instance()
    {
        return CSingleton<CommonHelper>::Instance();
    }

    static void Destroy()
    {
        CSingleton<CommonHelper>::Destroy();
    };

public:
    std::string GenerateDtcKey(uint32_t appid, std::string type, std::string key);
    std::string GenerateDtcKey(uint32_t appid, std::string type, uint32_t key);
    std::string GenerateDtcKey(uint32_t appid, std::string type, int64_t key);
    std::string GenerateDtcKey(uint32_t appid, std::string type, double key);
    void SplitFunc(std::string pinyin, std::string &split_str);
    void GetIntelligent(std::string str, std::vector<IntelligentInfo> &info_vec, bool &flag);
    std::vector<std::string> Combination(std::vector<std::vector<std::string> > &dimensionalArr);
    std::vector<int> SplitInt(const std::string& src, std::string separate_character);
    bool NoChinese(std::string str);
    bool AllChinese(std::string str);

private:
    void ConvertIntelligent(const std::vector<Content> &result, std::vector<IntelligentInfo> &info_vec, bool &flag);
    void ConvertIntelligentAlphaNum(const std::vector<Content> &result, std::vector<IntelligentInfo> &info_vec, bool &flag);
};

#endif /* SRC_INDEX_GEN_DTC_TOOLS_H_ */
