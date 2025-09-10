#ifndef __SPEED_H
#define __SPEED_H
#include "stm32f10x.h" // Device header

// 自定义布尔类型（STM32标准库兼容）
#define FALSE 0
#define TRUE 1
// 状态机枚举
typedef enum {
    STATE_START,// 系统开始状态
    STATE_LEADING_EMPTY,// 前导空坑状态
    STATE_CHIPS,// 芯片统计状态
    STATE_TRAILING_EMPTY// 尾部空坑状态
} CountState;
// 全局变量
extern volatile uint32_t hole_count ;          // 定位孔计数
extern volatile uint32_t lead_empty_count ;    // 前导空坑计数
extern volatile uint32_t chips_count ;         // 芯片计数
extern volatile uint32_t trail_empty_count ;   // 尾部空坑计数
extern volatile uint8_t consecutive_empty ;    // 连续空坑计数
extern volatile uint8_t middle_empty ;       // 中间空坑计数
//extern volatile uint8_t hole_detected ;        // 定位孔检测标志
extern volatile uint8_t chip_detection_pending ; // 芯片检测待处理标志

extern volatile CountState current_state ;


// 函数声明
void GPIO_Configuration(void);
void TIM_InputCapture_Configuration(void);
void NVIC_Configuration(void);
uint8_t DetectChip(void);
void ProcessChipDetection(void);
void PrintStatistics(void);
void HandleError(const char* message);
void ResetCounter(void);
void Display_message(void);
void Begin_counting_control_init(void);//开始计数控制初始化
void Begin_counting_control(void);//开始计数控制


#endif
