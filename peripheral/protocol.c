#include <string.h>
#include "config.h"
#include "protocol.h"
#include "uart.h"
#include "crc.h"
#include "buildInfo.h"
#include "threshold.h"

//发送连拍检测结果
void reportClapStatus(clap_count_e pClapCounts)
{
	msg_clap_t report;
	report.clap_counts = pClapCounts;
	FILL_CLAP_MSG(report);

#if (DEBUG_MODE == 0)
	//上报消息
	Uart_SendData((uint8_t*)&report, sizeof(report));
#else
	//打印
	static char buf[100];
	memset(buf, 0, sizeof(buf));
	int len = 0;
	uint8_t* p = (uint8_t*)&report;
	for (int i = 0; i < sizeof(report); i++) {
		len += sprintf(buf + len, "%02X ", *p++);
	}
	len += sprintf(buf + len, "\n");
	Uart_SendStr(buf);
#endif
}

//发送滑动检测结果
void reportSlideStatus(slide_direction_e pSlideDirection)
{
	msg_slide_t report;
	report.slide_direction = pSlideDirection;
	FILL_SLIDE_MSG(report);

#if (DEBUG_MODE == 0)
	//上报消息
	Uart_SendData((uint8_t*)&report, sizeof(report));
#else
	//打印
	static char buf[100];
	memset(buf, 0, sizeof(buf));
	int len = 0;
	uint8_t* p = (uint8_t*)&report;
	for (int i = 0; i < sizeof(report); i++) {
		len += sprintf(buf + len, "%02X ", *p++);
	}
	len += sprintf(buf + len, "\n");
	Uart_SendStr(buf);
#endif
}

//发送参数调节结果
void reportClapParam()
{
	msg_clap_time_param_t report;
	const clap_param_t *clapParam = ClapParam_Read();
	report.min_interval_time = clapParam->min_interval_time;
	report.max_timeout = clapParam->max_timeout;
	FILL_RESP_MSG(report);

#if (DEBUG_MODE == 0)
	//上报消息
	Uart_SendData((uint8_t*)&report, sizeof(report));
#else
	//打印
	static char buf[100];
	memset(buf, 0, sizeof(buf));
	int len = 0;
	uint8_t* p = (uint8_t*)&report;
	for (int i = 0; i < sizeof(report); i++) {
		len += sprintf(buf + len, "%02X ", *p++);
	}
	len += sprintf(buf + len, "\n");
	Uart_SendStr(buf);
#endif
}



//获取命令参数，从1号开始
static int parseIntPara(const uint8_t* pMsg, uint16_t length, int paraId) {
	if (paraId < 1) {
		return 0;
	}
	for (int i = 0; i < length && pMsg[i] >= ' '; i++) {
		if (pMsg[i] == ',') {
			if (--paraId == 0) {
				return atoi((const char*)&pMsg[i + 1]);
			}
		}
	}
	return 0;
}


bool Protocol_HandleMsg(const uint8_t* pMsg, uint16_t length) {
//判断命令名
#define IS_CMD(cmd) (0 == strncmp((const char*)pMsg, cmd, sizeof(cmd) - 1))
	if (pMsg[0] <= 'z') {
		//首先处理文本命令
		if (IS_CMD("reboot")) {
			Uart_SendStrForCmd("\n\n ...To Reboot...\n\n");
			while(1); //重启
		} else if (IS_CMD("build info")) {
			Uart_SendStrForCmd("\tbuild time: " BUILD_DATE " " BUILD_TIME "\n\tfw: " FULL_VERSION_STR "\n\thw: SS_CX2504-Prj1-Main-250410-A1_0\n");
			return true;
		}
	}
	
		//二进制消息处理
	if (length < sizeof(msg_header_t)) {
		DBG_LN("msg len %d is too short", length);
		return false;
	}

	const msg_header_t* pHeader = (const msg_header_t*)pMsg;
	if (length < pHeader->size) {
		DBG_LN("length %d < size %d", length, pHeader->size); //debug mode才会自动生效
		return false;
	}
	
	if (!CHECK_CRC(pHeader)) {
		DBG_LN("crc mismatch");
		return false;
	}
	
	msg_clap_time_param_t* configMsg = (msg_clap_time_param_t*)pMsg;
	//如果两个时间参数都为0值就恢复默认值
	if(configMsg->min_interval_time == 0 && configMsg->max_timeout == 0){
		configMsg->min_interval_time = MULTI_PRESS_INTERVAL_TICK;
		configMsg->max_timeout = MULTI_PRESS_TIMEOUT_TICK;
	}
	
	//写入新的时间参数
	clap_param_t newClapParam;
	newClapParam.min_interval_time = configMsg->min_interval_time;
	newClapParam.max_timeout = configMsg->max_timeout;
	
	if(!ClapParam_Write(&newClapParam)){
		return false;
	}
	reportClapParam();
    return true;
}
