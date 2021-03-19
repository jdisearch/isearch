/*
 * =====================================================================================
 *
 *       Filename:  get_aois_action.h
 *
 *    Description:  aoi related definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "get_aois_action.h"

#include <math.h>

const double kEarthRadius = 6378137;

// 点到多边形最短距离
double GetShortestDistance(double lng, double lat, const Polygon &polygon)
{
    double mindistance = 0;
    vector<double> point;
    for (size_t i = 0; i < polygon.size() - 1; i++)
    {
        double ab[2] = {polygon[i + 1][0] - polygon[i][0], polygon[i + 1][1] - polygon[i][1]};
        double ap[2] = {lng - polygon[i][0], lat - polygon[i][1]};
        double ab_ap = ab[0] * ap[0] + ab[1] * ap[1];
        double ab_2 = ab[0] * ab[0] + ab[1] * ab[1];

        vector<double> d(2, 0);
        if (ab_2 != 0)
        {
            double t = ab_ap / ab_2;
            if (t > 1)
            {
                d = polygon[i + 1];
            }
            else if (t > 0)
            {
                d[0] = polygon[i][0] + ab[0] * t;
                d[1] = polygon[i][1] + ab[1] * t;
            }
            else
            {
                d = polygon[i];
            }
        }
        double pd[2] = {d[0] - lng, d[1] - lat};
        double pd_2 = pd[0] * pd[0] + pd[1] * pd[1];
        if (mindistance == 0 || mindistance > pd_2)
        {
            mindistance = pd_2;
            point = d;
        }
    }
    return GetDistance(lng, lat, point[0], point[1]);
}

double GetDistance(double lon1, double lat1, double lon2, double lat2)
{
    double radlat1 = lat1 * M_PI / 180.0;
    double radlat2 = lat2 * M_PI / 180.0;
    double a = radlat1 - radlat2;
    double b = (lon1 - lon2) * M_PI / 180.0;
    double s = 2 * asin(sqrt(pow(sin(a / 2), 2) + cos(radlat1) * cos(radlat2) * pow(sin(b / 2), 2)));
    s = s * kEarthRadius;
    return s;
}

// 射线法判断点是否在多边形内部
bool PointInPolygon(double longitude, double latitude, const Polygon &polygon)
{
    int cross = 0;
    for (size_t i = 0; i < polygon.size() - 1; i++)
    {
        double lng1 = polygon[i][0];
        double lat1 = polygon[i][1];
        double lng2 = polygon[i + 1][0];
        double lat2 = polygon[i + 1][1];
        if (lat1 == lat2)
            continue;
        if (latitude < min(lat1, lat2))
            continue;
        if (latitude >= max(lat1, lat2))
            continue;
        double x = (latitude - lat1) * (lng2 - lng1) / (lat2 - lat1) + lng1;
        if (x > longitude)
        {
            cross++;
        }
    }
    return cross % 2 == 1;
}