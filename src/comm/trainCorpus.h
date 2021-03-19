/*
 * =====================================================================================
 *
 *       Filename:  trainCorpus.h
 *
 *    Description:  trainCorpus class definition.
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

#ifndef __TRAIN_CORPUS_H__
#define __TRAIN_CORPUS_H__
#include <map>
#include <string>
#include <stdint.h>
using namespace std;

class TrainCorpus {
public:
	TrainCorpus();
	~TrainCorpus(){}
	bool Init(string path);
	double MinEmit() {
		return min_emit;
	}

	map<char, map<char, double> > trans_dict;
	map<char, map<string, double> > emit_dict;
	map<char, double> start_dict;

private:
	map<char, uint32_t> count_dict;
	uint32_t line_num;
	double min_emit;
};
#endif
