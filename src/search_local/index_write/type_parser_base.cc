#include "type_parser_base.h"
#include <set>
#include "split_manager.h"
#include "geohash.h"

TypeParserBase::TypeParserBase()
    : word_property_vet_()
    , m_iErrorCode(-1)
{ }

TypeParserBase::~TypeParserBase()
{ }

int TypeParserBase::StartParser(Json::Value& fieldid_formatkey)
{
    if (IsReady()){
        if (!GenerateKeyWords()){
            Clear();
            return m_iErrorCode;
        }
        bool bRet = InsertDtcTable(fieldid_formatkey);
        Clear();
        return (bRet ? 0 : m_iErrorCode);
    }else{
        Clear();
        return -RT_ERROR_ILLEGAL_PARSE_CMD;
    }
}

bool TypeParserBase::InsertIndexDtc(const InsertParam& insert_param , Json::Value& oResult){
    std::vector<WordProperty>::iterator iter = word_property_vet_.begin();
    for ( ; iter != word_property_vet_.end(); ++iter){
        int iRet = g_IndexInstance.insert_index_dtc(*iter , insert_param , oResult);
        if (iRet != 0){
            SetErrorContext(__LINE__ , -RT_ERROR_INSERT_INDEX_DTC 
                        , "TypeParserBase# insert keywords table failed");
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////
//// Ip is a single type, evenif now Ip handle 
//// functions are same with figureParser
/////////////////////////////////////////////
IpParser::IpParser()
    : TypeParserBase()
    , m_uiIpBinary(0)
    , m_pIpCmdCont(NULL)
{ }

IpParser::~IpParser()
{ }

bool IpParser::SendCmd(CmdBase* pCmdContext)
{
    m_pIpCmdCont = dynamic_cast<CmdContext<IpContext>* >(pCmdContext);
    if (NULL == m_pIpCmdCont){
        SetErrorContext(__LINE__ , -RT_ERROR_CMD_TRANSFORM , "IpCmd is incorrect, please check cmdmap");
        return false;
    }
    log_debug("IP value:%s" , m_pIpCmdCont->oFieldBody.oFieldValue.sIpContext.c_str());
    return true;
}

void IpParser::ClearOneField()
{
    m_pIpCmdCont->oFieldBody.oFieldValue.Clear();
    m_pIpCmdCont->Clear();
}

bool IpParser::IsReady()
{
    return m_pIpCmdCont->oFieldBody.oFieldValue.IsIpFormat(m_uiIpBinary);
}

bool IpParser::GenerateKeyWords()
{
    std::stringstream stream_ip;
    stream_ip << ntohl(m_uiIpBinary);
    KeyFormat::UnionKey o_keyinfo_vet;
    o_keyinfo_vet.push_back(std::make_pair(FIELD_IP , stream_ip.str()));
    std::string sValue = KeyFormat::Encode(o_keyinfo_vet);
    
    std::string s_format_key = CommonHelper::Instance()->GenerateDtcKey(m_pIpCmdCont->oInsertParam.appid, "00" , sValue);
    word_property_vet_.push_back(WordProperty(s_format_key , 0));
    return true;
}

bool IpParser::InsertDtcTable(Json::Value& oResult)
{ 
    return InsertIndexDtc(m_pIpCmdCont->oInsertParam , oResult);
}

/////////////////////////////////////////////
//// string type: short_text and text
/////////////////////////////////////////////
StrParser::StrParser()
    : TypeParserBase()
    , word_property_map_()
    , m_oIntelligentInfo()
    , m_oIntelligentkeysVet()
    , m_pStrCmdCont(NULL)
{ }

StrParser::~StrParser()
{ }

bool StrParser::SendCmd(CmdBase* pCmdContext)
{
    m_pStrCmdCont = dynamic_cast<CmdContext<std::string>* >(pCmdContext);
    if (NULL == m_pStrCmdCont){
        SetErrorContext(__LINE__, -RT_ERROR_CMD_TRANSFORM ,"str cmd is incorrect, please check cmdmap");
        return false;
    }
    log_debug("String value:%s" , m_pStrCmdCont->oFieldBody.oFieldValue.c_str());
    return true;
}

void StrParser::ClearOneField()
{
    word_property_map_.clear();
    m_oIntelligentInfo.clear();
    m_pStrCmdCont->oFieldBody.oFieldValue.clear();
    m_pStrCmdCont->Clear();
}

void StrParser::CleareOneReq()
{
    m_oIntelligentkeysVet.clear();
}

bool StrParser::IsReady()
{
    return !m_pStrCmdCont->oFieldBody.oFieldValue.empty();
}

bool StrParser::GenerateKeyWords()
{
    switch (m_pStrCmdCont->p_field_property->segment_tag)
    {
    case SEGMENT_CHINESE:
    case SEGMENT_ENGLISH:
        {
            if (!m_pStrCmdCont->IsSegTagVaild()){
                SetErrorContext(__LINE__ , -RT_ERROR_FIELD_FORMAT 
                    , "StrParser#segmentTag: 3, value must be chinese; segmentTag: 4, value must be no chinese");
                return false;
            }
            word_property_map_.insert(std::make_pair(m_pStrCmdCont->oFieldBody.oFieldValue, WordProperty("" , 1)));

            bool bFlag = false;
            CommonHelper::Instance()->GetIntelligent(m_pStrCmdCont->oFieldBody.oFieldValue, m_oIntelligentInfo, bFlag);
            if (!bFlag){
                SetErrorContext(__LINE__ , -RT_ERROR_WORD_SEGMENTATION 
                    , "StrParser#get intelligent words have some errors");
                return false; 
            }
        }
        break;
    case SEGMENT_DEFAULT:
        {
            std::vector<std::vector<std::string> > oSplitContent = SplitManager::Instance()->split(
                m_pStrCmdCont->oFieldBody.oFieldValue , m_pStrCmdCont->oInsertParam.appid);

            StatWordFreq(m_pStrCmdCont->oInsertParam.appid, oSplitContent);
        }
        break;
    default:
        SetErrorContext(__LINE__ , -RT_ERROR_FIELD_FORMAT 
            , "StrParser# illegal segmentTag define , please check request");
        return false;
    }
    return true;
}

bool StrParser::InsertDtcTable(Json::Value& oResult)
{
    if (!m_oIntelligentInfo.empty()){
        log_debug("StrParser# here segmentTag must 3 or 4 , now is:%d" , m_pStrCmdCont->p_field_property->segment_tag);
        std::stringstream streamKey;
        streamKey << m_pStrCmdCont->oInsertParam.appid << "#" << m_pStrCmdCont->p_field_property->field_id;
        int iRet = g_hanpinIndexInstance.do_insert_intelligent(streamKey.str()
                , m_pStrCmdCont->oInsertParam.doc_id
                , m_pStrCmdCont->oFieldBody.oFieldValue
                , m_oIntelligentInfo
                , m_pStrCmdCont->oInsertParam.doc_version);

        if(0 != iRet){
            SetErrorContext(__LINE__ , -RT_ERROR_INSERT_INDEX_DTC 
                , "StrParser# insert intelligent table failed");
            return false;
        }
        m_oIntelligentkeysVet.push_back(streamKey.str());
    }

    int iRet = g_IndexInstance.do_insert_index(word_property_map_
        , m_pStrCmdCont->oInsertParam , oResult);
    if (iRet != 0){
        SetErrorContext(__LINE__ , -RT_ERROR_INSERT_INDEX_DTC 
                , "StrParser# insert keywords table failed");
    }

    return (0 == iRet);
}

void StrParser::StatWordFreq(uint32_t iAppid , std::vector<std::vector<std::string> >& strss)
{
    string word;
    uint32_t id = 0;
    ostringstream oss;
    vector<vector<string> >::iterator iters = strss.begin();
    uint32_t index = 0;

    for(;iters != strss.end(); iters++){
        index++;
        vector<string>::iterator iter = iters->begin();

        log_debug("start StatWordFreq, appid = %u\n",iAppid);
        for (; iter != iters->end(); iter++) {

            word = *iter;
            if (!SplitManager::Instance()->wordValid(word, iAppid, id)){
                continue;
            }
            if (word_property_map_.find(word) == word_property_map_.end()) {
                WordProperty word_property;
                word_property.ui_word_freq = 1;
                word_property.word_indexs_vet.push_back(index);
                word_property_map_.insert(make_pair(word, word_property));
            }
            else {
                word_property_map_[word].ui_word_freq++;
                word_property_map_[word].word_indexs_vet.push_back(index);
            }

            oss << (*iter) << "|";
        }
    }
    log_debug("split: %s",oss.str().c_str());
}

void StrParser::RollBackSelfIndex()
{
    // 删除hanpin_index
    for(int i = 0; i < (int)m_oIntelligentkeysVet.size(); i++){
        g_hanpinIndexInstance.delete_intelligent(m_oIntelligentkeysVet[i]
                                                , m_pStrCmdCont->oInsertParam.doc_id
                                                , m_pStrCmdCont->oInsertParam.trans_version);
    }
}

/////////////////////////////////////////////
//// GeoPoint type: Latitude and longtitude
//// to present a location on earch
/////////////////////////////////////////////
GeoPointParser::GeoPointParser()
    : TypeParserBase()
    , m_pGeoPointCmdCont(NULL)
{ }

GeoPointParser::~GeoPointParser()
{ }

bool GeoPointParser::SendCmd(CmdBase* pCmdContext)
{
    m_pGeoPointCmdCont = dynamic_cast<CmdContext<GeoPointContext>* >(pCmdContext);
    if (NULL == m_pGeoPointCmdCont){
        SetErrorContext(__LINE__, -RT_ERROR_CMD_TRANSFORM ,"GeoPoint cmd is incorrect, please check cmdmap");
        return false;
    }
    log_debug("GeoPoint Latitude:%s , Longtitude:%s" 
        , m_pGeoPointCmdCont->oFieldBody.oFieldValue.sLatitude.c_str()
        , m_pGeoPointCmdCont->oFieldBody.oFieldValue.sLongtitude.c_str());
    return true;
}

void GeoPointParser::ClearOneField()
{
    m_pGeoPointCmdCont->oFieldBody.oFieldValue.Clear();
    m_pGeoPointCmdCont->Clear();
}

bool GeoPointParser::IsReady()
{
    return m_pGeoPointCmdCont->oFieldBody.oFieldValue.IsGeoPointFormat();
}

bool GeoPointParser::GenerateKeyWords()
{
    std::string sGisId = encode(atof(m_pGeoPointCmdCont->oFieldBody.oFieldValue.sLatitude.c_str())
                    , atof(m_pGeoPointCmdCont->oFieldBody.oFieldValue.sLongtitude.c_str()), 6);
    std::string sFormatGisid = KeyFormat::EncodeBytes(sGisId);
    log_debug("gis code = %s , formatGisId:%s",sGisId.c_str() , sFormatGisid.c_str());
    std::string s_format_key = CommonHelper::Instance()->GenerateDtcKey(m_pGeoPointCmdCont->oInsertParam.appid, "00", sFormatGisid);
    word_property_vet_.push_back(WordProperty(s_format_key , 0));
    return true;
}

bool GeoPointParser::InsertDtcTable(Json::Value& oResult)
{
    return InsertIndexDtc(m_pGeoPointCmdCont->oInsertParam , oResult);
}

/////////////////////////////////////////////
//// GeoShape type: muti Latitude and longtitude combinations
//// a scope location on earch
/////////////////////////////////////////////
GeoShapeParser::GeoShapeParser()
    : TypeParserBase()
    , m_pGeoShapeCmdCont(NULL)
{ }

GeoShapeParser::~GeoShapeParser()
{ }

bool GeoShapeParser::SendCmd(CmdBase* pCmdContext)
{
    m_pGeoShapeCmdCont = dynamic_cast<CmdContext<GeoShapeContext>* >(pCmdContext);
    if (NULL == m_pGeoShapeCmdCont){
        SetErrorContext(__LINE__, -RT_ERROR_CMD_TRANSFORM ,"GeoShape cmd is incorrect, please check cmdmap");
        return false;
    }
    std::vector<GeoPointContext>* pGeoShapeVet = &m_pGeoShapeCmdCont->oFieldBody.oFieldValue.oGeoShapeVet;
    std::vector<GeoPointContext>::iterator iter = pGeoShapeVet->begin();
    for (; iter != pGeoShapeVet->end(); ++iter)
    {
        log_debug("GeoShape Latitude:%s , Longtitude:%s" 
            , iter->sLatitude.c_str()
            , iter->sLongtitude.c_str());
    }

    return true;
}

void GeoShapeParser::ClearOneField()
{
    m_pGeoShapeCmdCont->oFieldBody.oFieldValue.Clear();
    m_pGeoShapeCmdCont->Clear();
}

bool GeoShapeParser::IsReady()
{
    return m_pGeoShapeCmdCont->oFieldBody.oFieldValue.IsGeoShapeFormat();
}

bool GeoShapeParser::GenerateKeyWords()
{
    EnclosingRectangle oEncloseRect = m_pGeoShapeCmdCont->oFieldBody.oFieldValue.GetMinEnclosRect();
    if (!oEncloseRect.IsVaild()){
        SetErrorContext(__LINE__, -RT_ERROR_FIELD_FORMAT ,"GeoShape enclosing rectangle is unreasonable");
        return false;
    }
    
    std::vector<std::string> oGisIdVet = GetArroundGeoHash(oEncloseRect , 6);
    for (size_t i = 0; i < oGisIdVet.size(); ++i){
        std::string sFormatGisid = KeyFormat::EncodeBytes(oGisIdVet[i]);
        log_debug("gis code = %s , formatGisId:%s",oGisIdVet[i].c_str() , sFormatGisid.c_str());
        std::string s_format_key = CommonHelper::Instance()->GenerateDtcKey(m_pGeoShapeCmdCont->oInsertParam.appid, "00", sFormatGisid);
        word_property_vet_.push_back(WordProperty(s_format_key , 0));
    }
    return true;
}

bool GeoShapeParser::InsertDtcTable(Json::Value& oResult)
{
    return InsertIndexDtc(m_pGeoShapeCmdCont->oInsertParam , oResult);
}