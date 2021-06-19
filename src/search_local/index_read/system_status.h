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
    const KeywordDoccountMap& GetKeywordDoccountMap() const { return key_doccount_map_;};

    void SetValidDocs(int logic_type , const ValidDocSet& valid_docs_set);
    const ValidDocSet& GetValidDocs() const { return valid_docs_set_;};

private:
    void SetOrValidDocs(const ValidDocSet& or_valid_docs_set);
    void SetAndValidDocs(const ValidDocSet& and_valid_docs_set);
    void SetInvertValidDocs(const ValidDocSet& invert_valid_docs_set);

private:
    ValidDocSet valid_docs_set_;
    HighLightWordSet highlight_word_set_;
    DocKeyinfosMap docid_keyinfovet_map_;
    KeywordDoccountMap key_doccount_map_;
};
#endif