#include "correction.h"
#include "gtest/gtest.h"
#include "../search_conf.h"
#include "../split_manager.h"
#include "../data_manager.h"
#include "log.h"

pthread_mutex_t mutex;
class CorrectionTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        stat_init_log_("correction_unittest", "../log");
        stat_set_log_level_(7);
        static char conf_filename[1024] = "../conf/index_read.conf";
        if (!SearchConf::Instance()->ParseConf(conf_filename)) {
            printf("parse conf file error: %s.\n", conf_filename);
        }
        SGlobalConfig &global_cfg = SearchConf::Instance()->GetGlobalConfig();
        if (!SplitManager::Instance()->Init(global_cfg.sWordsPath, global_cfg.sTrainPath, global_cfg.sSplitMode)) {
            printf("SplitManager init error\n");
        }

        if (!DataManager::Instance()->Init(global_cfg)){
            printf("DataManager init error\n");
        }

        correction_ = new Correction();
        correction_->Init(global_cfg.sEnWordsPath, global_cfg.sCharacterPath);
    }
    static void TearDownTestCase() {
        if(NULL != correction_){
            delete correction_;
        }
        SearchConf::Instance()->Destroy();
        DataManager::Instance()->Destroy();
        SplitManager::Instance()->Destroy();
    }

    static Correction* correction_;
};

Correction* CorrectionTest::correction_ = NULL;

TEST_F(CorrectionTest, CHINESE_) {
    bool is_correct = false;
    string probably_word;
    correction_->JudgeWord(10001, "京东", is_correct, probably_word);
    EXPECT_TRUE(is_correct);
    EXPECT_EQ("", probably_word);
    correction_->JudgeWord(10001, "京栋", is_correct, probably_word);
    EXPECT_FALSE(is_correct);
    EXPECT_EQ("京东", probably_word);
}

TEST_F(CorrectionTest, HYBRID_) {
    bool is_correct = false;
    string probably_word;
    correction_->JudgeWord(10001, "jing东", is_correct, probably_word);
    EXPECT_FALSE(is_correct);
    EXPECT_EQ("京东", probably_word);
    correction_->JudgeWord(10001, "京dong", is_correct, probably_word);
    EXPECT_FALSE(is_correct);
    EXPECT_EQ("京东", probably_word);
}

TEST_F(CorrectionTest, PHONETIC_) {
    bool is_correct = false;
    string probably_word;
    correction_->JudgeWord(10001, "jingdong", is_correct, probably_word);
    EXPECT_FALSE(is_correct);
    EXPECT_EQ("京东", probably_word);
}

TEST_F(CorrectionTest, ENGLISH_) {
    bool is_correct = false;
    string probably_word;
    correction_->JudgeWord(10001, "apple", is_correct, probably_word);
    EXPECT_TRUE(is_correct);
    EXPECT_EQ("", probably_word);
    correction_->JudgeWord(10001, "applle", is_correct, probably_word);
    EXPECT_FALSE(is_correct);
    EXPECT_EQ("apple", probably_word);
    correction_->JudgeWord(10001, "appla", is_correct, probably_word);
    EXPECT_FALSE(is_correct);
    EXPECT_EQ("applaud", probably_word);
}