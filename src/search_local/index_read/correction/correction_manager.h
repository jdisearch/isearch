/*
 * =====================================================================================
 *
 *       Filename:  correction_manager.h
 *
 *    Description:  correction_manager class definition.
 *
 *        Version:  1.0
 *        Created:  21/07/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __CORRECTION_MANAGER_H__
#define __CORRECTION_MANAGER_H__
#include "correction.h"

class CorrectManager{
public:
    CorrectManager(){
        correction_ = new Correction();
    }
    ~CorrectManager(){
        if(NULL != correction_){
            delete correction_;
        }
    }
    static CorrectManager *Instance(){
        return CSingleton<CorrectManager>::Instance();
    }
    static void Destroy(){
        CSingleton<CorrectManager>::Destroy();
    }
    bool Init(const string& en_word_path, const string& character_path){
        return correction_->Init(en_word_path, character_path);
    }
    int Correct(uint32_t appid, string word, bool& is_correct, string& probably_word){
        return correction_->JudgeWord(appid, word, is_correct, probably_word);
    }
private:
    Correction* correction_;
};

#endif