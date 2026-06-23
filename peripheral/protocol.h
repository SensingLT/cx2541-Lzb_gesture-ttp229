#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include "clap.h"
#include "slide.h"

#define CLAP_MSG_MAGIC 0xAEAE
#define SLIDE_MSG_MAGIC 0x55AA
#define CLAP_RESP_MAGIC 0x5A5A

typedef struct {
	uint16_t	magic; //固定为MSG_MAGIC
	uint16_t	crc; //从size字段开始到最后一个字节
	uint16_t	size; //whole msg size, even including tail
} msg_header_t;

typedef struct {
	msg_header_t header;
	clap_count_e clap_counts;
}msg_clap_t;

typedef struct {
	msg_header_t header;
	slide_direction_e slide_direction;
}msg_slide_t;

typedef struct {
	msg_header_t header;
	uint16_t  min_interval_time;//连拍最小间隔时间
	uint16_t  max_timeout;//连拍超时时间
}msg_clap_time_param_t;

#define FILL_CLAP_MSG(msg) \
	msg.header.magic = CLAP_MSG_MAGIC, \
	msg.header.size = sizeof(msg), \
	msg.header.crc = crc16((const uint8_t*)&msg.header.size, sizeof(msg) - 4)
		
#define FILL_SLIDE_MSG(msg) \
	msg.header.magic = SLIDE_MSG_MAGIC, \
	msg.header.size = sizeof(msg), \
	msg.header.crc = crc16((const uint8_t*)&msg.header.size, sizeof(msg) - 4)
		
#define FILL_RESP_MSG(msg) \
	msg.header.magic = CLAP_RESP_MAGIC, \
	msg.header.size = sizeof(msg), \
	msg.header.crc = crc16((const uint8_t*)&msg.header.size, sizeof(msg) - 4)
		
	
#define CHECK_CRC(pHeader) (pHeader->crc == crc16((const uint8_t*)&pHeader->size, pHeader->size - 4))

bool Protocol_HandleMsg(const uint8_t* pMsg, uint16_t length);
void reportClapStatus(clap_count_e pClapCounts);
void reportSlideStatus(slide_direction_e pSlideDirection);

#endif //__PROTOCOL_H__
