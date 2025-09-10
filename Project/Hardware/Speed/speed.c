#include "stm32f10x.h"
#include "speed.h"
#include "Delay.h"
#include "OLED.h"
#include "USART1.h"
// 全局变量
volatile uint32_t hole_count = 2;        // 定位孔计数
volatile uint32_t lead_empty_count = 0;  // 前导空坑计数
volatile uint32_t chips_count = 0;       // 芯片计数
volatile uint32_t trail_empty_count = 0; // 尾部空坑计数
volatile uint8_t consecutive_empty = 0;  // 连续空坑计数
volatile uint8_t middle_empty = 0;       // 中间空坑计数
// volatile uint8_t hole_detected = FALSE;          // 定位孔检测标志
volatile uint8_t chip_detection_pending = FALSE; // 芯片检测待处理标志

volatile CountState current_state = STATE_START; // 运行状态

// GPIO配置函数
void GPIO_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 使能GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 配置定位孔传感器引脚(PA0)为浮空输入 - TIM2_CH1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置芯片传感器引脚(PA1)为上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置(PA2)为复位清零引脚
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置LED指示灯(PC13)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// 定时器输入捕获配置
void TIM_InputCapture_Configuration(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;

    // 使能TIM2时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 定时器时基配置
    TIM_TimeBaseStructure.TIM_Period = 65536 - 1;               // 自动重装载值
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;               // 72MHz/72 = 1MHz，1us分辨率
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;                // 不分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 输入捕获配置
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;                // 使用通道1
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;     // 上升沿捕获
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; // 直接或间接/TRC模式
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;           // 分频
    TIM_ICInitStructure.TIM_ICFilter = 0x0F;                        // 硬件滤波去抖
    TIM_ICInit(TIM2, &TIM_ICInitStructure);

    // 使能捕获中断
    TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);

    // 使能定时器
    // TIM_Cmd(TIM2, ENABLE);//由外部按键控制是否开始进行计数
}

// NVIC中断配置
void NVIC_Configuration(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 优先级分组
    NVIC_InitTypeDef NVIC_InitStructure;

    // 配置TIM2中断
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

// 检测芯片是否存在
uint8_t DetectChip(void)
{
    // 读取芯片传感器状态，根据实际硬件调整逻辑
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == Bit_SET)
    {
        return TRUE; // 检测到芯片
    }
    else
    {
        return FALSE; // 未检测到芯片
    }
}

// 处理芯片检测逻辑
void ProcessChipDetection(void)
{
    uint8_t chip_present = DetectChip(); // 检测芯片是否存在

    switch (current_state)
    {
    case STATE_START:
        if (chip_present == FALSE)
        {
            lead_empty_count++;
            consecutive_empty++;
            if (consecutive_empty >= 3) // 连续3个空坑认为进入前导区域
            {
                current_state = STATE_LEADING_EMPTY;
            }
        }
        else
        {
            chips_count++;
            consecutive_empty = 0;
            current_state = STATE_CHIPS;
        }
        break;

    case STATE_LEADING_EMPTY:
        if (chip_present == FALSE)
        {
            lead_empty_count++;
            consecutive_empty++;
        }
        else
        {
            chips_count++;
            consecutive_empty = 0;
            current_state = STATE_CHIPS;
        }
        break;

    case STATE_CHIPS:
        if (chip_present == FALSE)
        {
            consecutive_empty++;
            if (consecutive_empty >= 5) // 连续5个空坑认为进入尾部区域
            {
                trail_empty_count = consecutive_empty;
                current_state = STATE_TRAILING_EMPTY;
            }
        }
        else
        {
            if (consecutive_empty >= 1)
            {
                middle_empty += consecutive_empty;
            }
            chips_count++;
            consecutive_empty = 0;
        }
        break;

    case STATE_TRAILING_EMPTY:
        if (chip_present == FALSE)
        {
            trail_empty_count++;
        }
        else
        {
            HandleError("芯片出现在尾部区域"); // LED快速闪烁并串口打印表示错误信息
        }
        break;

    default:
        current_state = STATE_START;
        break;
    }
}

// TIM2输入捕获中断处理
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET) // 定时器2捕获中断
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_CC1); // 清除中断标志位

        // 载带开始计数控制
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == Bit_RESET)
        {
            // 定位孔计数增加
            hole_count++;

            // 只在奇数个定位孔时触发芯片检测
            if (hole_count % 2 == 1)
            {
                chip_detection_pending = TRUE;
            }

            // 设置检测标志
            // hole_detected = TRUE; // 检测到定位孔标志

        } // 载带开始计数控制
    }
}

// 输出统计信息
void PrintStatistics(void)
{
    // 在实际应用中，这里可以通过串口输出信息
    Serial_Printf("=== Statistics ===\r\n");
    Serial_Printf("上升沿数-Hole Count: %lu\r\n", hole_count - 2);
    Serial_Printf("前空数-Lead Empty: %u\r\n", lead_empty_count);
    Serial_Printf("中间空数-Middle Empty: %u\r\n", middle_empty);
    Serial_Printf("芯片数-Chips Count: %lu\r\n", chips_count);
    Serial_Printf("后空数-Trail Empty: %lu\r\n", trail_empty_count);
    Serial_Printf("当前状态-Current State: %u\r\n", current_state);
    Serial_Printf("=====End Count====\r\n");

    // 简单LED闪烁表示工作正常
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);
    Delay_ms(100);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);
}

// 错误处理函数，用于处理芯片出现在尾部区域的情况
void HandleError(const char *message)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        Serial_Printf("Error!!!-在结尾空段中发现芯片\r\n");
    }
    // LED快速闪烁表示错误
    for (uint8_t i = 0; i < 8; i++)
    {
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);
        Delay_ms(100);
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);
        Delay_ms(100);
    }
}

// 重置计数器
void ResetCounter(void)
{
    hole_count = 2;
    lead_empty_count = 0;
    chips_count = 0;
    trail_empty_count = 0;
    consecutive_empty = 0;
    current_state = STATE_START;
    chip_detection_pending = FALSE;
    // hole_detected = FALSE;

    TIM_SetCounter(TIM2, 0); // 定时器计数清零
    TIM_Cmd(TIM2, ENABLE);
}

void Display_message(void) // OLED显示信息
{
    // 开始显示系统信息
    OLED_ShowString(0, 0, "HT-carrier-count", OLED_8X16);

    // 第一行: hole数量 + 标签 (左侧)
    OLED_ShowString(16, 16, "H:", OLED_8X16);
    OLED_ShowNum(16 + 16, 16, hole_count - 2, 4, OLED_8X16);

    // 第一行: lead empty数量 + 标签 (右侧，增加间距)
    OLED_ShowString(32 + 16 + 24, 16, "L:", OLED_8X16);
    OLED_ShowNum(32 + 16 + 24 + 16, 16, lead_empty_count, 3, OLED_8X16);

    // 第二行: chips数量 + 标签 (左侧)
    OLED_ShowString(16, 18 + 14, "C:", OLED_8X16);
    OLED_ShowNum(16 + 16, 18 + 14, chips_count, 4, OLED_8X16);

    // 第二行: trail empty数量 + 标签 (右侧，增加间距)
    OLED_ShowString(32 + 16 + 24, 18 + 14, "T:", OLED_8X16);
    OLED_ShowNum(32 + 16 + 24 + 16, 18 + 14, trail_empty_count, 3, OLED_8X16);

    // 第三行: 当前状态 + 标签 (居中)
    OLED_ShowString(48, 36 + 12, "S:", OLED_8X16);
    OLED_ShowNum(48 + 16, 36 + 12, current_state, 2, OLED_8X16);

    OLED_Update(); // 刷新显示
}
void Begin_counting_control_init(void) // 开始计数控制初始化
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}
void Begin_counting_control(void) // 开始计数控制
{
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 0)
    {
        TIM_Cmd(TIM2, ENABLE);
    }
    else
    {
        TIM_Cmd(TIM2, DISABLE);
    }
}
