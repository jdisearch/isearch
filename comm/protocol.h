#ifndef __CH_PROTOCOL_H_
#define __CH_PROTOCOL_H_
#include <stdint.h>


#define MAXFIELDS_PER_TABLE	255
#define MAXPACKETSIZE	(64<<20)

struct CPacketHeader {
	uint16_t magic;//0xFDFC
	uint16_t cmd;
	uint32_t len;
	uint32_t seq_num;
};

struct CTimeInfo {
	uint64_t time;
};

#endif
