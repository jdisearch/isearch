#ifndef TYPE_PARSER_MANAGER_H_
#define TYPE_PARSER_MANAGER_H_

#include <map>
#include "singleton.h"
#include "noncopyable.h"
#include "cmdbase.h"

class CmdBase;
class TypeParserBase;
struct UserTableContent;

class TypeParserManager : private noncopyable
{
public:
    TypeParserManager();
    virtual ~TypeParserManager();

public:
    static TypeParserManager* Instance()
    {
        return CSingleton<TypeParserManager>::Instance();
    };

    static void Destroy()
    {
        CSingleton<TypeParserManager>::Destroy();
    };

public:
    bool BindNewCmd(int iType , CmdBase* pCmdBase){
        std::pair<std::map<int , CmdBase*>::iterator , bool> ret;
        ret = m_oTypeCmdContMap.insert(std::make_pair(iType , pCmdBase));
        return ret.second;
    };

    bool BindNewTypeParser(int iType , TypeParserBase* pTypeParser){
        std::pair<std::map<int , TypeParserBase*>::iterator , bool> ret;
        ret = m_oTypeParserMap.insert(std::make_pair(iType , pTypeParser));
        return ret.second;
    };

public:
    int HandleTableContent(int iAppid, Json::Value& oJsonTableContent);

private:
    void InitCmdContMap();
    void InitTypeParserMap();
    bool ConvertFieldValue2Cmd(int field_type, const Json::Value& oValue);
    void RollBackIndex(const Json::Value& fieldid_formatkey , const InsertParam& oInsertPara);

    int UpdateTransVersion(const UserTableContent& oUserTableCont, int& iTransVersion);
    int DecodeFields(const Json::Value& table_content,Json::Value& json_fields,UserTableContent& fields);
    void GetSnapshotCont(int iAppid , const Json::Value& oJsonFields , Json::Value& outSnapShotCont);
    void HandleUnifiedIndex(const InsertParam& oInsertPara , const Json::Value& oDocidIndexJson);
    int UpdateDocVersion(const InsertParam& oInsertPara , const Json::Value& oDocidIndexJson , UserTableContent& oUserTableCont);

    int DeleteDtcFields(const UserTableContent& oUserTableCont);

private:
    std::map<int , CmdBase*> m_oTypeCmdContMap;
    std::map<int , TypeParserBase*> m_oTypeParserMap;
};


#endif