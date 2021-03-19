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

#ifndef _LBS_WEBSERVICE_GET_AOIS_ACTION_H_
#define _LBS_WEBSERVICE_GET_AOIS_ACTION_H_

#include <vector>
#include <limits.h>
#include <float.h>
#include <iostream>
using namespace std;

typedef std::vector<std::vector<double> > Polygon;
typedef std::vector<double> GeometryLocation;

// 点到多边形最短距离
double GetShortestDistance(double lng, double lat, const Polygon &polygon);
double GetDistance(double lon1, double lat1, double lon2, double lat2);
bool PointInPolygon(double longitude, double latitude, const Polygon &polygon);

class DistanceOp{
public:
    DistanceOp(Polygon g0, Polygon g1, double terminateDistance){
        geom[0].assign(g0.begin(), g0.end());
        geom[1].assign(g1.begin(), g1.end());
        this->terminateDistance = terminateDistance;
        minDistance = DBL_MAX;
    }

    double distance(){
        if(geom[0].size() > 0 && geom[1].size() > 0){
            computeMinDistance();
            return minDistance;
        } else {
            return 0;
        }
    }

    void computeMinDistance(){
        computeContainmentDistance(0);
        if (minDistance > terminateDistance) {
            computeContainmentDistance(1);
        }
        if (minDistance > terminateDistance) {
            computeFacetDistance();
        }
    }

    void computeContainmentDistance(int polyGeomIndex) {
        int locationsIndex = 1 - polyGeomIndex;
        computeContainmentDistance(geom[locationsIndex], geom[polyGeomIndex]);
        if (minDistance <= terminateDistance) {
            return;
        }
    }

    void computeContainmentDistance(const Polygon &locs, const Polygon &polys) {
        for(size_t i = 0; i < locs.size(); ++i) {
            GeometryLocation loc = locs[i];

            computeContainmentDistance(loc, polys);
            if (minDistance <= terminateDistance) {
                return;
            }
        }
    }

    void computeContainmentDistance(GeometryLocation ptLoc, Polygon poly) {
        if (2 != locate(ptLoc, poly)) {
            minDistance = 0.0;
            minDistanceLocation[0].assign(ptLoc.begin(), ptLoc.end());
            minDistanceLocation[1].assign(ptLoc.begin(), ptLoc.end());
        }
    }

    int locate(GeometryLocation ptLoc, Polygon poly){
        if(poly.size() == 0){
            return 2;
        }
        // 判断点是否在多边形里面，刚好在边上或者在多边形内部返回1，在多边形外面返回2，不考虑环的情况
        if(PointInPolygon(ptLoc[0], ptLoc[1], poly)){
            return 1;
        }
        return 2;
    }

    void computeFacetDistance(){
        for(size_t i = 0; i < geom[0].size(); ++i){
            GeometryLocation p = geom[0][i];
            double distance = GetShortestDistance(p[0], p[1], geom[1]);
            if(distance < minDistance){
                minDistance = distance;
            }
            if(minDistance <= terminateDistance){
                return;
            }
        }
        for(size_t i = 0; i < geom[1].size(); ++i){
            GeometryLocation p = geom[1][i];
            double distance = GetShortestDistance(p[0], p[1], geom[0]);
            if(distance < minDistance){
                minDistance = distance;
            }
            if(minDistance <= terminateDistance){
                return;
            }
        }
    }

private:
    Polygon geom[2];
    std::vector<double> minDistanceLocation[2];
    double terminateDistance;
    double minDistance;
};

#endif