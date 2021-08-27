/*
 * =====================================================================================
 *
 *       Filename:  geohash.h
 *
 *    Description:  geohash class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef GEOHASH_H_
#define GEOHASH_H_

#include <vector>
#include <string>
#include <cmath>
#include <stdlib.h>
using namespace std;

const double DOUBLE_EPS = 1e-10;

struct GeoPoint {
    double lon;
    double lat;
};

struct EnclosingRectangle{
    double dlngMax;
    double dlngMin;
    double dlatMax;
    double dlatMin;

    EnclosingRectangle()
        : dlngMax(0.0)
        , dlngMin(0.0)
        , dlatMax(0.0)
        , dlatMin(0.0)
    { }

    EnclosingRectangle(double _dlngMax, double _dlngMin
            , double _dlatMax , double _dlatMin)
        : dlngMax(_dlngMax)
        , dlngMin(_dlngMin)
        , dlatMax(_dlatMax)
        , dlatMin(_dlatMin)
    { }

    bool IsVaild(){
        return (!(fabs(dlngMax - dlngMin) < DOUBLE_EPS))
            && (!(fabs(dlatMax - dlatMin) < DOUBLE_EPS));
    }
};

string encode(double lat, double lng, int precision);
vector<string> getArroundGeoHash(double lat, double lon, int precision);
GeoPoint GetTerminalGeo(GeoPoint& beg , // 初始的geo坐标
                        double distance,// 距离
                        double  angle   //角度
);
vector<string> GetArroundGeoHash(GeoPoint& circle_center, double distance, int precision);
vector<string> GetArroundGeoHash(const EnclosingRectangle& oEnclosingRectangle, int precision);

#endif