#ifndef TYPE_PARSER_MOCK_TEST_H_
#define TYPE_PARSER_MOCK_TEST_H_

#include "unitest_common_.h"
#include "../type_parser_base.h"

UNITEST_NAMESPACE_BEGIN
class TypeParserMock : public TypeParserBase
{
public:
    TypeParserMock() : TypeParserBase() {};
    MOCK_METHOD(bool ,SendCmd , (CmdBase* pCmdContext));
    MOCK_METHOD(void ,ClearOneField , ());

    MOCK_METHOD(bool ,IsReady , ());
    MOCK_METHOD(bool ,GenerateKeyWords , ());
    MOCK_METHOD(bool ,InsertDtcTable , (Json::Value& oResult));
};

class TypeParserMockTest : public testing::Test {
protected:
    TypeParserMockTest():type_parser_(&type_parser_mock_){};

    TypeParserBase* type_parser_;
    TypeParserMock type_parser_mock_;
};

TEST_F(TypeParserMockTest , StartParserLogicTest){
    EXPECT_CALL(type_parser_mock_ , IsReady()).Times(AnyNumber())
        .WillOnce(Return(false)).WillOnce(Return(true))
        .WillRepeatedly(Return(true));
    
    EXPECT_CALL(type_parser_mock_ , ClearOneField()).Times(AnyNumber())
        .WillRepeatedly(Return());

    EXPECT_CALL(type_parser_mock_ , GenerateKeyWords()).Times(AnyNumber())
        .WillOnce(Return(true)).WillOnce(Return(false))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(type_parser_mock_ , InsertDtcTable(testing::_)).Times(AnyNumber())
        .WillOnce(Return(true)).WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    
    Json::Value fieldid_formatkey;
    EXPECT_NE(0 ,type_parser_->StartParser(fieldid_formatkey));

    EXPECT_EQ(0 ,type_parser_->StartParser(fieldid_formatkey));

    type_parser_->SetErrorCode(-1);
    EXPECT_NE(0 ,type_parser_->StartParser(fieldid_formatkey));
    EXPECT_NE(0 ,type_parser_->StartParser(fieldid_formatkey));

    EXPECT_EQ(0 ,type_parser_->StartParser(fieldid_formatkey));

}

UNITEST_NAMESPACE_END

#endif