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
    correction_->JudgeWord(10001, "荒鸟", is_correct, probably_word);
    EXPECT_FALSE(is_correct);
    EXPECT_EQ("荒岛", probably_word);
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

TEST_F(CorrectionTest, DataManager_Sensitive_) {
    bool ret = false;
    ret = DataManager::Instance()->IsSensitiveWord("兴奋剂");
    EXPECT_TRUE(ret);
    ret = DataManager::Instance()->IsSensitiveWord("京东");
    EXPECT_FALSE(ret);
}

TEST_F(CorrectionTest, DataManager_Relate_) {
    bool ret = false;
    vector<string> word_vec;
    ret = DataManager::Instance()->GetRelateWord("apple", word_vec, 1);
    EXPECT_TRUE(ret);
    EXPECT_EQ(1, word_vec.size());
    word_vec.clear();
    ret = DataManager::Instance()->GetRelateWord("banana", word_vec, 1);
    EXPECT_FALSE(ret);
    vector<vector<string> > relate_vvec;
    vector<string> relate_vec;
    relate_vec.push_back("pear");
    relate_vec.push_back("梨");
    relate_vvec.push_back(relate_vec);
    int i_ret = DataManager::Instance()->UpdateRelateWord(relate_vvec);
    EXPECT_EQ(0, i_ret);
}

TEST_F(CorrectionTest, DataManager_Synonym_) {
    vector<FieldInfo> keys;
    DataManager::Instance()->GetSynonymByKey("京东白条", keys);
    EXPECT_EQ(1, keys.size());
    keys.clear();
    DataManager::Instance()->GetSynonymByKey("金条", keys);
    EXPECT_EQ(0, keys.size());
}

TEST_F(CorrectionTest, DataManager_Word_) {
    uint32_t id;
    int ret = DataManager::Instance()->GetWordId("京东", 0, id);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(7754420, id);
}

TEST_F(CorrectionTest, DataManager_Hot_) {
    bool ret = false;
    vector<string> word_vec;
    ret = DataManager::Instance()->GetHotWord(10001, word_vec, 5);
    EXPECT_TRUE(ret);
    EXPECT_EQ(3, word_vec.size());
    map<uint32_t, map<string, uint32_t> > new_hot_map;
    map<string, uint32_t> word_map;
    word_map["apple"] = 1;
    word_map["pear"] = 3;
    word_map["banana"] = 2;
    new_hot_map[10001] = word_map;
    int i_ret = DataManager::Instance()->UpdateHotWord(new_hot_map);
    EXPECT_EQ(0, i_ret);
}