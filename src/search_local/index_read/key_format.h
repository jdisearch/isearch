#ifndef __KEY_FORMAT_H__
#define __KEY_FORMAT_H__

#include <string>
#include <sstream>
#include <stdint.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <string.h>
#include <map>
#include <vector>

class KeyFormat {
public:
    typedef std::vector<std::pair<int,std::string> >  UnionKey;

public:
    static std::string Encode(const UnionKey& oUnionKey);
    static bool Decode(const std::string& sKey, UnionKey& oUnionKey);

	static std::string EncodeBytes(const std::string& src);
    static std::string EncodeBytes(int src);
	static std::string EncodeBytes(int64_t src);
	static std::string EncodeBytes(uint64_t src);
	static std::string EncodeBytes(double src);
	static void DecodeBytes(const std::string& src, int64_t& dst);
	static void DecodeBytes(const std::string& src, std::string&  dst);
	static void DecodeBytes(const std::string& src, uint64_t& dst);
	static void DecodeBytes(const std::string& src, double& dst);
};


#endif
