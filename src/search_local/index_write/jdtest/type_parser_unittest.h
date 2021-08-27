// Copyright 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// A sample program demonstrating using Google C++ testing framework.

// This sample shows how to write a more complex unit test for a class
// that has multiple member functions.
//
// Usually, it's a good idea to have one test for each method in your
// class.  You don't have to do that exactly, but it helps to keep
// your tests organized.  You may also throw in additional tests as
// needed.
#ifndef TYPE_PARSER_UNITTEST_H_
#define TYPE_PARSER_UNITTEST_H_

#include "unitest_common_.h"
#include "../type_parser_base.h"

UNITEST_NAMESPACE_BEGIN

  template<class T>
  TypeParserBase* CreateTypeParser();

  template<>
  TypeParserBase* CreateTypeParser<IpParser>(){
      return new IpParser();
  }

  template<>
  TypeParserBase* CreateTypeParser<StrParser>(){
      return new StrParser();
  }

  template<>
  TypeParserBase* CreateTypeParser<GeoPointParser>(){
      return new GeoPointParser();
  } 

  template<>
  TypeParserBase* CreateTypeParser<GeoShapeParser>(){
      return new GeoShapeParser();
  }

  template<>
  TypeParserBase* CreateTypeParser<FigureParser<int> >(){
      return new FigureParser<int>();
  }

  template<>
  TypeParserBase* CreateTypeParser<FigureParser<int64_t> >(){
      return new FigureParser<int64_t>();
  }

  template<>
  TypeParserBase* CreateTypeParser<FigureParser<double> >(){
      return new FigureParser<double>();
  }

  template<class T>
  class TypeParserTest : public testing::Test {
  protected:
    TypeParserTest() : type_parser_(CreateTypeParser<T>()){};
    virtual~ TypeParserTest() {
      if(type_parser_ != NULL) {
        delete type_parser_;
      }
    };

    TypeParserBase* type_parser_;
  };

#if GTEST_HAS_TYPED_TEST_P
  using testing::Types;
  TYPED_TEST_SUITE_P(TypeParserTest);

  ////***********************************
  ////任意Typeparser都有属于自己的filedvalue
  ////要不然认为设计的parser不合理
  ////***********************************
  TYPED_TEST_P(TypeParserTest , ReturnFalseForSendCmd){
    CmdBase* pCmdBase =  new CmdBase();
    EXPECT_FALSE(this->type_parser_->SendCmd(pCmdBase));
    DELETE(pCmdBase);
  }

  REGISTER_TYPED_TEST_SUITE_P(
    TypeParserTest,  // The first argument is the test case name.
    // The rest of the arguments are the test names.
    ReturnFalseForSendCmd);
  
  typedef Types<IpParser, StrParser, GeoPointParser, GeoShapeParser, 
    FigureParser<int>, FigureParser<int64_t>, FigureParser<double> > 
  TypeParserImplementations;

  INSTANTIATE_TYPED_TEST_SUITE_P(TypeParserUniTest,           // Instance name
                               TypeParserTest,                // Test case name
                               TypeParserImplementations);    // Type list
#endif

UNITEST_NAMESPACE_END

#endif
