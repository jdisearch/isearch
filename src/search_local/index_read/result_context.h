#ifndef SYSTEM_STATUS_H_
#define SYSTEM_STATUS_H_

#include "comm.h"
#include "singleton.h"
#include "noncopyable.h"

class ResultContext: private noncopyable{
public:
    ResultContext();
    virtual ~ResultContext();

public:
    static ResultContext* Instance(){
        return CSingleton<ResultContext>::Instance();
    };

    static void Destroy(){
        CSingleton<ResultContext>::Destroy();
    };

public:
    void Clear();

    void SetHighLightWordSet(const std::string& highlight_word);
    const HighLightWordSet& GetHighLightWordSet() const {return highlight_word_set_;};

    void SetDocKeyinfoMap(const std::string& s_doc_id, const KeyInfo& key_info);
    const DocKeyinfosMap& GetDocKeyinfosMap() const { return docid_keyinfovet_map_;};

    void SetWordDoccountMap(const std::string& s_word, uint32_t ui_doc_count);
    uint32_t GetKeywordDoccountMap(const std::string& s_word) { return key_doccount_map_[s_word];};

    void SetIndexInfos(int logic_type , std::vector<IndexInfo>& index_info_vet);
    const std::vector<IndexInfo>& GetIndexInfos() const { return index_info_vet_;};

    void SetValidDocs(const std::string& valid_docid);
    ValidDocSet* GetValidDocs() { return &valid_docs_set_;};

private:
    void SetOrIndexInfos(std::vector<IndexInfo>& or_index_info_vet);
    void SetAndIndexInfos(std::vector<IndexInfo>& and_index_info_vet);
    void SetInvertIndexInfos(std::vector<IndexInfo>& invert_index_info_vet);

private:
    std::vector<IndexInfo> index_info_vet_;
    ValidDocSet valid_docs_set_;
    HighLightWordSet highlight_word_set_;
    DocKeyinfosMap docid_keyinfovet_map_;
    KeywordDoccountMap key_doccount_map_;
};
#endif