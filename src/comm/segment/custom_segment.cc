#include "custom_segment.h"
#include <dlfcn.h>
#include "../log.h"

static const char *cache_configfile = "../conf/cache.conf";

CustomSegment::CustomSegment(){
    cache_config_ = new CConfig();
}

CustomSegment::~CustomSegment(){
    if(NULL != cache_config_){
        delete cache_config_;
    }
}

bool CustomSegment::Init(string word_path, string train_path){
    Segment::Init(word_path, train_path);
    if (cache_config_->ParseConfig(cache_configfile, "search_cache")) {
        log_error("no cache config or config file [%s] is error", cache_configfile);
        return false;
    }
    //读取配置文件
    const char* so = cache_config_->GetStrVal("search_cache", "WordSplitSo");
    if (so == NULL) {
        log_error("has no so.");
        return false;
    }
    char* fun = (char* )cache_config_->GetStrVal("search_cache", "WordSplitFunction");
    if (fun == NULL) {
        log_error("has no function.");
        return false;
    }
    void* dll = dlopen(so, RTLD_NOW|RTLD_GLOBAL);
    if(dll == (void*)NULL){
        log_error("dlopen(%s) error: %s", so, dlerror());
        return false;
    }
    word_split_func_ = (split_interface)dlsym(dll, fun);
    if(word_split_func_ == NULL){
        log_error("word-split plugin function[%s] not found in [%s]", fun, so);
        return false;
    }
    return true;
}

void CustomSegment::ConcreteSplit(iutf8string& phrase, uint32_t appid, vector<string>& vec){
    char res[100] = {'\0'};
    word_split_func_(phrase.stlstring().c_str(), res, 100);
    string tmp = "";
    for(int i = 0; i < strlen(res); i++){
        if(res[i] != ' '){
            tmp += res[i];
        } else {
            vec.push_back(tmp);
            tmp = "";
        }
    }
    if(tmp != ""){
        vec.push_back(tmp);
    }
}