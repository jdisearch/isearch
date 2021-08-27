/*
 * =====================================================================================
 *
 *       Filename:  dtc_tools.cc
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

#include "dtc_tools.h"
#include "split_manager.h"
#include "log.h"
#include "comm.h"
#include <iostream>
#include <sstream>
#include <string.h>

string initial_table[] = { "b","p","m","f","d","t","n","l","g","k","h","j","q","x","zh","ch","sh","r","z","c","s","y","w" };

int DTCTools::init_servers(DTC::DTCServers &servers, SDTCHost &dtc_config)
{
    int ret = 0;
    ret = servers.SetTableName(dtc_config.szTablename.c_str());
    if (0 != ret)
    {
        cout << "SetTableName error !\n";
        return ret;
    }
    std::vector<DTC::ROUTE_NODE> list;
    for(std::vector<SDTCroute>::const_iterator route_elem = dtc_config.vecRoute.begin(); route_elem != dtc_config.vecRoute.end(); route_elem++)
    {
        DTC::ROUTE_NODE route;
        route.bid = route_elem->uBid;
        route.port = route_elem->uPort;
        route.status = route_elem->uStatus;
        route.weight = route_elem->uWeight;
        memcpy(route.ip,route_elem->szIpadrr.c_str(), strlen(route_elem->szIpadrr.c_str()));
        route.ip[strlen(route_elem->szIpadrr.c_str())] = '\0';
        list.push_back(route);
    }
    ret = servers.SetRouteList(list);
    if (0 != ret) {
        cout << "SetRouteList error!\n";
        return ret;
    }

    servers.SetMTimeout(dtc_config.uTimeout);
    ret = servers.SetAccessKey(dtc_config.szAccesskey.c_str());
    if (0 != ret)
    {
        cout << "SetAccessKey error !\n";
        return ret;
    }
    ret = servers.SetKeyType(dtc_config.uKeytype);
    if (0 != ret)
    {
        cout << "SetKeyType error !\n";
        return ret;
    }
    return ret;
}

int DTCTools::init_dtc_server(DTC::Server &server, const char *ip_str, const char *dtc_port, SDTCHost &dtc_config)
{
    int ret = 0;
    ret = server.SetTableName(dtc_config.szTablename.c_str());
    if (0 != ret)
    {
        cout << "SetTableName error !\n";
        return ret;
    }
    server.SetAddress(ip_str, dtc_port);
    server.SetMTimeout(dtc_config.uTimeout);
    if(1 == dtc_config.uKeytype || 2 == dtc_config.uKeytype)
        server.IntKey();
    else
        server.StringKey();
    return ret;
}

bool DTCTools::insert_dtc_server(u_int64_t ip_port_key,const char *ip_str,const char *port_str,SDTCHost &dtc_config){
    DTC::Server s;
    init_dtc_server(s,ip_str,port_str,dtc_config);
    dtc_handle.insert(make_pair(ip_port_key,s));
    return true;
}


CommonHelper::CommonHelper()
{ }

CommonHelper::~CommonHelper()
{ }

std::string CommonHelper::GenerateDtcKey(uint32_t appid, std::string type, std::string key) {
    std::stringstream ss;
    ss << appid << "#" << type << "#" << key;
    return ss.str();
}

std::string CommonHelper::GenerateDtcKey(uint32_t appid, std::string type, uint32_t key) {
    std::stringstream ss;
    ss << appid << "#" << type << "#" << key;
    return ss.str();
}

std::string CommonHelper::GenerateDtcKey(uint32_t appid, std::string type, int64_t key) {
    std::stringstream ss;
    ss << appid << "#" << type << "#" << key;
    return ss.str();
}

std::string CommonHelper::GenerateDtcKey(uint32_t appid, std::string type, double key) {
    std::stringstream ss;
    ss << appid << "#" << type << "#" << key;
    return ss.str();
}

void CommonHelper::SplitFunc(std::string pinyin, std::string &split_str) {
    int i = 0;
    std::stringstream result;
    for (i = 0; i < (int)pinyin.size(); i++)
    {
        if (strchr("aeiouv", pinyin.at(i)))
        {
            result << pinyin.at(i);
            continue;
        }
        else
        {
            if (pinyin.at(i) != 'n')  //不是n从该辅音前分开
            {
                if (i == 0)
                {
                    result << pinyin.at(i);
                }
                else
                {
                    result << ' ' << pinyin.at(i);
                }
                if ((i + 1) < (int)pinyin.size() && (pinyin.at(i) == 'z' || pinyin.at(i) == 'c' || pinyin.at(i) == 's') &&
                    (pinyin.at(i + 1) == 'h'))
                {
                    result << 'h';
                    i++;
                }
                continue;
            }
            else                 //是n,继续向后
            {
                if (i == (int)pinyin.size() - 1)
                {
                    result << pinyin.at(i);
                    continue;
                }
                else
                    i++;   //继续向后

                if (strchr("aeiouv", pinyin.at(i)))   //如果是元音,从n前分开
                {
                    if (i == 1)
                    {
                        result << 'n' << pinyin.at(i);
                        continue;
                    }
                    else
                    {
                        result << ' ' << 'n' << pinyin.at(i);
                        continue;
                    }
                }
                //如果是辅音字母
                else
                {
                    if (pinyin.at(i) == 'g')
                    {
                        if (i == (int)pinyin.size() - 1)
                        {
                            result << 'n' << pinyin.at(i);
                            continue;
                        }
                        else
                            i++;  //继续向后

                        if (strchr("aeiouv", pinyin.at(i)))
                        {
                            result << 'n' << ' ' << 'g' << pinyin.at(i);
                            continue;
                        }
                        else
                        {
                            result << 'n' << 'g' << ' ' << pinyin.at(i);
                            if ((i + 1) < (int)pinyin.size() && (pinyin.at(i) == 'z' || pinyin.at(i) == 'c' || pinyin.at(i) == 's') &&
                                (pinyin.at(i + 1) == 'h'))
                            {
                                result << 'h';
                                i++;
                            }
                            continue;
                        }
                    }
                    else   //不是g的辅音字母,从n后分开
                    {
                        result << 'n' << ' ' << pinyin.at(i);
                        if ((i + 1) < (int)pinyin.size() && (pinyin.at(i) == 'z' || pinyin.at(i) == 'c' || pinyin.at(i) == 's') &&
                            (pinyin.at(i + 1) == 'h'))
                        {
                            result << 'h';
                            i++;
                        }
                        continue;
                    }
                }
            }
        }
    }
    split_str = result.str();
}

void CommonHelper::ConvertIntelligentAlphaNum(const std::vector<Content> &result, std::vector<IntelligentInfo> &info_vec, bool &flag) {
    int i = 0;
    flag = true;
    IntelligentInfo basic_info;
    vector<Content>::const_iterator content_iter = result.begin();
    for (; content_iter != result.end(); content_iter++, i++) {
        basic_info.initial_char[i] = ((*content_iter).str)[0];
    }
    info_vec.push_back(basic_info);
}

void CommonHelper::ConvertIntelligent(const std::vector<Content> &result, std::vector<IntelligentInfo> &info_vec, bool &flag) {
    int i = 0;
    flag = true;
    IntelligentInfo basic_info;
    vector<vector<string> > phonetic_id_vecs;
    vector<uint32_t> length_vec;
    vector<Content>::const_iterator content_iter = result.begin();
    for (; content_iter != result.end(); content_iter++, i++) {
        uint32_t charact_id = 0;
        uint32_t phonetic_id = 0;
        vector<string> phonetic_id_vec;
        if ((*content_iter).type == CHINESE) { // 查找字id
            SplitManager::Instance()->GetCharactId((*content_iter).str, charact_id);
            basic_info.charact_id[i] = charact_id;
            vector<string> vec = SplitManager::Instance()->GetPhonetic((*content_iter).str);
            if (vec.size() == 1) {
                phonetic_id_vec.push_back(vec[0]);
            }
            else if (vec.size() > 1) {  // 多音字
                int j = 0;
                for (; j < (int)vec.size(); j++) {
                    SplitManager::Instance()->GetPhoneticId(vec[j], phonetic_id);
                    phonetic_id_vec.push_back(vec[j]);
                }
            }
            phonetic_id_vecs.push_back(phonetic_id_vec);
            length_vec.push_back(phonetic_id_vec.size());
        }
        else {
            basic_info.initial_char[i] = (*content_iter).str[0];
        }
    }

    /*
    * 以下为计算不定长数组取值交叉遍历组合生成的算法
    * 如词语 重传 会生成以下4种组合
    * chongchuan chongzhuan zhongchuan zhongzhuan
    * 由于词语本身长度不固定，每个字对应的多音字数量不固定，因此采用该方法
    * 示例：
    * int factor[3][4] =
    *    {
    *        {0, 1, 2, 3}, 
    *        {0, 1}, 
    *        {0, 1, 2}, 
    *    };
    * 将位置3x2x4的24种组合理解为[0-2] [0-1] [0-3]的三个方框的组合方式，把每个方框看成一
    * 位的话，那个方框就使用了一个固定的进制，所以0 - 23 之间的值都可以用三个位表示，
    * 每一位就代表在每个方框中的取值，也即在二维数组中的位置。
    * 而0 - 23这24个值恰好覆盖了三个方框所有种组合，所以用这种多进制组合位的方式可以实现多组值的交叉遍历。 
    */
    i = 0;
    int j = 0;
    int k = 0;
    int len = 0;
    int len_num = 0;
    int totalLength = 1;
    uint32_t phonetic_id = 0;
    int colum = phonetic_id_vecs.size();
    for (i = 0; i < colum; i++)
    {
        totalLength *= length_vec[i];
    }
    for (i = 0; i < totalLength; i++) {
        k = i;
        len_num = 0;
        IntelligentInfo info = basic_info;
        for (j = 0; j < colum; j++) {
            len = length_vec[len_num];
            string phonetic = phonetic_id_vecs[j][k % len];
            SplitManager::Instance()->GetPhoneticId(phonetic, phonetic_id);
            info.phonetic_id[j] = phonetic_id;
            if (phonetic.size() > 1) {
                info.initial_char[j] = phonetic[0];
            }
            k = k / len;
            len_num++;
        }
        info_vec.push_back(info);
    }
    if (info_vec.size() == 0 && phonetic_id_vecs.size() == 0) {
        info_vec.push_back(basic_info);
    }
}

void CommonHelper::GetIntelligent(std::string str, std::vector<IntelligentInfo> &info_vec, bool &flag) {
    vector<Content> result;
    set<string> initial_vec(initial_table, initial_table + 23);
    iutf8string utf8_str(str);
    int i = 0;
    if (NoChinese(str)) {
        for (; i < (int)str.length(); i++) {
            Content content;
            content.str = str[i];
            content.type = INITIAL;
            result.push_back(content);
        }
        if (result.size() >= 16){
            log_info("content length[%d] must be less than 16", (int)result.size());
            flag = false;
            return;
        }
        ConvertIntelligentAlphaNum(result, info_vec, flag);
    }
    else{
        for (; i < utf8_str.length(); ) {
            if (utf8_str[i].size() > 1) {
                Content content;
                content.type = CHINESE;
                content.str = utf8_str[i];
                result.push_back(content);
                i++;
            }
            else {
                Content content;
                content.type = INITIAL;
                content.str = utf8_str[i];
                result.push_back(content);
                i++;
            }
        }
        if (result.size() >= 8){
            log_info("content length[%d] must be less than 8", (int)result.size());
            flag = false;
            return;
        }
        ConvertIntelligent(result, info_vec, flag);
    }
}

bool CommonHelper::NoChinese(std::string str) {
    iutf8string utf8_str(str);
    if (utf8_str.length() == (int)str.length()) {
        return true;
    }
    else {
        return false;
    }
}

bool CommonHelper::AllChinese(std::string str) {
    iutf8string utf8_str(str);
    for (int i = 0; i < utf8_str.length(); i++) {
        if (utf8_str[i].length() == 1) {
            return false;
        }
    }
    return true;
}

/*
** 通过递归求出二维vector每一维vector中取一个数的各种组合
** 输入：[[a],[b1,b2],[c1,c2,c3]]
** 输出：[a_b1_c1,a_b1_c2,a_b1_c3,a_b2_c1,a_b2_c2,a_b2_c3]
*/
std::vector<std::string> CommonHelper::Combination(std::vector<std::vector<std::string> > &dimensionalArr){
    int FLength = dimensionalArr.size();
    if(FLength >= 2){
        int SLength1 = dimensionalArr[0].size();
        int SLength2 = dimensionalArr[1].size();
        int DLength = SLength1 * SLength2;
        vector<string> temporary(DLength);
        int index = 0;
        for(int i = 0; i < SLength1; i++){
            for (int j = 0; j < SLength2; j++) {
                temporary[index].append(dimensionalArr[0][i]);
                temporary[index].append(dimensionalArr[1][j]);
                // log_debug("combine:%s , combineLeng:%d, pre:%s ,preLength:%d, post:%s .postLeng:%d" 
                //      , temporary[index].c_str() , temporary[index].length()
                //      , dimensionalArr[0][i].c_str() , dimensionalArr[0][i].length()
                //      , dimensionalArr[1][j].c_str() , dimensionalArr[1][j].length());
                index++;
            }
        }
        vector<vector<string> > new_arr;
        new_arr.push_back(temporary);
        for(int i = 2; i < (int)dimensionalArr.size(); i++){
            new_arr.push_back(dimensionalArr[i]);
        }
        return Combination(new_arr);
    } else {
        return dimensionalArr[0];
    }
}

std::vector<int> CommonHelper::SplitInt(const std::string& src, std::string separate_character)
{
    vector<int> strs;

    //分割字符串的长度,这样就可以支持如“,,”多字符串的分隔符
    int separate_characterLen = separate_character.size();
    int lastPosition = 0, index = -1;
    string str;
    int pos = 0;
    while (-1 != (index = src.find(separate_character, lastPosition)))
    {
        if (src.substr(lastPosition, index - lastPosition) != " ") {
            str = src.substr(lastPosition, index - lastPosition);
            pos = atoi(str.c_str());
            strs.push_back(pos);
        }
        lastPosition = index + separate_characterLen;
    }
    string lastString = src.substr(lastPosition);//截取最后一个分隔符后的内容
    if (!lastString.empty() && lastString != " "){
        pos = atoi(lastString.c_str());
        strs.push_back(pos);//如果最后一个分隔符后还有内容就入队
    }
    return strs;
}