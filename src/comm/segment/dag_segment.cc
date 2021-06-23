#include "dag_segment.h"
#include <math.h>
#include <float.h>

DagSegment::DagSegment()
{
}

DagSegment::~DagSegment()
{
}

void DagSegment::ConcreteSplit(iutf8string& sentence, uint32_t appid, vector<string>& vec){
    map<uint32_t, vector<uint32_t> > dag_map;
    getDag(sentence, appid, dag_map);
    map<uint32_t, RouteValue> route;
    calc(sentence, dag_map, route, appid);
    uint32_t N = sentence.length();
    uint32_t i = 0;
    while (i < N) {
        uint32_t j = route[i].idx + 1;
        string l_word = sentence.substr(i, j - i);
        vec.push_back(l_word);
        i = j;
    }

    return;
}

void DagSegment::getDag(iutf8string& utf8_str, uint32_t appid, map<uint32_t, vector<uint32_t> >& map_dag) {
    uint32_t N = utf8_str.length();
    for (uint32_t k = 0; k < N; k++) {
        uint32_t i = k;
        vector<uint32_t> tmplist;
        string frag = utf8_str[k];
        while (i < N) {
            if (wordValid(frag, appid) == true) {
                tmplist.push_back(i);
            }
            i++;
            frag = utf8_str.substr(k, i + 1 - k);
        }
        if (tmplist.empty()) {
            tmplist.push_back(k);
        }
        map_dag[k] = tmplist;
    }
    return;
}

void DagSegment::calc(iutf8string& utf8_str, const map<uint32_t, vector<uint32_t> >& map_dag, map<uint32_t, RouteValue>& route, uint32_t appid) {
    uint32_t N = utf8_str.length();
    RouteValue route_N;
    route[N] = route_N;
    double logtotal = log(TOTAL);
    for (int i = N - 1; i > -1; i--) {
        vector<uint32_t> vec = map_dag.at(i);
        double max_route = -DBL_MAX;
        uint32_t max_idx = 0;
        for (size_t t = 0; t < vec.size(); t++) {
            string word = utf8_str.substr(i, vec[t] + 1 - i);
            WordInfo word_info;
            uint32_t word_freq = 1;
            if (word_dict_.find(word) != word_dict_.end()) {
                map<uint32_t, WordInfo> wordInfo = word_dict_[word];
                if (wordInfo.find(0) != wordInfo.end()) {
                    word_info = wordInfo[0];
                }
                if (wordInfo.find(appid) != wordInfo.end()) {
                    word_info = wordInfo[appid];
                }
                word_freq = word_info.word_freq;
            }
            double route_value = log(word_freq) - logtotal + route[vec[t] + 1].max_route;
            if (route_value > max_route) {
                max_route = route_value;
                max_idx = vec[t];
            }
        }
        RouteValue route_value;
        route_value.max_route = max_route;
        route_value.idx = max_idx;
        route[i] = route_value;
    }
}