/*
 * =====================================================================================
 *
 *       Filename:  priority_queue.h
 *
 *    Description:  priority queue class definition.
 *
 *        Version:  1.0
 *        Created:  02/08/2019 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef PRIORITY_QUEUE_H_
#define PRIORITY_QUEUE_H_

#include <iostream>
#include <algorithm>
#include <vector>
using namespace std;

typedef unsigned int uint32_t;
typedef pair<uint32_t, string> PAIR;

struct CmpByValue {
    bool operator()(const PAIR& lhs, const PAIR& rhs) {
        return lhs.first > rhs.first;
    }
};

class PriorityQueue {
private:
    vector<PAIR> pairs;
    int size;
    int count;
private:
    int Parent(int pos) {
        return (pos-1)/2;
    }

    int Left(int pos) {
        return pos*2 + 1;
    }

    int Right(int pos) {
        return pos*2 + 2;
    }
public:
    PriorityQueue(int len):size(0),count(len)
    {
        pairs = vector<PAIR>(len);
    }

    bool EnQueue(const PAIR& val) {
        if (size >= count) {
            if (pairs[0].first >= val.first)
                return true;
            else {
                DeQueue();
            }
        }

        int i = size;
        pairs[size++] = val;
        //插入到尾部，一直上提到使当前堆符合最小堆性质为止
        while(i > 0 && pairs[i].first < pairs[Parent(i)].first) {
            PAIR tmp = pairs[i];
            pairs[i] = pairs[Parent(i)];
            pairs[Parent(i)] = tmp;
            i = Parent(i);
        }
        return true;
    }

    bool DeQueue() {
        //取出最小值，并把尾部元素和最小值交换，然后维护当前堆到符合堆性质
        if(size < 1) {
            return false;
        }
        pairs[0] = pairs[--size];
        if (size == 0)
            return true;
        MinHeapIfy(0);
        return true;
    }

    void MinHeapIfy(int pos) {
        int minPos = pos;
        if(Left(pos) < size && pairs[pos].first > pairs[Left(pos)].first) {
            minPos = Left(pos);
        }
        if(Right(pos) < size && pairs[minPos].first > pairs[Right(pos)].first) {
            minPos = Right(pos);
        }
        if(minPos != pos) {
            PAIR tmp = pairs[pos];
            pairs[pos] = pairs[minPos];
            pairs[minPos] = tmp;
            MinHeapIfy(minPos);
        }
    }

    vector<string> GetElements() {
        vector<string> vec;
        if (size == 0) {
            return vector<string>();
        }
        if (size == 1) {
            vec.push_back(pairs[0].second);
            return vec;
        }

        vector<PAIR> remains;
        for (int i = 0; i < size; i++) {
            remains.push_back(pairs[i]);
        }
        sort(remains.begin(), remains.end(), CmpByValue());
        for (int i = 0 ;i < size; i++) {
            vec.push_back(remains[i].second);
        }
        return vec;
    }

    int GetSize() {
        return size;
    }

    bool Top(PAIR &t) {
        if(size < 1)
            return false;
        t = pairs[0];
        return true;
    }
};

#endif