#include "type_parser_manager.h"
#include "type_parser_base.h"
#include "memcheck.h"
#include "geohash.h"
#include "index_tbl_op.h"

TypeParserManager::TypeParserManager()
    : m_oTypeCmdContMap()
    , m_oTypeParserMap()
{
    InitCmdContMap();
    InitTypeParserMap();
}

TypeParserManager::~TypeParserManager()
{
    std::map<int , CmdBase*>::iterator pCmdBaseIter = m_oTypeCmdContMap.begin();
    for ( ; pCmdBaseIter != m_oTypeCmdContMap.end(); ++pCmdBaseIter)
    {
        DELETE(pCmdBaseIter->second);
    }
    m_oTypeCmdContMap.clear();

    std::map<int , TypeParserBase*>::iterator pTypeParserBaseIter = m_oTypeParserMap.begin();
    for ( ; pTypeParserBaseIter != m_oTypeParserMap.end(); ++pTypeParserBaseIter)
    {
        DELETE(pTypeParserBaseIter->second);
    }
    m_oTypeParserMap.clear();
}

int TypeParserManager::HandleTableContent(int iAppid, Json::Value& oJsonTableContent)
{
    UserTableContent oContentFields(iAppid);
    Json::Value oJsonFields;
    int iCmd = DecodeFields(oJsonTableContent, oJsonFields, oContentFields);

    switch (iCmd)
    {
    case RT_CMD_ADD:
        {
            // 更新TransVersion值，表示准备开始插入索引
            int iTransVersion = 0;
            int i_error_code = UpdateTransVersion(oContentFields , iTransVersion);
            if (i_error_code != 0) { return i_error_code; }

            //每次新的请求，需初始化各个Parser的成员变量
            std::map<int , TypeParserBase*>::iterator pTypeParserIter = m_oTypeParserMap.begin();
            for ( ; pTypeParserIter != m_oTypeParserMap.end(); ++pTypeParserIter){
                pTypeParserIter->second->CleareOneReq();
            }
            //参考app_field_define表定义，判断哪些字段内容属于快照信息
            Json::Value oSnapShotCont;
            GetSnapshotCont(iAppid , oJsonFields , oSnapShotCont);
            Json::FastWriter ex_writer;
            std::string s_extend = ex_writer.write(oSnapShotCont);
            //InsertParam存储建索引时所需的字段信息
            InsertParam oInsertParam(iAppid , oContentFields.doc_id 
                                ,iTransVersion ,iTransVersion);
            //遍历检索写各字段，建索引
            Json::Value fieldid_formatkey;
            Json::Value::Members oMember = oJsonFields.getMemberNames();
            Json::Value::Members::iterator iter = oMember.begin();
            for (; iter != oMember.end(); ++iter){
                table_info* pTbInfo = SplitManager::Instance()->get_table_info(iAppid, *iter);
                if(NULL == pTbInfo) { continue; }
                if (1 == pTbInfo->index_tag){
                    if (SEGMENT_FEATURE_SNAPSHOT == pTbInfo->segment_feature){
                        oInsertParam.sExtend = s_extend;
                    }else{
                        oInsertParam.sExtend.clear();
                    }
                    oInsertParam.field_id = pTbInfo->field_id;

                    m_oTypeCmdContMap[pTbInfo->field_type]->SetCmdContext(oInsertParam , pTbInfo);
                    if (!ConvertFieldValue2Cmd(pTbInfo->field_type , oJsonFields[*iter])){
                        i_error_code = -RT_ERROR_CMD_TRANSFORM;
                        break;
                    }

                    TypeParserBase* pCurrentTypeParser = m_oTypeParserMap[pTbInfo->field_type];
                    if (NULL == pCurrentTypeParser){
                        log_error("TypeParser is not exist, please check TypeParserMap");
                        continue;
                    }
                    if (pCurrentTypeParser->SendCmd(m_oTypeCmdContMap[pTbInfo->field_type])){
                        i_error_code = pCurrentTypeParser->StartParser(fieldid_formatkey);
                        if (i_error_code != 0){
                            log_error("TypeParser has error , internal errorid:%d" , i_error_code);
                            break;
                        }
                    }else{
                        i_error_code = pCurrentTypeParser->GetErrorCode();
                        break;
                    }
                }
            }
            //创建联合索引，并更新“20”和“10”的相关信息
            if (0 == i_error_code){
                HandleUnifiedIndex(oInsertParam , fieldid_formatkey);
                oInsertParam.sExtend = s_extend; // "10"需要快照信息
                i_error_code = UpdateDocVersion(oInsertParam , fieldid_formatkey , oContentFields);
            }
            //如果有错误，需回滚操作，删除相关倒排索引
            if (i_error_code != 0){
                RollBackIndex(fieldid_formatkey , oInsertParam);
                return i_error_code;
            }
        }
        break;
    case RT_CMD_DELETE:
        {
            return DeleteDtcFields(oContentFields);
        }
        break;
    default:
        break;
    }

    return 0;
}

void TypeParserManager::InitCmdContMap()
{
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_INT ,         new CmdContext<int>()));
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_STRING ,      new CmdContext<std::string>()));
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_TEXT ,        new CmdContext<std::string>()));
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_IP ,          new CmdContext<IpContext>()));
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_GEO_POINT ,   new CmdContext<GeoPointContext>()));
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_DOUBLE ,      new CmdContext<double>()));
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_LONG ,        new CmdContext<int64_t>()));
    m_oTypeCmdContMap.insert(std::make_pair(FIELD_GEO_SHAPE ,   new CmdContext<GeoShapeContext>()));
}

void TypeParserManager::InitTypeParserMap()
{
    m_oTypeParserMap.insert(std::make_pair(FIELD_INT ,          new FigureParser<int>()));
    m_oTypeParserMap.insert(std::make_pair(FIELD_STRING ,       new StrParser()));
    m_oTypeParserMap.insert(std::make_pair(FIELD_TEXT ,         new StrParser()));
    m_oTypeParserMap.insert(std::make_pair(FIELD_IP ,           new IpParser()));
    m_oTypeParserMap.insert(std::make_pair(FIELD_GEO_POINT ,    new GeoPointParser()));
    m_oTypeParserMap.insert(std::make_pair(FIELD_DOUBLE ,       new FigureParser<double>()));
    m_oTypeParserMap.insert(std::make_pair(FIELD_LONG ,         new FigureParser<int64_t>()));
    m_oTypeParserMap.insert(std::make_pair(FIELD_GEO_SHAPE ,    new GeoShapeParser()));
}

bool TypeParserManager::ConvertFieldValue2Cmd(int field_type, const Json::Value& oValue)
{
    switch (field_type)
    {
    case FIELD_INT:
        {
            CmdContext<int>* pIntCmd = dynamic_cast<CmdContext<int>* >(m_oTypeCmdContMap[field_type]);
            if (NULL == pIntCmd){
                return false;
            }
            if (oValue.isInt()){
                pIntCmd->oFieldBody.oFieldValue = oValue.asInt();
            }else{
                return false;
            }
        }
        break;
    case FIELD_DOUBLE:
        {
            CmdContext<double>* pDoubleCmd = dynamic_cast<CmdContext<double>* >(m_oTypeCmdContMap[field_type]);
            if (NULL == pDoubleCmd){
                return false;
            }
            if (oValue.isDouble()){
                pDoubleCmd->oFieldBody.oFieldValue = oValue.asDouble();
            }else{
                return false;
            }
        }
        break;
    case FIELD_LONG:
        {
            CmdContext<int64_t>* pInt64Cmd = dynamic_cast<CmdContext<int64_t>* >(m_oTypeCmdContMap[field_type]);
            if (NULL == pInt64Cmd){
                return false;
            }
            if (oValue.isInt64()){
                pInt64Cmd->oFieldBody.oFieldValue = oValue.asInt64();
            }else{
                return false;
            }
        }
        break;
    case FIELD_STRING:
    case FIELD_TEXT:
        {
            CmdContext<std::string>* pStrCmd = dynamic_cast<CmdContext<std::string>* >(m_oTypeCmdContMap[field_type]);
            if (NULL == pStrCmd){
                return false;
            }
            if (oValue.isString()){
                pStrCmd->oFieldBody.oFieldValue = oValue.asString();
            }else{
                return false;
            }
        }
        break;
    case FIELD_IP:
        {
            CmdContext<IpContext>* pIpCmd = dynamic_cast<CmdContext<IpContext>* >(m_oTypeCmdContMap[field_type]);
            if (NULL == pIpCmd){
                return false;
            }
            if (oValue.isString()){
                pIpCmd->oFieldBody.oFieldValue(oValue.asString());
            }else{
                return false;
            }
        }
        break;
    case FIELD_GEO_POINT:
        {
            CmdContext<GeoPointContext>* pGeoPonitCmd = dynamic_cast<CmdContext<GeoPointContext>* >(m_oTypeCmdContMap[field_type]);
            if (NULL == pGeoPonitCmd){
                return false;
            }
            pGeoPonitCmd->oFieldBody.oFieldValue(oValue);
        }
        break;
    case FIELD_GEO_SHAPE:
        {
            CmdContext<GeoShapeContext>* pGeoShapeCmd = dynamic_cast<CmdContext<GeoShapeContext>* >(m_oTypeCmdContMap[field_type]);
            if (NULL == pGeoShapeCmd){
                return false;
            }
            pGeoShapeCmd->oFieldBody.oFieldValue(oValue);
        }
        break;
    default:
        break;
    }
    return true;
}

void TypeParserManager::RollBackIndex(const Json::Value& fieldid_formatkey 
                , const InsertParam& oInsertPara)
{
    // 删除各Parser，在当前请求中，各自的回滚函数
    std::map<int , TypeParserBase*>::iterator pTypeParserIter = m_oTypeParserMap.begin();
    for ( ; pTypeParserIter != m_oTypeParserMap.end(); ++pTypeParserIter)
    {
        pTypeParserIter->second->RollBackSelfIndex();
    }
    // 删除keyword_index
    if(fieldid_formatkey.isArray()){
        for(int i = 0;i < (int)fieldid_formatkey.size();i++){
            Json::Value info = fieldid_formatkey[i];
            if(info.isArray()){
                for(int j = 0;j < (int)info.size();j++){
                    if(info[j].isString()){
                        string key = info[j].asString();
                        g_IndexInstance.delete_index(key
                        , oInsertPara.doc_id
                        , oInsertPara.trans_version
                        , i);
                    }
                }
            }
        }
    }
    // 如果trans_version=1，删除快照，否则更新快照的trans_version=trans_version-1
    if(1 == oInsertPara.trans_version){
        g_IndexInstance.delete_snapshot_dtc(oInsertPara.doc_id, oInsertPara.appid);
    } else {
        g_IndexInstance.update_sanpshot_dtc(oInsertPara.appid
                            , oInsertPara.doc_id
                            , oInsertPara.trans_version);
    }
}

int TypeParserManager::UpdateTransVersion(const UserTableContent& oUserTableCont , int& iTransVersion)
{
    int iOldTransVersion = 0;
    int iRet = g_IndexInstance.get_snapshot_active_doc(oUserTableCont, iOldTransVersion);
    if (0 == iRet){
        iTransVersion = iOldTransVersion + 1;
    }else if (RT_NO_THIS_DOC == iRet){
        iTransVersion = 1;
    }else{
        log_error("get_snapshot_active_doc error.");
        return -iRet;
    }

    if (iTransVersion != 1){
        // 更新快照的TransVersion字段
        int iAffectedRows = 0;
        iRet = g_IndexInstance.update_snapshot_version(oUserTableCont
                    , iTransVersion , iAffectedRows);
        if (iRet != 0){
            log_error("doc_id[%s] update snapshot version error, continue.", oUserTableCont.doc_id.c_str());
            return -iRet;
        }else if (0 == iAffectedRows){
            log_info("doc_id[%s] update snapshot conflict, continue.", oUserTableCont.doc_id.c_str());
            return -RT_UPDATE_SNAPSHOT_CONFLICT;
        }
    }else{ // 1 == iTransVersion
        // 新Key，第一次插入表
        iRet = g_IndexInstance.insert_snapshot_version(oUserTableCont, iTransVersion);
        if (iRet != 0){
            log_error("doc_id[%s] insert error, continue.", oUserTableCont.doc_id.c_str());
            return -iRet;
        }
    }
    return iRet;
}

int TypeParserManager::DecodeFields(const Json::Value& table_content 
                , Json::Value& json_fields
                , UserTableContent& fields)
{
    std::string cmd;
    if(table_content.isMember("cmd") && table_content["cmd"].isString()){
        cmd = table_content["cmd"].asString();
        if(cmd == "add" || cmd == "update"){
            if(table_content.isMember("fields") && table_content["fields"].isObject()){
                json_fields = table_content["fields"];

                if(json_fields.isMember("id") && (json_fields["id"].isString() || json_fields["id"].isInt())){
                    fields.doc_id = json_fields["id"].asString();
                }else{
                    if(json_fields.isMember("doc_id") && json_fields["doc_id"].isString()){
                        fields.doc_id = json_fields["doc_id"].asString();
                    }else
                        return RT_NO_DOCID;
                }

                if(json_fields.isMember("weight") && json_fields["weight"].isInt()){
                    fields.weight = json_fields["weight"].asInt();
                }else{
                    fields.weight = 1;
                }
                return RT_CMD_ADD;
            }
            else{
                return RT_ERROR_FIELD;
            }
        }else if(cmd == "delete"){
            json_fields = table_content["fields"];
            if(json_fields.isMember("doc_id") && json_fields["doc_id"].isString()){
                fields.doc_id = json_fields["doc_id"].asString();
            }else if(json_fields.isMember("id") && (json_fields["id"].isString() || json_fields["id"].isInt())){
                fields.doc_id = json_fields["id"].asString();
            }else{
                return RT_NO_DOCID;
            }
            return RT_CMD_DELETE;
        }else{
            return RT_ERROR_FIELD_CMD;
        }
    }
    return 0;
}

void TypeParserManager::GetSnapshotCont(int iAppid , const Json::Value& oJsonFields 
                                        , Json::Value& outSnapShotCont)
{
    Json::Value::Members oMember = oJsonFields.getMemberNames();
    Json::Value::Members::iterator iter = oMember.begin();

    // 根据app_field_define表，判断Table字段是否需要存入快照
    for(; iter != oMember.end(); ++iter){
        std::string field_name = *iter;
        table_info* tbinfo = NULL;
        tbinfo = SplitManager::Instance()->get_table_info(iAppid, field_name);
        if(tbinfo == NULL){
            continue;
        }
        if(tbinfo->snapshot_tag == 1){ //snapshot
            if(tbinfo->field_type == 1 && oJsonFields[field_name].isInt()){
                outSnapShotCont[field_name] = oJsonFields[field_name].asInt();
            }else if(tbinfo->field_type > 1 && oJsonFields[field_name].isString()){
                outSnapShotCont[field_name] = oJsonFields[field_name].asString();
            }else if(tbinfo->field_type > 1 && oJsonFields[field_name].isDouble()){
                outSnapShotCont[field_name] = oJsonFields[field_name].asDouble();
            }else if(tbinfo->field_type > 1 && oJsonFields[field_name].isInt64()){
                outSnapShotCont[field_name] = oJsonFields[field_name].asInt64();
            }else if(tbinfo->field_type > 1 && oJsonFields[field_name].isArray()){
                outSnapShotCont[field_name] = oJsonFields[field_name];
            }else if (tbinfo->field_type > 1 && oJsonFields[field_name].isObject()){
                outSnapShotCont[field_name] = oJsonFields[field_name];
            }
        }
    }
}

void TypeParserManager::HandleUnifiedIndex(const InsertParam& oInsertPara , const Json::Value& oDocidIndexJson)
{
    std::vector<std::string> union_key_vec;
    SplitManager::Instance()->getUnionKeyField(oInsertPara.appid, union_key_vec);

    vector<string>::iterator union_key_iter = union_key_vec.begin();
    for(; union_key_iter != union_key_vec.end(); union_key_iter++){
        string union_key = *union_key_iter;
        vector<int> union_field_vec = CommonHelper::Instance()->SplitInt(union_key, ",");
        vector<int>::iterator union_field_iter = union_field_vec.begin();
        vector<vector<string> > keys_vvec;
        for(; union_field_iter != union_field_vec.end(); union_field_iter++){
            //联合索引单个字段 union_field_id
            int union_field_id = *union_field_iter;
            if(union_field_id >= (int)oDocidIndexJson.size()){
                log_error("appid[%d] field[%d] is invalid", oInsertPara.appid, *union_field_iter);
                break;
            }
            vector<string> key_vec;
            if(!oDocidIndexJson[union_field_id].isArray()){
                log_debug("doc_id[%s] union_field_id[%d] has no keys", oInsertPara.doc_id.c_str(), union_field_id);
                break;
            }
            for (int key_index = 0; key_index < (int)oDocidIndexJson[union_field_id].size(); key_index++){
                if(oDocidIndexJson[union_field_id][key_index].isString()){
                    std::string union_index_key = oDocidIndexJson[union_field_id][key_index].asString();
                    std::stringstream sAppid;
                    sAppid << oInsertPara.appid;
                    uint32_t iPos = sAppid.str().length() + 4; // 倒排key的格式为：应用ID#00#折扣，这里只取第二个#后面的内容
                    if (iPos < union_index_key.length()){
                        key_vec.push_back(union_index_key.substr(iPos));
                    }
                }
            }
            keys_vvec.push_back(key_vec);
        }
        if(keys_vvec.size() != union_field_vec.size()){
            log_debug("keys_vvec.size not equal union_field_vec.size");
            break;
        }
        vector<string> union_keys = CommonHelper::Instance()->Combination(keys_vvec);
        for(int m = 0 ; m < (int)union_keys.size(); ++m){
            int iRet = g_IndexInstance.insert_union_index_dtc(union_keys[m], oInsertPara.doc_id
                        , oInsertPara.appid, oInsertPara.doc_version);
            if(iRet != 0){
                log_error("insert union key[%s] error", union_keys[m].c_str());
            }
        }
    }
}

int TypeParserManager::UpdateDocVersion(const InsertParam& oInsertPara 
        , const Json::Value& oDocidIndexJson 
        , UserTableContent& oUserTableCont)
{
    oUserTableCont.content = oInsertPara.sExtend;
    Json::FastWriter doc_index_writer;
    std::string doc_index_map_string = doc_index_writer.write(oDocidIndexJson);
    if(oInsertPara.doc_version != 1){//need update
        map<uint32_t, vector<string> > index_res;
        g_IndexInstance.GetIndexData(CommonHelper::Instance()->GenerateDtcKey(oInsertPara.appid
                    , "20", oInsertPara.doc_id), oInsertPara.doc_version - 1, index_res);
        map<uint32_t, vector<string> >::iterator map_iter = index_res.begin();
        for(; map_iter != index_res.end(); map_iter++){
            uint32_t field = map_iter->first;
            vector<string> words = map_iter->second;
            for(int i = 0; i < (int)words.size(); i++){
                DeleteTask::GetInstance().RegisterInfo(words[i], oInsertPara.doc_id
                                    , oInsertPara.doc_version - 1, field);
            }
        }

        int affected_rows = 0;
        int iRet = g_IndexInstance.update_sanpshot_dtc(oUserTableCont, oInsertPara.trans_version, affected_rows);
        if(iRet != 0 || 0 == affected_rows){
            log_error("update_sanpshot_dtc error, roll back, ret: %d, affected_rows: %d.", iRet, affected_rows);
            return -RT_ERROR_UPDATE_SNAPSHOT;
        }

        g_IndexInstance.update_docid_index_dtc(doc_index_map_string, oInsertPara.doc_id
                    , oInsertPara.appid, oInsertPara.doc_version);
    }else{
        int affected_rows = 0;
        int iRet = g_IndexInstance.update_sanpshot_dtc(oUserTableCont, oInsertPara.trans_version, affected_rows);
        if(iRet != 0 || 0 == affected_rows){
            log_error("update_sanpshot_dtc error, roll back, ret: %d, affected_rows: %d.", iRet, affected_rows);
            return -RT_ERROR_UPDATE_SNAPSHOT;
        }
        g_IndexInstance.insert_docid_index_dtc(doc_index_map_string, oInsertPara.doc_id
                    , oInsertPara.appid, oInsertPara.doc_version);
    }
    return 0;
}

int TypeParserManager::DeleteDtcFields(const UserTableContent& oUserTableCont)
{
    // 从hanpin_index_data中删除
    std::vector<uint32_t> field_vec;
    SplitManager::Instance()->getHanpinField(oUserTableCont.appid, field_vec);
    vector<uint32_t>::iterator iter = field_vec.begin();
    for (; iter != field_vec.end(); iter++) {
        std::stringstream ss;
        ss << oUserTableCont.appid << "#" << *iter;
        int iRet = g_IndexInstance.delete_hanpin_index(ss.str(), oUserTableCont.doc_id);
        if (iRet != 0) {
            log_error("delete error! errcode %d", iRet);
            return iRet;
        }
    }

    // 从keyword_index_data中删除
    int old_version = 0;
    int iRet = g_IndexInstance.get_snapshot_active_doc(oUserTableCont, old_version);
    if(iRet != 0 && iRet != RT_NO_THIS_DOC){
        log_error("get_snapshot_active_doc error! errcode %d", iRet);
        return iRet;
    }
    std::map<uint32_t, std::vector<string> > index_res;
    g_IndexInstance.GetIndexData(CommonHelper::Instance()->GenerateDtcKey(oUserTableCont.appid, "20", oUserTableCont.doc_id)
                            , old_version, index_res);
    std::map<uint32_t, std::vector<string> >::iterator map_iter = index_res.begin();
    for(; map_iter != index_res.end(); map_iter++){
        uint32_t field = map_iter->first;
        vector<string> words = map_iter->second;
        for(int i = 0; i < (int)words.size(); i++){
            DeleteTask::GetInstance().RegisterInfo(words[i], oUserTableCont.doc_id, old_version, field);
        }
    }

    iRet = g_IndexInstance.delete_snapshot_dtc(oUserTableCont.doc_id, oUserTableCont.appid);//not use the doc_version curr
    g_IndexInstance.delete_docid_index_dtc(CommonHelper::Instance()->GenerateDtcKey(oUserTableCont.appid, "20", oUserTableCont.doc_id)
                , oUserTableCont.doc_id);
    return iRet;
}