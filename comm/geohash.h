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

struct GeoPoint {
	double lon;
	double lat;
};

string encode(double lat, double lng, int precision);
vector<string> getArroundGeoHash(double lat, double lon, int precision);
GeoPoint GetTerminalGeo(GeoPoint& beg , // 初始的geo坐标
						double distance,// 距离
						double  angle   //角度
);
vector<string> GetArroundGeoHash(GeoPoint& circle_center, double distance, int precision);
vector<string> GetArroundGeoHash(double lng_max, double lng_min, double lat_max, double lat_min, int precision);

#endif