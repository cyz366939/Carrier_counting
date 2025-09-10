/********************************************************************************
 * @file    main.c
 * @author  褚耀宗
 * @version V1.0.1
 * @date    2025-09-7
 * @brief   Main program body - 载带芯片数据统计系统
 *******************************************************************************/
/*******************************************************************************
Hardware Connection:
 * PA0: 定位孔传感器-DO
 * PA1: 载带芯片检测传感器-DO
 * PA2: 重置清零按键-KEY
 * PA3: 系统开始计数控制-Button
 * PC13: 系统启动指示灯-LED(+)
 * USART1: 串口通信：PB6(TX)和PB7(RX)
 * OLED: 用于显示统计信息: SCL:PB8,SDA:PB9
*******************************************************************************/
/*
 * Project History:
 * 2025-09-1 - Initial version created
 * 2025-09-6 - Added serial communication functionality
 * 2025-09-7 - Optimized display refresh algorithm
 */

#include "stm32f10x.h"
#include "OLED.h"
#include "speed.h"
#include "Delay.h"
#include "USART1.h"

// 主函数
int main(void)
{
    // 外设初始化
    GPIO_Configuration();
    TIM_InputCapture_Configuration();
    NVIC_Configuration();
    OLED_Init();
    USART1_Init();
    Begin_counting_control_init();

    // 系统启动指示
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);
    Delay_ms(500);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);

    // 打印系统启动信息
    Serial_Printf("===System started===\r\n");

    while (1)
    {

        // 处理待处理的芯片检测
        if (chip_detection_pending == TRUE)
        {
            ProcessChipDetection();
            chip_detection_pending = FALSE;
        }

        // 定期输出统计结果
        static uint32_t last_print = 0;
        if (hole_count % ((24) * 2 + 1) /*需要打印的坑位数*/ == 0 && hole_count != last_print)
        { /*每计算到多少个坑时就输出一次统计信息，这里的24即为数量，+1是最后结束时为不遮挡；
            如果最后结束时有隔离区域会先出现一个无效遮挡然后不遮挡，此时需要+2*/
            last_print = hole_count;
            PrintStatistics(); // 串口打印统计信息
        }
        // 更新显示
        Display_message(); // OLED屏幕显示信息

        // 检测是否需要重置数据（通过按键）
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == Bit_RESET)
        { // PA2接按键
            ResetCounter();
            Delay_ms(500); // 防抖
        }

        // 短暂延时
        // Delay_ms(1);

    } // while

} // main
