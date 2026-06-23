#include "clap.h"
#include "uart.h"
#include "PT32Y003x.h"
#include "threshold.h"

#define CLAP_SDO_PORT 	GPIOC
#define	CLAP_SDO_PIN	GPIO_Pin_3

#define CLAP_SCL_PORT 	GPIOC
#define	CLAP_SCL_PIN	GPIO_Pin_4

#define CLAP_SCL_HIGH	GPIO_SetBits(CLAP_SCL_PORT,CLAP_SCL_PIN);
#define CLAP_SCL_LOW	GPIO_ResetBits(CLAP_SCL_PORT,CLAP_SCL_PIN);

#define CLAP_SDO_HIGH	GPIO_SetBits(CLAP_SDO_PORT,CLAP_SDO_PIN);
#define CLAP_SDO_LOW	GPIO_ResetBits(CLAP_SDO_PORT,CLAP_SDO_PIN);


static void sdo_in(void){
	GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_In;
    GPIO_InitStructure.GPIO_Pull = GPIO_Pull_Up;
	GPIO_InitStructure.GPIO_Pin  = CLAP_SDO_PIN;
    GPIO_Init(CLAP_SDO_PORT,&GPIO_InitStructure);
}

static void sdo_out(void){
	GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OutPP;
    GPIO_InitStructure.GPIO_Pull = GPIO_Pull_NoPull;
	GPIO_InitStructure.GPIO_Pin  = CLAP_SDO_PIN;
    GPIO_Init(CLAP_SDO_PORT,&GPIO_InitStructure);
}

void clap_init(void){
	GPIO_InitTypeDef GPIO_InitStructure;
	
	//SCL - 输出
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OutPP;
    GPIO_InitStructure.GPIO_Pull = GPIO_Pull_Up;
	GPIO_InitStructure.GPIO_Pin  = CLAP_SCL_PIN;
    GPIO_Init(CLAP_SCL_PORT,&GPIO_InitStructure);
	
}

static uint8_t clap_readSDO(void){
	return GPIO_ReadDataBit(CLAP_SDO_PORT ,CLAP_SDO_PIN);
}

static uint8_t clap_keyOut(void) {
    uint8_t keys = 0;
	static bool keyPressed = false;
	static uint8_t lastKey = 0;
	sdo_out();
	CLAP_SDO_HIGH;
	usTick_Delay(100);
	CLAP_SDO_LOW;
	usTick_Delay(20);
	
	sdo_in();
    for(int i = 0; i < 8; i++) {
		CLAP_SCL_HIGH;	
		usTick_Delay(100);
		CLAP_SCL_LOW;
		usTick_Delay(1);
        if(clap_readSDO() == RESET) {
            keys = i + 1;
            break;
        }
    }
    if (keys != 0) {
		// 新按键按下
		if (!keyPressed || keys != lastKey) {
			keyPressed = true;
			lastKey = keys;
			DBG_LN("Key %d pressed, waiting release...", keys);
		}
		return 0; // 按键还未释放，返回0
	} 
	else if (keyPressed) {
		// 按键已释放
		uint8_t releasedKey = lastKey;
		keyPressed = false;
		lastKey = 0;
		DBG_LN("Key %d released", releasedKey);
		return releasedKey;
	}
	return 0; // 无按键状态
}


typedef struct {
    uint8_t pressCount;              // 当前连拍计数
    uint32_t firstPressTick;   // 第一次按键时间
    uint32_t lastPressTick;    // 最后一次按键时间
    uint8_t lastDetectedKey;   // 最后检测到的按键
} clap_multi_detector_t;

static clap_multi_detector_t multiDetector = {0};


// 重置检测器
static void resetMultiPressDetector(void) {
    multiDetector.pressCount = 0;
    multiDetector.lastDetectedKey = 0;
	multiDetector.firstPressTick = 0;
	multiDetector.lastPressTick = 0;
}
static clap_count_e detectMultiPress(uint8_t currentKey, uint32_t currentTick) {    
    const clap_param_t * clapParam = ClapParam_Read(); 
    
    if (currentKey != 0) {
        // 按键按下
        if (multiDetector.pressCount == 0) {
            // 第一次按下
            multiDetector.firstPressTick = currentTick;
            multiDetector.pressCount = 1;
            multiDetector.lastDetectedKey = currentKey;
            //DBG_LN("First press, count=1, key=%d", currentKey);
        } 
        else {
            // 检查是否有效连拍
            uint32_t timeSinceLastPress = currentTick - multiDetector.lastPressTick;
            
            if (timeSinceLastPress < clapParam -> min_interval_time) {
                // 间隔太短，认为是抖动
                //DBG_LN("Press too fast, ignored. Interval: %d ticks", timeSinceLastPress);
                return CLAP_ZERO;
            }
            
            if (timeSinceLastPress > clapParam -> max_timeout) {
                // 超时，重新开始计数
                //DBG_LN("Timeout, reset to 1. Time since last: %d ticks", timeSinceLastPress);
                multiDetector.pressCount = 1;
                multiDetector.firstPressTick = currentTick;
                multiDetector.lastDetectedKey = currentKey;
            } 
            else {
                // 有效连拍
                multiDetector.pressCount++;
                multiDetector.lastDetectedKey = currentKey;
                
                // 检测到3次，立即返回结果
                if (multiDetector.pressCount >= 3) {
                    //DBG_LN("Triple press detected, count=%d, key=%d", multiDetector.pressCount, currentKey);
                    uint8_t result = (multiDetector.pressCount > CLAP_TRIPLE) ? CLAP_TRIPLE : multiDetector.pressCount;
                    multiDetector.pressCount = 0;  // 重置计数
                    return (clap_count_e)result;
                }
                
                //DBG_LN("Multi-press, count=%d, key=%d", multiDetector.pressCount, currentKey);
            }
        }
        
        // 更新释放时间
        multiDetector.lastPressTick = currentTick;
        return CLAP_ZERO;
    } 
    else {
        // 按键释放
        if (multiDetector.pressCount > 0) {
            // 检查是否超时
            uint32_t timeSinceFirstPress = currentTick - multiDetector.firstPressTick;
            
            if (timeSinceFirstPress > clapParam -> max_timeout) {
                // 超时，返回当前次数
                clap_count_e result = (clap_count_e)multiDetector.pressCount;
                if (result > CLAP_TRIPLE) {
                    result = CLAP_TRIPLE;
                }
                
                //DBG_LN("Timeout, final result: %d, total time: %d ticks", result, timeSinceFirstPress);
                multiDetector.pressCount = 0;
                return result;
            }
        }
    }
    
    return CLAP_ZERO;
}

void clap_task(void){
	/******************连拍逻辑**********************/
	uint8_t clapKey = clap_keyOut();
	uint32_t currentTick = Tick_Get();
	// 检测连拍
	clap_count_e multiPress = detectMultiPress(clapKey, currentTick);
	if(multiPress){
		reportClapStatus(multiPress);
	}
}