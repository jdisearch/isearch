#ifndef CMDBASE_UNITEST_H_
#define CMDBASE_UNITEST_H_

#include "unitest_common_.h"
#include "../cmdbase.h"

UNITEST_NAMESPACE_BEGIN

class CmdBaseTest : public testing::Test {
    protected:
    
    virtual void SetUp() {
        string_type_cmd_.p_field_property = new table_info();
    }

    virtual void TearDown(){
        DELETE(string_type_cmd_.p_field_property);
    }

    Json::Reader reader_;
    CmdContext<std::string> string_type_cmd_;
    IpContext ip_context_;
    GeoPointContext geo_point_context_;
    GeoShapeContext geo_shape_context_;
};

TEST_F(CmdBaseTest , CmdContextTest){
    this->string_type_cmd_.p_field_property->segment_tag = SEGMENT_CHINESE;
    this->string_type_cmd_.oFieldBody.oFieldValue = "我爱中国";
    EXPECT_TRUE(this->string_type_cmd_.IsSegTagVaild());
    this->string_type_cmd_.oFieldBody.oFieldValue = "I love China";
    EXPECT_FALSE(this->string_type_cmd_.IsSegTagVaild());

    this->string_type_cmd_.p_field_property->segment_tag = SEGMENT_ENGLISH;
    this->string_type_cmd_.oFieldBody.oFieldValue = "I love China";
    EXPECT_TRUE(this->string_type_cmd_.IsSegTagVaild());
    this->string_type_cmd_.oFieldBody.oFieldValue = "我爱中国";
    EXPECT_FALSE(this->string_type_cmd_.IsSegTagVaild());

    this->string_type_cmd_.oInsertParam.appid = 10001;
    this->string_type_cmd_.Clear();
    EXPECT_EQ(0, this->string_type_cmd_.oInsertParam.appid);
    EXPECT_EQ(NULL , this->string_type_cmd_.p_field_property);
};

TEST_F(CmdBaseTest , IpContextTest){
    uint32_t ui_ip_binary;
    this->ip_context_.sIpContext = "127.0.0.1";
    EXPECT_TRUE(this->ip_context_.IsIpFormat(ui_ip_binary));

    this->ip_context_.sIpContext = "12345";
    EXPECT_FALSE(this->ip_context_.IsIpFormat(ui_ip_binary));
};

TEST_F(CmdBaseTest , GeoPointContextTest){
    #define D_LATITUDE  -73.983
    #define D_LONGITUDE 40.719
    std::string s_test = "{\"stringfield\":\"-73.983, 40.719\",\"objectfield\":{\"latitude\":\"-73.983\",\"longitude\":\"40.719\"},\"arrayfield\":[\"-73.983\",\"40.719\"]}";
    Json::Value geo_parse_json_value;
    bool b_ret = reader_.parse(s_test , geo_parse_json_value);
    ASSERT_EQ(true , b_ret);

    this->geo_point_context_(geo_parse_json_value["stringfield"]);
    EXPECT_EQ(D_LATITUDE , atof(this->geo_point_context_.sLatitude.c_str()));
    EXPECT_EQ(D_LONGITUDE , atof(this->geo_point_context_.sLongtitude.c_str()));
    EXPECT_TRUE(this->geo_point_context_.IsGeoPointFormat());

    this->geo_point_context_(geo_parse_json_value["objectfield"]);
    EXPECT_EQ(D_LATITUDE , atof(this->geo_point_context_.sLatitude.c_str()));
    EXPECT_EQ(D_LONGITUDE , atof(this->geo_point_context_.sLongtitude.c_str()));

    this->geo_point_context_(geo_parse_json_value["arrayfield"]);
    EXPECT_EQ(D_LATITUDE , atof(this->geo_point_context_.sLatitude.c_str()));
    EXPECT_EQ(D_LONGITUDE , atof(this->geo_point_context_.sLongtitude.c_str()));

    this->geo_point_context_.Clear();
    EXPECT_FALSE(this->geo_point_context_.IsGeoPointFormat());
};

TEST_F(CmdBaseTest , GeoShapeContextTest){
    std::string s_test = "{\"geoshape\":\"POLYGON((121.437271 31.339747, 121.438022 31.337291, 121.435297 31.336814, 121.434524 31.339252, 121.437271 31.339747))\"}";
    Json::Value geo_shape_parse_json;
    bool b_ret = reader_.parse(s_test , geo_shape_parse_json);
    ASSERT_EQ(true , b_ret);

    this->geo_shape_context_(geo_shape_parse_json["geoshape"]);
    EXPECT_FALSE(this->geo_shape_context_.oGeoShapeVet.empty());

    EnclosingRectangle oEncloseRect = this->geo_shape_context_.GetMinEnclosRect();
    EXPECT_TRUE(oEncloseRect.IsVaild());
    EXPECT_TRUE(this->geo_shape_context_.IsGeoShapeFormat());

    this->geo_shape_context_.Clear();
    EXPECT_FALSE(this->geo_shape_context_.IsGeoShapeFormat());
};

UNITEST_NAMESPACE_END
#endif