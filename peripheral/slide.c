#include "slide.h"
#include "uart.h"
#include "string.h"
#include "protocol.h"
#include "PT32Y003x.h"


// 滑动检测结构体
typedef struct {
    slide_state_e state;
    slide_direction_e direction;
    uint8_t lastKey;
    uint8_t keyCount;
    uint32_t lastKeyTick;
    uint8_t keySequence[5]; 
} slide_detector_t;

slide_detector_t gSlideDetector;  // 滑动操作结构体


static void slide_initDetector(slide_detector_t * pSlideDetector){
    pSlideDetector->state = SLIDE_IDLE;
    pSlideDetector->direction = SLIDE_NONE;
    pSlideDetector->lastKey = 0;
    pSlideDetector->keyCount = 0;
    pSlideDetector->lastKeyTick = 0;
    memset(pSlideDetector->keySequence, 0, sizeof(pSlideDetector->keySequence));
}

#define SLIDE_SDO_PORT 	GPIOC
#define	SLIDE_SDO_PIN	GPIO_Pin_3

#define SLIDE_SCL_PORT 	GPIOC
#define	SLIDE_SCL_PIN	GPIO_Pin_4

#define SLIDE_SCL_HIGH	GPIO_SetBits(SLIDE_SCL_PORT,SLIDE_SCL_PIN);
#define SLIDE_SCL_LOW	GPIO_ResetBits(SLIDE_SCL_PORT,SLIDE_SCL_PIN);

#define SLIDE_SDO_HIGH	GPIO_SetBits(SLIDE_SDO_PORT,SLIDE_SDO_PIN);
#define SLIDE_SDO_LOW	GPIO_ResetBits(SLIDE_SDO_PORT,SLIDE_SDO_PIN);


static void sdo_in(void){
	GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_In;
    GPIO_InitStructure.GPIO_Pull = GPIO_Pull_Up;
	GPIO_InitStructure.GPIO_Pin  = SLIDE_SDO_PIN;
    GPIO_Init(SLIDE_SDO_PORT,&GPIO_InitStructure);
}

static void sdo_out(void){
	GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OutPP;
    GPIO_InitStructure.GPIO_Pull = GPIO_Pull_NoPull;
	GPIO_InitStructure.GPIO_Pin  = SLIDE_SDO_PIN;
    GPIO_Init(SLIDE_SDO_PORT,&GPIO_InitStructure);
}

void slide_init(void){
	GPIO_InitTypeDef GPIO_InitStructure;
	//SCL - 输出
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OutPP;
    GPIO_InitStructure.GPIO_Pull = GPIO_Pull_NoPull;
	GPIO_InitStructure.GPIO_Pin  = SLIDE_SCL_PIN;
    GPIO_Init(SLIDE_SCL_PORT,&GPIO_InitStructure);
	GPIO_SetBits(SLIDE_SCL_PORT,SLIDE_SCL_PIN);
    
    // 初始化滑动检测器
    slide_initDetector(&gSlideDetector);
}

static uint8_t slide_readSDO(void){
	return GPIO_ReadDataBit(SLIDE_SDO_PORT ,SLIDE_SDO_PIN);
}

static uint8_t slide_keyOut(void){
	uint8_t keys = 0;
	sdo_out();
	SLIDE_SDO_HIGH;
	usTick_Delay(100);
	SLIDE_SDO_LOW;
	usTick_Delay(20);
	
	sdo_in();
	for(int i = 0;i <16; i++){
		SLIDE_SCL_HIGH;	
		usTick_Delay(100);
		SLIDE_SCL_LOW;
		usTick_Delay(1);	
		if(slide_readSDO() == RESET){
			keys = i+1;
		}
	}
	Tick_Delay(1);
	return keys;
}

const static uint8_t minSlideKeys = 3; 
// 检查按键序列是否形成有效滑动
static bool slide_checkPattern(slide_detector_t* detector) {
    
    if (detector->keyCount < minSlideKeys) {
        return false;
    }
    return true;
}

// 检测滑动操作
static bool slide_detectSlide(slide_detector_t* detector, uint8_t currentKey, uint32_t currentTick) {
    if (currentKey == 0) {
        detector->lastKeyTick = currentTick;
        return false;
    }

    switch(detector->state) {
        case SLIDE_IDLE:
        case SLIDE_COMPLETE:
            detector->state = SLIDE_DETECTING;
            detector->lastKey = currentKey;
            detector->keyCount = 1;
            detector->keySequence[0] = currentKey;
            detector->lastKeyTick = currentTick;
            detector->direction = SLIDE_NONE;
            return false;

        case SLIDE_DETECTING:
            if (currentKey == detector->lastKey) {
                detector->lastKeyTick = currentTick;
                return false;
            }

            // 首次确定方向
            if (detector->direction == SLIDE_NONE) {
                detector->direction = (currentKey > detector->lastKey) ? SLIDE_TO_FORWARD : SLIDE_TO_BACKWARD;
            }

            // 方向校验
            bool directionOk = false;
            if (detector->direction == SLIDE_TO_FORWARD && currentKey > detector->lastKey) directionOk = true;
            else if (detector->direction == SLIDE_TO_BACKWARD && currentKey < detector->lastKey) directionOk = true;

            if (directionOk) {
                // 记录按键序列
                if (detector->keyCount < 5) {
                    detector->keySequence[detector->keyCount++] = currentKey;
                }
                detector->lastKey = currentKey;
                detector->lastKeyTick = currentTick;

                // 满足条件，触发滑动
                if (slide_checkPattern(detector)) {
                    detector->state = SLIDE_COMPLETE;
                    return true;
                }
            } else {
                // 方向错误，重置
                    detector->state = SLIDE_IDLE;  // 只重置状态，不清除其他字段
					detector->direction = SLIDE_NONE;
					detector->keyCount = 0;
					return false;
            }
            break;

        default:
            slide_initDetector(detector);
            break;
    }
    return false;
}

#define SLIDE_TIMEOUT_TICK    300   // 1500毫秒超时

// 检查超时并重置检测器
static void slide_checkTimeOut(slide_detector_t* detector, uint32_t currentTick) {
    if (detector->state != SLIDE_IDLE && 
        (currentTick - detector->lastKeyTick) > SLIDE_TIMEOUT_TICK) {
        slide_initDetector(detector);
    }
}

#define COOL_DOWN_PERIOD  100// 500ms冷却时间

static bool coolingDown = false;
static uint32_t coolDownStartTime = 0;
static bool keyPressed = false;
static uint8_t lastKey = 0;

//防卡死保护
static void slide_antiStuckCheck(void) {
    static uint32_t lastCheckTick = 0;
    static uint32_t lastStateChangeTick = 0;
    static slide_state_e lastState = SLIDE_IDLE;

    uint32_t currentTick = Tick_Get();
    if (currentTick - lastCheckTick >= 100) {
        lastCheckTick = currentTick;

        if (gSlideDetector.state != lastState) {
            lastState = gSlideDetector.state;
            lastStateChangeTick = currentTick;
        }

        // 1秒卡死强制重置
        if (gSlideDetector.state != SLIDE_IDLE && 
            (currentTick - lastStateChangeTick) > 200) {
            // 只重置检测器，不干扰冷却状态
            slide_initDetector(&gSlideDetector);
            keyPressed = false;
            lastKey = 0;
        }
    }
}


void slide_task(void){
    slide_antiStuckCheck();
    slide_checkTimeOut(&gSlideDetector, Tick_Get());
    
    // 检测冷却时间
    if(coolingDown){
        if(Tick_Get() - coolDownStartTime > COOL_DOWN_PERIOD){
            coolingDown = false;
            // 冷却结束后，重置所有状态
            slide_initDetector(&gSlideDetector);
            keyPressed = false;
            lastKey = 0;
        } else {
            return;  // 冷却期间不处理任何按键
        }
    }
    
    uint8_t slideKey = slide_keyOut();
    
    if (slideKey != 0) {
        if ( slideKey != lastKey || !keyPressed) {
			DBG_LN("Slide Key: %d", slideKey);
            // 新按键按下
            if (slide_detectSlide(&gSlideDetector, slideKey, Tick_Get())) {
                DBG_LN("Slide detected! Direction: %s", 
                      (gSlideDetector.direction == SLIDE_TO_FORWARD) ? 
                      "Forward" : "Backward");
                
                // 先发送数据
                if(gSlideDetector.direction == SLIDE_TO_FORWARD){
                    reportSlideStatus(SLIDE_TO_FORWARD);
                } else {
                    reportSlideStatus(SLIDE_TO_BACKWARD);
                }
                
                // 然后重置状态
                coolingDown = true;
                coolDownStartTime = Tick_Get();
                slide_initDetector(&gSlideDetector);
            }
            keyPressed = true;
            lastKey = slideKey;
        }
    } else {
        // 按键释放
        if (keyPressed) {
            // 发送释放信号
            slide_detectSlide(&gSlideDetector, 0, Tick_Get());
            keyPressed = false;
            lastKey = 0;
        }
    }
}