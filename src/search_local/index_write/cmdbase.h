#ifndef CMD_BASE_H_
#define CMD_BASE_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <iomanip>
#include <string.h>
#include <algorithm>
#include "comm.h"
#include "json/json.h"
#include "split_manager.h"
#include "dtc_tools.h"
#include "geohash.h"

struct CmdBase
{
    virtual~CmdBase(){};
    InsertParam oInsertParam;
    table_info* p_field_property;

    void SetCmdContext(const InsertParam& insert_param
        , table_info* const p_table_info){
            oInsertParam = insert_param;
            p_field_property = p_table_info;
    }

    void Clear(){
        oInsertParam.Clear();
        p_field_property = NULL;
    }
};

template<class T>
struct FieldBody
{
    T oFieldValue;
};

template<class T>
struct CmdContext : public CmdBase
{
    FieldBody<T> oFieldBody;

    bool IsSegTagVaild() {
        if (SEGMENT_CHINESE == p_field_property->segment_tag
            && false == CommonHelper::Instance()->AllChinese(oFieldBody.oFieldValue)){
            return false;
        }

        if (SEGMENT_ENGLISH == p_field_property->segment_tag
            && false == CommonHelper::Instance()->NoChinese(oFieldBody.oFieldValue)){
            return false;
        }

        return true;
    }

    void Clear(){
        CmdBase::Clear();
    }
};

struct IpContext
{
    std::string sIpContext;

    bool IsIpFormat(uint32_t& uiIpBinary){
        int iRet = inet_pton(AF_INET, sIpContext.c_str(), (void *)&uiIpBinary);
        return (iRet != 0);
    }

    void operator()(const std::string& sValue)
    {
        sIpContext = sValue;
    }

    void Clear(){
        sIpContext.clear();
    }
};

struct GeoPointContext
{
    std::string sLatitude;
    std::string sLongtitude;

    GeoPointContext()
        : sLatitude("")
        , sLongtitude("")
    {}

    GeoPointContext(const std::string& sLat, const std::string& sLng)
        : sLatitude(sLat)
        , sLongtitude(sLng)
    { }

    void operator()(const Json::Value& oJsonValue)
    {
        if (oJsonValue.isString()){
            std::string sValue = oJsonValue.asString();
            std::size_t iPos = sValue.find(",");
            sLatitude = sValue.substr(0,iPos);
            sLongtitude = sValue.substr(iPos + 1);
        }

        if (oJsonValue.isArray()){
            if (oJsonValue[0].isString()){
                sLatitude = oJsonValue[0].asString();
            }
            if (oJsonValue[1].isString()){
                sLongtitude = oJsonValue[1].asString();
            }
        }

        if (oJsonValue.isObject()){
            if (oJsonValue["latitude"].isString()){
                sLatitude = oJsonValue["latitude"].asString();
            }
            if (oJsonValue["longitude"].isString()){
                sLongtitude = oJsonValue["longitude"].asString();
            }
        }
    }

    bool IsGeoPointFormat(){
        return ((!sLatitude.empty()) && (!sLongtitude.empty()));
    }

    void Clear(){
        sLatitude.clear();
        sLongtitude.clear();
    }
};

struct GeoShapeContext
{
    std::vector<GeoPointContext> oGeoShapeVet;

    void operator()(const Json::Value& oJsonValue)
    {
        if (oJsonValue.isString()){
            std::string sValue = oJsonValue.asString();
            sValue = delPrefix(sValue);
            std::vector<std::string> oValueVet = splitEx(sValue, ",");
            for(uint32_t str_vec_idx = 0; str_vec_idx < oValueVet.size(); str_vec_idx++){
                std::string wkt_str = trim(oValueVet[str_vec_idx]);
                std::vector<std::string> wkt_vec = splitEx(wkt_str, " ");
                if(wkt_vec.size() == 2){
                    oGeoShapeVet.push_back(GeoPointContext(wkt_vec[1], wkt_vec[0]));
                }
            }
        }
    }

    EnclosingRectangle GetMinEnclosRect(){
        std::vector<double> oLatVet;
        std::vector<double> oLngVet;
        for (size_t i = 0; i < oGeoShapeVet.size(); ++i){
            oLatVet.push_back(atof(oGeoShapeVet[i].sLatitude.c_str()));
            oLngVet.push_back(atof(oGeoShapeVet[i].sLongtitude.c_str()));
        }
        if (oLatVet.empty() || oLngVet.empty()){
            return EnclosingRectangle();
        }
        
        std::sort(oLatVet.begin(), oLatVet.end());
        std::sort(oLngVet.begin(), oLngVet.end());

        return EnclosingRectangle(*(oLngVet.end() - 1), *(oLngVet.begin())
            , *(oLatVet.end() - 1), *(oLatVet.begin()));
    }

    bool IsGeoShapeFormat(){
        bool bRet = !oGeoShapeVet.empty();
        for (size_t i = 0; i < oGeoShapeVet.size(); i++)
        {
            bRet &= oGeoShapeVet[i].IsGeoPointFormat();
        }
        return bRet;
    }

    void Clear(){
        oGeoShapeVet.clear();
    }
};

#endif