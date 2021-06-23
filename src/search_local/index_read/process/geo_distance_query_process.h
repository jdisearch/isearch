/*
 * =====================================================================================
 *
 *       Filename:  geo_distance_query_process.h
 *
 *    Description:  geo_distance_query_process class definition.
 *
 *        Version:  1.0
 *        Created:  17/05/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef GEO_DISTANCE_QUERY_PROCESS_H_
#define GEO_DISTANCE_QUERY_PROCESS_H_

#include "query_process.h"
#include "geohash.h"

const double DEFAULT_DISTANCE = 2.0;
const int GEO_PRECISION = 6;

struct GeoPointContext
{
    std::string sLatitude;
    std::string sLongtitude;
    double d_distance;

    GeoPointContext()
        : sLatitude("")
        , sLongtitude("")
        , d_distance(DEFAULT_DISTANCE)
    {}

    GeoPointContext(const Json::Value& oJsonValue){
         ParseJson(oJsonValue);
    }

    GeoPointContext(const std::string& sLat, const std::string& sLng
            , double dDis = DEFAULT_DISTANCE)
        : sLatitude(sLat)
        , sLongtitude(sLng)
        , d_distance(dDis)
    { }

    void operator()(const Json::Value& oJsonValue)
    {
        ParseJson(oJsonValue);
    }

    void SetDistance(const double& dDis){
        d_distance = dDis;
    }

    bool IsGeoPointFormat(){
        return ((!sLatitude.empty()) && (!sLongtitude.empty()));
    }

    void Clear(){
        sLatitude.clear();
        sLongtitude.clear();
    }

private:
    void ParseJson(const Json::Value& oJsonValue){
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
};

class GeoDistanceQueryProcess: public QueryProcess{
public:
    GeoDistanceQueryProcess(const Json::Value& value);
    virtual ~GeoDistanceQueryProcess();

public:
    virtual int ParseContent(int logic_type);
    virtual int GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys);

    virtual int ParseContent();
    virtual int GetValidDoc();
    virtual int CheckValidDoc();
    virtual int GetScore();
    virtual void SortScore(int& i_sequence , int& i_rank);

private:
    GeoPointContext o_geo_point_;
    hash_double_map o_distance_;
};

#endif