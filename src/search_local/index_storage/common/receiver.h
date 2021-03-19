/*
 * =====================================================================================
 *
 *       Filename:  receiver.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __RECVEIVER_H__
#define __RECVEIVER_H__

#include <unistd.h>
#include "protocol.h"

class SimpleReceiver
{
private:
	struct PacketHeader _header;
	char *_packet;
	int _netfd;
	int _count;
	int _offset;

public:
	SimpleReceiver(int fd = -1) : _packet(NULL), _netfd(fd), _count(0), _offset(0){};
	~SimpleReceiver(void){};

	void attach(int fd) { _netfd = fd; }
	void erase(void)
	{
		_packet = NULL;
		_count = 0;
		_offset = 0;
	}
	int remain(void) const { return _count - _offset; }
	int discard(void)
	{
		char buf[1024];
		int len;
		const int bsz = sizeof(buf);
		while ((len = remain()) > 0)
		{
			int rv = read(_netfd, buf, (len > bsz ? bsz : len));
			if (rv <= 0)
				return rv;
			_offset += rv;
		}
		return 1;
	}
	int fill(void)
	{
		int rv = read(_netfd, _packet + _offset, remain());
		if (rv > 0)
			_offset += rv;
		return rv;
	}
	void init(void)
	{
		_packet = (char *)&_header;
		_count = sizeof(_header);
		_offset = 0;
	}
	void set(char *p, int sz)
	{
		_packet = p;
		_count = sz;
		_offset = 0;
	}
	char *c_str(void) { return _packet; }
	const char *c_str(void) const { return _packet; }
	PacketHeader &header(void) { return _header; }
	const PacketHeader &header(void) const { return _header; }
};

#endif
