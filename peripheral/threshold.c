#include <string.h>
#include "threshold.h"
#include "crc.h"
#include "uart.h"

static clap_param_t gclapParam;

#define FLASH_START_ADDR 0x00004000


static bool readFromFlash(clap_param_t* pOutput) {    
	clap_param_t ClapParam;
	uint8_t* pBuf = (uint8_t*)&ClapParam;
	uint32_t addr = FLASH_START_ADDR;
	for (int i = 0; i < sizeof(ClapParam); i++) {
		pBuf[i] = IMMC_ReadByte(addr);
		addr += 4;
	}

	// CRC校验
	uint16_t calculatedCrc = crc16((uint8_t*)&ClapParam, sizeof(ClapParam.min_interval_time) + sizeof(ClapParam.max_timeout));
	if (ClapParam.crc != calculatedCrc) {
		DBG_LN("CRC mismatch! EEPROM=0x%08X, Calculated=0x%08X", ClapParam.crc, calculatedCrc);
		return false;
	}
	// 打印调试信息
	DBG_LN("Read MIN: 0x%04X, MAX: 0x%04X", ClapParam.min_interval_time, ClapParam.max_timeout);

	*pOutput = ClapParam;
	return true;
}

const clap_param_t* ClapParam_Read() {
	return &gclapParam;
}

//会重新计算并覆盖crc
bool ClapParam_Write(clap_param_t *ClapParam) {
	// 参数校验保持不变
	if(ClapParam->min_interval_time > ClapParam->max_timeout){
		DBG_LN("ClapParam Error: MIN > MAX");
		return false;
	}
	if(ClapParam->min_interval_time == 0 || ClapParam->max_timeout == 0){
		DBG_LN("ClapParam Error: Zero value");
		return false;
	}
	//计算CRC并存储
	ClapParam->crc = crc16((uint8_t*)ClapParam, sizeof(gclapParam.min_interval_time) + sizeof(gclapParam.max_timeout));
	uint8_t* pBuf = (uint8_t*)ClapParam;
	uint32_t addr = FLASH_START_ADDR;
	for (int i = 0; i < sizeof(*ClapParam); i++) {
		if (SET != IMMC_WriteWord(addr, pBuf[i])) {
			DBG_LN("EEPROM write failed!");
			return false;
		}
		addr += 4;
	}

	//read的时候会验证正确与否，成功时才会覆盖gclapParam
	if (!readFromFlash(&gclapParam)) {
		return false;
	}
	return gclapParam.min_interval_time == ClapParam->min_interval_time && gclapParam.max_timeout == ClapParam->max_timeout;
}

void ClapParam_Init() {
	if (!readFromFlash(&gclapParam)) {
        gclapParam.min_interval_time = MULTI_PRESS_INTERVAL_TICK;
        gclapParam.max_timeout = MULTI_PRESS_TIMEOUT_TICK;
    }
}
