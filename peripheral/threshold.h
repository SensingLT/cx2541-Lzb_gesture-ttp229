#ifndef __THRESHOLD_H__
#define __THRESHOLD_H__

#include <stdint.h>
#include <stdbool.h>
#include <PT32Y003x.h>

// 1tick = 5ms
#define MULTI_PRESS_TIMEOUT_TICK   130  // 连拍超时时间（650毫秒）
#define MULTI_PRESS_INTERVAL_TICK  10  // 连拍最小间隔时间（50毫秒）

typedef struct {
    uint16_t min_interval_time;
    uint16_t max_timeout;
    uint16_t crc;
} clap_param_t;

void ClapParam_Init();
bool ClapParam_Write(clap_param_t *clap_param);
const clap_param_t* ClapParam_Read();

#endif //__THRESHOLD_H__
