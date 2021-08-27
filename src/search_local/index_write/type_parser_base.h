#ifndef TYPE_PARSER_BASE_H_
#define TYPE_PARSER_BASE_H_

#include <sstream>
#include <type_traits>
#include <vector>
#include "cmdbase.h"
#include "key_format.h"
#include "json/json.h"
#include "log.h"
#include "index_tbl_op.h"

class TypeParserBase
{
public:
    TypeParserBase();
    virtual ~TypeParserBase();

public:
    virtual bool SendCmd(CmdBase* pCmdContext) = 0;
    virtual void ClearOneField() = 0;
    virtual void CleareOneReq() { return; };
    virtual void RollBackSelfIndex() { return; };

    void Clear() {
        word_property_vet_.clear();
        ClearOneField();
    }

    int StartParser(Json::Value& fieldid_formatkey);
    void SetErrorCode(int i_error_id) { m_iErrorCode = i_error_id;};
    int GetErrorCode() { return m_iErrorCode;};

protected:
    virtual bool IsReady() { return true; };
    virtual bool GenerateKeyWords() = 0;
    virtual bool InsertDtcTable(Json::Value& oResult) = 0;

protected:
    void SetErrorContext(int iLine ,int iErId , const std::string& sError){
        log_error("[TypeParser]:lineNum:%d , errorcode:%d , errorInfo:%s" 
            , iLine , iErId , sError.c_str());
        m_iErrorCode = iErId;
    };

    bool InsertIndexDtc(const InsertParam& insert_param , Json::Value& oResult);

protected:
    std::vector<WordProperty> word_property_vet_;

private:
    int m_iErrorCode;
};

/////////////////////////////////////////////
//// figureType:int double and long
//// Now they have same functions ,so use T 
/////////////////////////////////////////////
template<class T>
class FigureParser : public TypeParserBase
{
public:
    FigureParser()
    : TypeParserBase()
    , m_pFigureCmdCont(NULL)
    { };
    virtual ~FigureParser() {};

public:
    virtual bool SendCmd(CmdBase* pCmdContext){
        m_pFigureCmdCont = dynamic_cast<CmdContext<T>* >(pCmdContext);
        if (NULL == m_pFigureCmdCont){
            SetErrorContext(__LINE__ , -RT_ERROR_CMD_TRANSFORM , "T type is incorrect, please check mapList");
            return false;
        }
        return true;
    }

    virtual void ClearOneField(){
        m_pFigureCmdCont->Clear();
    }

private:
    virtual bool GenerateKeyWords(){
        std::stringstream stream_key;
	    stream_key << m_pFigureCmdCont->oFieldBody.oFieldValue;

        KeyFormat::UnionKey o_keyinfo_vet;
        o_keyinfo_vet.push_back(std::make_pair(m_pFigureCmdCont->p_field_property->field_type ,
             stream_key.str()));
	    std::string sValue = KeyFormat::Encode(o_keyinfo_vet);

        log_debug("figureParser fieldId:%d , keyFormatValue:%s" 
            , m_pFigureCmdCont->p_field_property->field_id , sValue.c_str());
        std::string s_format_key = CommonHelper::Instance()->GenerateDtcKey(m_pFigureCmdCont->oInsertParam.appid, "00" , sValue);
        word_property_vet_.push_back(WordProperty(s_format_key , 0));
        return true;
    }

    virtual bool InsertDtcTable(Json::Value& oResult){
        return InsertIndexDtc(m_pFigureCmdCont->oInsertParam , oResult);
    }

private:
    CmdContext<T>* m_pFigureCmdCont;
};

// 减少冗余代码编写
#define TYPE_PARSER_NAME(parserName)  parserName##Parser

#define TypeParser(parserName)                                          \
class TYPE_PARSER_NAME(parserName) : public TypeParserBase              \
{                                                                       \
public:                                                                 \
    TYPE_PARSER_NAME(parserName)();                                     \
    virtual ~TYPE_PARSER_NAME(parserName)();                            \
                                                                        \
public:                                                                 \
    virtual bool SendCmd(CmdBase* pCmdContext);                         \
    virtual void ClearOneField();                                       \
private:                                                                \
    virtual bool GenerateKeyWords();                                    \
    virtual bool InsertDtcTable(Json::Value& oResult);

#define ENDFLAG };

#define IsReadyDeclare()                                                \
private:                                                                \
    virtual bool IsReady();

#define CleareOneReqDeclare()                                           \
public:                                                                 \
    virtual void CleareOneReq();

#define RollBackSelfIndexDeclare()                                      \
public:                                                                 \
    virtual void RollBackSelfIndex();

/////////////////////////////////////////////
//// Ip is a single type, evenif now Ip handle 
//// functions are same with figureParser
/////////////////////////////////////////////
TypeParser(Ip)
IsReadyDeclare()
private:
    uint32_t m_uiIpBinary;
    CmdContext<IpContext>* m_pIpCmdCont;
ENDFLAG

/////////////////////////////////////////////
//// string type: short_text and text
/////////////////////////////////////////////
TypeParser(Str)
IsReadyDeclare()
CleareOneReqDeclare()
RollBackSelfIndexDeclare()
private:
    void StatWordFreq(uint32_t iAppid , std::vector<std::vector<std::string> >& strss);

private:
    std::map<std::string, WordProperty> word_property_map_;
    std::vector<IntelligentInfo> m_oIntelligentInfo;
    std::vector<std::string> m_oIntelligentkeysVet;
    CmdContext<std::string>* m_pStrCmdCont;
ENDFLAG

/////////////////////////////////////////////
//// GeoPoint type: Latitude and longtitude
//// to present a location on earch
/////////////////////////////////////////////
TypeParser(GeoPoint)
IsReadyDeclare()
private:
    CmdContext<GeoPointContext>* m_pGeoPointCmdCont;
ENDFLAG

/////////////////////////////////////////////
//// GeoShape type: muti Latitude and longtitude combinations
//// a scope location on earch
/////////////////////////////////////////////
TypeParser(GeoShape)
IsReadyDeclare()
private:
    CmdContext<GeoShapeContext>* m_pGeoShapeCmdCont;
ENDFLAG

#endif