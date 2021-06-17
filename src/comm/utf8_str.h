/*
 * =====================================================================================
 *
 *       Filename:  utf8_str.h
 *
 *    Description:  utf8_str class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __UTF8_STR_H__
#define __UTF8_STR_H__

#include <string>
#include <vector>
using namespace std;

class iutf8string
{
public:
	iutf8string(const std::string& str);
	iutf8string(const char* str);
	~iutf8string();
	iutf8string(const iutf8string& str);
	iutf8string& operator=(const iutf8string& str);

public:
	int length();
	std::string get(int index);
	iutf8string operator + (iutf8string& str);
	std::string operator [](int index);
	std::string stlstring();
	std::string stlstring() const;
	const char* c_str();
	iutf8string utf8substr(int u8start_index, int u8_length);
	std::string substr(int u8start_index, int u8_length);

private:
	std::string data;
	int* offerset;
	int _length;
	void refresh();
};

vector<string> splitEx(const string& src, string separate_character);

#endif