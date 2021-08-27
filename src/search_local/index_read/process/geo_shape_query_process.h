#ifndef GEO_SHAPE_QUERY_PROCESS_H_
#define GEO_SHAPE_QUERY_PROCESS_H_

#include "query_process.h"
#include "geo_distance_query_process.h"

const char* const POINTS ="points";

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

        if (oJsonValue.isArray()){
            for(int i = 0; i < (int)oJsonValue.size(); i++){
                GeoPointContext o_geo_point(oJsonValue[i]);
                oGeoShapeVet.push_back(o_geo_point);
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

class GeoShapeQueryProcess : public GeoDistanceQueryProcess{
public:
    GeoShapeQueryProcess(const Json::Value& value);
    virtual~ GeoShapeQueryProcess();

public:
    virtual int ParseContent(int logic_type);
};

#endif