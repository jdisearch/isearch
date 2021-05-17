#include "match_query_process.h"
#include "math.h"
#include "../order_op.h"

#define DOC_CNT 10000

MatchQueryProcess::MatchQueryProcess(uint32_t appid, Json::Value& value, Component* component)
:QueryProcess(appid, value, component){
    appid_ = component_->Appid();
    sort_type_ = component_->SortType();
    sort_field_ = component_->SortField();
    has_gis_ = false;
}

MatchQueryProcess::~MatchQueryProcess(){

}

int MatchQueryProcess::ParseContent(){
    return ParseContent(ORKEY);
}

int MatchQueryProcess::ParseContent(uint32_t type){
    vector<FieldInfo> fieldInfos;
    Json::Value::Members member = value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    string fieldname;
    Json::Value field_value;
    if(iter != member.end()){ // 一个match下只对应一个字段
        fieldname = *iter;
        field_value = value_[fieldname];
    } else {
        log_error("MatchQueryProcess error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    uint32_t segment_tag = 0;
    FieldInfo fieldInfo;
    uint32_t field = DBManager::Instance()->GetWordField(segment_tag, appid_, fieldname, fieldInfo);
    if (field != 0 && segment_tag == 1)
    {
        string split_data = SplitManager::Instance()->split(field_value.asString(), appid_);
        log_debug("split_data: %s", split_data.c_str());
        vector<string> split_datas = splitEx(split_data, "|");
        for(size_t index = 0; index < split_datas.size(); index++)
        {
            FieldInfo info;
            info.field = fieldInfo.field;
            info.field_type = fieldInfo.field_type;
            info.word = split_datas[index];
            info.segment_tag = fieldInfo.segment_tag;
            fieldInfos.push_back(info);
        }
    }
    else if (field != 0)
    {
        fieldInfo.word = field_value.asString();
        fieldInfos.push_back(fieldInfo);
    }

    component_->AddToFieldList(type, fieldInfos);
    return 0;
}

int MatchQueryProcess::GetValidDoc(){
    doc_manager_ = new DocManager(component_);
    logical_operate_ = new LogicalOperate(appid_, sort_type_, has_gis_, component_->CacheSwitch());

    for (size_t index = 0; index < component_->Keys().size(); index++)
	{
		vector<IndexInfo> doc_id_vec;
		vector<FieldInfo> fieldInfos = component_->Keys()[index];
		vector<FieldInfo>::iterator it;
		for (it = fieldInfos.begin(); it != fieldInfos.end(); it++) {
			vector<IndexInfo> doc_info;
			if ((*it).segment_tag == 3) {
				int ret = GetDocByShiftWord(*it, doc_info, appid_, highlightWord_);
				if (ret != 0) {
					doc_id_vec.clear();
					return -RT_GET_DOC_ERR;
				}
				sort(doc_info.begin(), doc_info.end());
				for (size_t doc_info_idx = 0; doc_info_idx < doc_info.size(); doc_info_idx++){
					KeyInfo info;
					info.word_freq = 1;
					info.field = (*it).field;
					info.word = (*it).word;
					doc_info_map_[doc_info[doc_info_idx].doc_id].push_back(info);
				}
			} else if ((*it).segment_tag == 4) {
				int ret = GetDocByShiftEnWord(*it, doc_info, appid_, highlightWord_);
				if (ret != 0) {
					doc_id_vec.clear();
					return -RT_GET_DOC_ERR;
				}
				sort(doc_info.begin(), doc_info.end());
				for (size_t doc_info_idx = 0; doc_info_idx < doc_info.size(); doc_info_idx++){
					KeyInfo info;
					info.word_freq = 1;
					info.field = (*it).field;
					info.word = (*it).word;
					doc_info_map_[doc_info[doc_info_idx].doc_id].push_back(info);
				}
			} else {
				int ret = logical_operate_->GetDocIdSetByWord(*it, doc_info);
				if (ret != 0){
					return -RT_GET_DOC_ERR;
				}
				if (doc_info.size() == 0)
					continue;
				if (!isAllNumber((*it).word))
					highlightWord_.insert((*it).word);
				if(sort_type_ == SORT_RELEVANCE){
					logical_operate_->CalculateByWord(*it, doc_info, doc_info_map_, key_in_doc_);
				}
			} 
			doc_id_vec = vec_union(doc_id_vec, doc_info);
		}
		if(index == 0){ // 第一个直接赋值给vecs，后续的依次与前面的进行逻辑运算
			doc_vec_.assign(doc_id_vec.begin(), doc_id_vec.end());
		} else {
			doc_vec_ = vec_union(doc_vec_, doc_id_vec);
		}
	}

    bool bRet = doc_manager_->GetDocContent(has_gis_, doc_vec_, valid_docs_, distances_);
    if (false == bRet) {
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return 0;
}

int MatchQueryProcess::GetScoreAndSort(){
    // BM25 algorithm
    uint32_t doc_cnt = DOC_CNT;
    double k1 = 1.2;
    double k2 = 200;
    double K = 1.65;
    string doc_id;
    string keyword;
    uint32_t word_freq = 0;
    uint32_t field = 0;

    if(sort_type_ == SORT_RELEVANCE || sort_type_ == SORT_TIMESTAMP){
        map<string, vec>::iterator ves_iter = doc_info_map_.begin();
        for (; ves_iter != doc_info_map_.end(); ves_iter++) {
            double score = 0;
            uint32_t key_docs = 0;
            
            doc_id = ves_iter->first;
            vector<KeyInfo> &key_info = ves_iter->second;
            if(valid_docs_.find(doc_id) == valid_docs_.end()){
                continue;
            }

            set<string> word_set;
            map<string, vector<int> > pos_map;
            map<string, vector<int> > title_pos_map;
            for (uint32_t i = 0; i < key_info.size(); i++) {
                keyword = key_info[i].word;
                if (word_set.find(keyword) == word_set.end()) {
                    word_set.insert(keyword);
                }
                word_freq = key_info[i].word_freq;
                field = key_info[i].field;
                if (field == LOCATE_ANY) {
                    pos_map[keyword] = key_info[i].pos_vec;
                }
                if (field == LOCATE_TITLE) {
                    title_pos_map[keyword] = key_info[i].pos_vec;
                }
                key_docs = key_in_doc_[keyword];
                score += log((doc_cnt - key_docs + 0.5) / (key_docs + 0.5)) * ((k1 + 1)*word_freq) / (K + word_freq) * (k2 + 1) * 1 / (k2 + 1);
            }
            /*if (!complete_keys.empty()) { // 完全匹配
                if (word_set.size() != word_vec.size()) { // 文章中出现的词语数量与输入的不一致，则不满足完全匹配
                    continue;
                }
                else { // 在标题和正文中都不连续出现，则不满足
                    if (CheckWordContinus(word_vec, pos_map) == false && CheckWordContinus(word_vec, title_pos_map) == false) {
                        continue;
                    }
                }
            }*/
            skipList_.InsertNode(score, doc_id.c_str());
        }
        
    } else {
        set<string>::iterator set_iter = valid_docs_.begin();
        for(; set_iter != valid_docs_.end(); set_iter++){
            doc_id = *set_iter;

            if (sort_type_ == SORT_FIELD_ASC || sort_type_ == SORT_FIELD_DESC){
                doc_manager_->GetScoreMap(doc_id, sort_type_, sort_field_, sort_field_type_, appid_);
            } else {
                skipList_.InsertNode(1, doc_id.c_str());
            }
        }
    }
	return 0;
}

void MatchQueryProcess::TaskEnd(){
    Json::FastWriter writer;
    Json::Value response;
    response["code"] = 0;
    int sequence = -1;
    int rank = 0;
    int page_size = component_->PageSize();
    int limit_start = page_size * (component_->PageIndex()-1);
    int limit_end = page_size * (component_->PageIndex()-1) + page_size - 1;

    log_debug("search result begin.");

    if((sort_type_ == SORT_FIELD_DESC || sort_type_ == SORT_FIELD_ASC) && skipList_.GetSize() == 0){
        OrderOpCond order_op_cond;
        order_op_cond.last_id = component_->LastId();
        order_op_cond.limit_start = limit_start;
        order_op_cond.count = page_size;
        order_op_cond.has_extra_filter = false;
        if(component_->ExtraFilterKeys().size() != 0 || component_->ExtraFilterAndKeys().size() != 0 || component_->ExtraFilterInvertKeys().size() != 0){
            order_op_cond.has_extra_filter = true;
        }
        if(sort_field_type_ == FIELDTYPE_INT){
            rank += doc_manager_->ScoreIntMap().size();
            COrderOp<int> orderOp(FIELDTYPE_INT, component_->SearchAfter(), sort_type_);
            orderOp.Process(doc_manager_->ScoreIntMap(), atoi(component_->LastScore().c_str()), order_op_cond, response, doc_manager_);
        } else if(sort_field_type_ == FIELDTYPE_DOUBLE) {
            rank += doc_manager_->ScoreDoubleMap().size();
            COrderOp<double> orderOp(FIELDTYPE_DOUBLE, component_->SearchAfter(), sort_type_);
            orderOp.Process(doc_manager_->ScoreDoubleMap(), atof(component_->LastScore().c_str()), order_op_cond, response, doc_manager_);
        } else {
            rank += doc_manager_->ScoreStrMap().size();
            COrderOp<string> orderOp(FIELDTYPE_STRING, component_->SearchAfter(), sort_type_);
            orderOp.Process(doc_manager_->ScoreStrMap(), component_->LastScore(), order_op_cond, response, doc_manager_);
        }
    } else if (has_gis_ || sort_type_ == SORT_FIELD_ASC) {
        log_debug("m_has_gis or SORT_FIELD_ASC, size:%d ", skipList_.GetSize());
        SkipListNode *tmp = skipList_.GetHeader()->level[0].forward;
        while (tmp->level[0].forward != NULL) {
            // 通过extra_filter_keys进行额外过滤（针对区分度不高的字段）
            if(doc_manager_->CheckDocByExtraFilterKey(tmp->value) == false){
                log_debug("CheckDocByExtraFilterKey failed, %s", tmp->value);
                tmp = tmp->level[0].forward;
                continue;
            }
            sequence++;
            rank++;
            if(component_->ReturnAll() == 0){
                if (sequence < limit_start || sequence > limit_end) {
                    tmp = tmp->level[0].forward;
                    continue;
                }
            }
            Json::Value doc_info;
            doc_info["doc_id"] = Json::Value(tmp->value);
            doc_info["score"] = Json::Value(tmp->key);
            response["result"].append(doc_info);
            tmp = tmp->level[0].forward;
        }
    } else {
        SkipListNode *tmp = skipList_.GetFooter()->backward;
        while(tmp->backward != NULL) {
            if(doc_manager_->CheckDocByExtraFilterKey(tmp->value) == false){
                tmp = tmp->backward;
                continue;
            }
            sequence++;
            rank++;
            if (component_->ReturnAll() == 0){
                if (sequence < limit_start || sequence > limit_end) {
                    tmp = tmp->backward;
                    continue;
                }
            }
            Json::Value doc_info;
            doc_info["doc_id"] = Json::Value(tmp->value);
            doc_info["score"] = Json::Value(tmp->key);
            response["result"].append(doc_info);
            tmp = tmp->backward;
        }
    }

    if(component_->Fields().size() > 0){
        doc_manager_->AppendFieldsToRes(response, component_->Fields());
    }

    if (rank > 0)
        AppendHighLightWord(response);
    if (has_gis_) {
        response["type"] = 1;
    }
    else {
        response["type"] = 0;
    }
    response["count"] = rank;
    /*if(m_index_set_cnt != 0){
        response["count"] = m_index_set_cnt;
    }*/
    log_debug("search result end: %lld.", (long long int)GetSysTimeMicros());
    std::string outputConfig = writer.write(response);
    request_->setResult(outputConfig);
    /*if (component_->ReturnAll() == 0 && component_->CacheSwitch() == 1 && component_->PageIndex() == 1 && has_gis_ == 0 
        && rank > 0 && outputConfig.size() < MAX_VALUE_LEN) {
        string m_Data_Cache = m_Primary_Data + "|" + component_->DataAnd() + "|" + component_->DataInvert() + "|" + component_->DataComplete() + "|" +
                                ToString(sort_type_) + "|" + ToString(appid_);
        unsigned data_size = m_Data_Cache.size();
        int ret = cachelist->add_list(m_Data_Cache.c_str(), outputConfig.c_str(), data_size, outputConfig.size());
        if (ret != 0) {
            log_error("add to cache_list error, ret: %d.", ret);
        }
        else {
            log_debug("add to cache_list: %s.", m_Data_Cache.c_str());
        }
    }*/
}

void MatchQueryProcess::AppendHighLightWord(Json::Value& response)
{
    int count = 0;
    set<string>::iterator iter = highlightWord_.begin();
    for (; iter != highlightWord_.end(); iter++) {
        if (count >= 10)
            break;
        count = count + 1;
        response["hlWord"].append((*iter).c_str());
    }
    return ;
}