#include "geohash.h"
#include <set>

const double EquatorRadius = 6378137; //赤道半径 km
const double PoleRaduis = 6356725; //极半径 km
const double PI = 3.1415926;

static double MINLAT = -90;
static double MAXLAT = 90;
static double MINLNG = -180;
static double MAXLNG = 180;

vector<bool> getBits(double lat, double floor, double ceiling, int bit_count) {
    vector<bool> buffer;
    for(int i = 0; i < bit_count; i++){
        double mid = (floor + ceiling) / 2.0;
        if(lat + 1e-14 < mid){
            buffer.push_back(false);
            ceiling = mid;
        } else {
            buffer.push_back(true);
            floor = mid;
        }
    }
    return buffer;
}

string base32(long i) {
    char digits[] = "0123456789bcdefghjkmnpqrstuvwxyz";
    char buf[65] = {'\0'};
    int char_pos = 64;
    bool negative = (i < 0);
    if(negative == false){
        i = -i;
    }
    while(i <= -32){
        buf[char_pos--] = digits[(int) (-(i % 32))];
        i /= 32;
    }
    buf[char_pos] = digits[(int) (-i)];
    if (negative){
        buf[--char_pos] = '-';
    }
    string res(buf + char_pos, buf + 65);
    return res;
}

string encode(double lat, double lng, int precision){
    string buffer;
    vector<bool> lat_bits = getBits(lat, -90, 90, floor(precision * 5 / 2.0));
    vector<bool> lng_bits = getBits(lng, -180, 180, ceil(precision * 5 / 2.0));
    int i = 0;
    for (; i < floor(precision * 5 / 2.0); i++) {
        buffer.append(lng_bits[i] ? "1" : "0");
        buffer.append(lat_bits[i] ? "1" : "0");
    }
    if(precision % 2 != 0){
        buffer.append(lng_bits[i] ? "1" : "0");
    }
    char* stop = NULL;
    long res = strtol(buffer.c_str(), &stop, 2);
    return base32(res);
}

void getMinLatLng(int precision, double &min_lat, double &min_lng){
    min_lat = MAXLAT - MINLAT;
    for (int i = 0; i < floor(precision * 5 / 2.0); i++) {
        min_lat /= 2.0;
    }
    min_lng = MAXLNG - MINLNG;
    for (int i = 0; i < ceil(precision * 5 / 2.0); i++) {
        min_lng /= 2.0;
    }
    return;
}

vector<string> getArroundGeoHash(double lat, double lon, int precision){
    double min_lat = 0;
    double min_lng = 0;
    getMinLatLng(precision, min_lat, min_lng);
    vector<string> list;
    double uplat = lat + min_lat;
    double downLat = lat - min_lat;

    double leftlng = lon - min_lng;
    double rightLng = lon + min_lng;

    string leftUp = encode(uplat, leftlng, precision);
    list.push_back(leftUp);

    string leftMid = encode(lat, leftlng, precision);
    list.push_back(leftMid);

    string leftDown = encode(downLat, leftlng, precision);
    list.push_back(leftDown);

    string midUp = encode(uplat, lon, precision);
    list.push_back(midUp);

    string midMid = encode(lat, lon, precision);
    list.push_back(midMid);

    string midDown = encode(downLat, lon, precision);
    list.push_back(midDown);

    string rightUp = encode(uplat, rightLng, precision);
    list.push_back(rightUp);

    string rightMid = encode(lat, rightLng, precision);
    list.push_back(rightMid);

    string rightDown = encode(downLat, rightLng, precision);
    list.push_back(rightDown);

    return list;
}

GeoPoint GetTerminalGeo(GeoPoint& beg, double distance, double angle)
{
    GeoPoint end;
    double dx = distance * 1000 * sin((angle * PI )/ 180.0);
    double dy = distance * 1000 * cos((angle * PI) / 180.0);

    double ec = PoleRaduis + (EquatorRadius - PoleRaduis) * (90.0 - beg.lat) / 90.0;
    double ed = ec * cos(beg.lat * PI / 180);

    end.lon = (dx / ed + beg.lon * PI / 180.0) * 180.0 / PI;
    end.lat = (dy / ed + beg.lat * PI / 180.0) * 180.0 / PI;
    return end;
}

vector<string> GetArroundGeoHash(GeoPoint& circle_center, double distance, int precision)
{
    vector<string> list;
    GeoPoint geo_north = GetTerminalGeo(circle_center, distance, 0);
    GeoPoint geo_east = GetTerminalGeo(circle_center, distance, 90);
    GeoPoint geo_south = GetTerminalGeo(circle_center, distance, 180);
    GeoPoint geo_west = GetTerminalGeo(circle_center, distance, 270);
    GeoPoint top_left, bottom_right;
    top_left.lat = geo_north.lat;
    top_left.lon = geo_west.lon;
    bottom_right.lat = geo_south.lat;
    bottom_right.lon = geo_east.lon;
    double  min_lat, min_lon;
    std::set <std::string> result;
    getMinLatLng(precision, min_lat, min_lon);
    min_lat = min_lat < (top_left.lat -  bottom_right.lat - 0.00001) ? min_lat : top_left.lat -  bottom_right.lat - 0.00001;
    min_lon = min_lon < (bottom_right.lon - top_left.lon  - 0.00001) ? min_lon : bottom_right.lon - top_left.lon  - 0.00001;
    for(;top_left.lat >= bottom_right.lat; top_left.lat -= min_lat){
        for(; top_left.lon <= bottom_right.lon; top_left.lon += min_lon){
            //std::cout << top_left.lat << " " << top_left.lon << " " <<  encode(top_left.lat, top_left.lon, precision) << std::endl;
            string geohash = encode(top_left.lat, top_left.lon, precision);
            if(result.find(geohash) == result.end()){
                result.insert(geohash);
                list.push_back(geohash);
            }
        }
        top_left.lon = geo_west.lon;
    }
    return list;
}

vector<string> GetArroundGeoHash(double lng_max, double lng_min, double lat_max, double lat_min, int precision)
{
    vector<string> list;
    GeoPoint top_left, bottom_right;
    top_left.lat = lat_max;
    top_left.lon = lng_min;
    bottom_right.lat = lat_min;
    bottom_right.lon = lng_max;
    double  min_lat, min_lon;
    std::set <std::string> result;
    getMinLatLng(precision, min_lat, min_lon);
    min_lat = min_lat < (top_left.lat -  bottom_right.lat - 0.00001) ? min_lat : top_left.lat -  bottom_right.lat - 0.00001;
    min_lon = min_lon < (bottom_right.lon - top_left.lon  - 0.00001) ? min_lon : bottom_right.lon - top_left.lon  - 0.00001;
    for(;top_left.lat >= bottom_right.lat; top_left.lat -= min_lat){
        for(; top_left.lon <= bottom_right.lon; top_left.lon += min_lon){
            string geohash = encode(top_left.lat, top_left.lon, precision);
            if(result.find(geohash) == result.end()){
                result.insert(geohash);
                list.push_back(geohash);
            }
        }
        top_left.lon = lng_min;
    }
    return list;
}

