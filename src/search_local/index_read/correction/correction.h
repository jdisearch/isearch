/*
 * =====================================================================================
 *
 *       Filename:  correction.h
 *
 *    Description:  Correction class definition.
 *
 *        Version:  1.0
 *        Created:  08/07/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __CORRECTION_H__
#define __CORRECTION_H__

#include "../comm.h"
#include "singleton.h"
#include <set>
#include <map>
#include "segment/segment.h"

class Correction{
public:
    Correction();
    virtual ~Correction();
    virtual bool Init(const string& en_word_path, const string& character_path);
    virtual int JudgeWord(uint32_t appid, const string& word, bool& is_correct, string& probably_word);
protected:
    string Correct(const string& word);
    set<string> Known(const set<string>& words);
    set<string> Edits1(const string& word);
    string ChCorrect(const string& word);
    set<string> ChKnown(const set<string>& words);
    set<string> ChEdits1(const string& word);
    bool IsChineseWord(uint32_t appid, const string& str);
    bool IsEnWord(const string& str);

protected:
    map<string, int> en_word_map_;
    vector<string> characters_;
};

#endif