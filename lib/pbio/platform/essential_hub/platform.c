// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 The Pybricks Authors

#include <stdbool.h>

#include <btstack_chipset_cc256x.h>
#undef UNUSED
#include <stm32f4xx_hal.h>

#include "pbio/uartdev.h"
#include "pbio/light_matrix.h"
#include "pbio/version.h"

#include "../../drv/adc/adc_stm32_hal.h"
#include "../../drv/bluetooth/bluetooth_btstack_control_gpio.h"
#include "../../drv/bluetooth/bluetooth_btstack_uart_block_stm32_hal.h"
#include "../../drv/bluetooth/bluetooth_btstack.h"
#include "../../drv/button/button_gpio.h"
#include "../../drv/charger/charger_mp2639a.h"
#include "../../drv/ioport/ioport_lpf2.h"
#include "../../drv/led/led_pwm.h"
#include "../../drv/pwm/pwm_lp50xx_stm32.h"
#include "../../drv/pwm/pwm_stm32_tim.h"
#include "../../drv/sound/sound_stm32_hal_dac.h"
#include "../../drv/uart/uart_stm32f4_ll_irq.h"

// Special symbols for firmware compatibility with official apps
typedef struct {
    const char *fw_ver;
    const uint32_t *checksum;
    const void *reserved;
    const char *id_string;
    const void *reserved2;
} lego_fw_info_t;

// defined in linker script
extern const uint32_t _checksum;

const lego_fw_info_t __attribute__((section(".fw_info"))) fw_info = {
    // These values are not used.
    .fw_ver = PBIO_VERSION_STR,
    .checksum = &_checksum,
    .reserved = NULL,
    // This value is checked when installing the firmware
    // via the 'firmware' MicroPython module on the hub.
    .id_string = "LEGO Technic Small Hub(0x000D)",
    .reserved2 = NULL,
};

enum {
    COUNTER_PORT_A,
    COUNTER_PORT_B,
};

enum {
    LED_DEV_0_STATUS,
    LED_DEV_1_BATTERY,
};

enum {
    PWM_DEV_0_TIM2,
    PWM_DEV_1_TIM3,
    PWM_DEV_2_TIM4,
    PWM_DEV_3_LP50XX,
};

enum {
    UART_PORT_A,
    UART_PORT_B,
};

// Bluetooth

const pbdrv_bluetooth_btstack_control_gpio_platform_data_t pbdrv_bluetooth_btstack_control_gpio_platform_data = {
    .enable_gpio = {
        .bank = GPIOC,
        .pin = 8,
    },
};

const pbdrv_bluetooth_btstack_uart_block_stm32_platform_data_t pbdrv_bluetooth_btstack_uart_block_stm32_platform_data = {
    .uart = USART2,
    .uart_irq = USART2_IRQn,
    .tx_dma = DMA1_Stream6,
    .tx_dma_ch = DMA_CHANNEL_4,
    .tx_dma_irq = DMA1_Stream6_IRQn,
    .rx_dma = DMA1_Stream7,
    .rx_dma_ch = DMA_CHANNEL_6,
    .rx_dma_irq = DMA1_Stream7_IRQn,
};

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        GPIO_InitTypeDef gpio_init;

        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF7_USART2;

        // CTS/RTS
        gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        // TX/RX
        gpio_init.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
}

void DMA1_Stream6_IRQHandler(void) {
    pbdrv_bluetooth_btstack_uart_block_stm32_hal_handle_tx_dma_irq();
}

void DMA1_Stream7_IRQHandler(void) {
    pbdrv_bluetooth_btstack_uart_block_stm32_hal_handle_rx_dma_irq();
}

void USART2_IRQHandler(void) {
    pbdrv_bluetooth_btstack_uart_block_stm32_hal_handle_uart_irq();
}

const pbdrv_bluetooth_btstack_platform_data_t pbdrv_bluetooth_btstack_platform_data = {
    .uart_block_instance = pbdrv_bluetooth_btstack_uart_block_stm32_hal_instance,
    .chipset_instance = btstack_chipset_cc256x_instance,
    .control_instance = pbdrv_bluetooth_btstack_control_gpio_instance,
    .er_key = (const uint8_t *)UID_BASE,
    .ir_key = (const uint8_t *)UID_BASE,
};

// button

const pbdrv_button_gpio_platform_t pbdrv_button_gpio_platform[PBDRV_CONFIG_BUTTON_GPIO_NUM_BUTTON] = {
    [0] = {
        .gpio = { .bank = GPIOB, .pin = 2 },
        .pull = PBDRV_GPIO_PULL_NONE,
        .button = PBIO_BUTTON_CENTER,
        .active_low = true,
    },
};

// charger

const pbdrv_charger_mp2639a_platform_data_t pbdrv_charger_mp2639a_platform_data = {
    .mode_gpio = { .bank = GPIOA, .pin = 10 },
    .chg_gpio = { .bank = GPIOC, .pin = 6 },
    .ib_adc_ch = 3,
};

// I/O ports

const pbdrv_ioport_lpf2_platform_data_t pbdrv_ioport_lpf2_platform_data = {
    .port_vcc = { .bank = GPIOC, .pin = 7 },
    .ports = {
        // Port A
        {
            .id1 = { .bank = GPIOC, .pin = 1 },
            .id2 = { .bank = GPIOC, .pin = 0 },
            .uart_buf = { .bank = GPIOB, .pin = 9 },
            .uart_tx = { .bank = GPIOC, .pin = 12 },
            .uart_rx = { .bank = GPIOD, .pin = 2 },
            .alt = GPIO_AF8_UART5,
        },
        // Port B
        {
            .id1 = { .bank = GPIOA, .pin = 5 },
            .id2 = { .bank = GPIOA, .pin = 4 },
            .uart_buf = { .bank = GPIOB, .pin = 8 },
            .uart_tx = { .bank = GPIOC, .pin = 10 },
            .uart_rx = { .bank = GPIOC, .pin = 11 },
            .alt = GPIO_AF7_USART3,
        },
    },
};

// LED

const pbdrv_led_pwm_platform_data_t pbdrv_led_pwm_platform_data[PBDRV_CONFIG_LED_PWM_NUM_DEV] = {
    {
        .id = LED_DEV_0_STATUS,
        .r_id = PWM_DEV_3_LP50XX,
        .r_ch = 3,
        .g_id = PWM_DEV_3_LP50XX,
        .g_ch = 4,
        .b_id = PWM_DEV_3_LP50XX,
        .b_ch = 5,
        .scale_factor = 35,
    },
    {
        .id = LED_DEV_1_BATTERY,
        .r_id = PWM_DEV_3_LP50XX,
        .r_ch = 0,
        .g_id = PWM_DEV_3_LP50XX,
        .g_ch = 1,
        .b_id = PWM_DEV_3_LP50XX,
        .b_ch = 2,
        .scale_factor = 35,
    },
};

// PWM

static void pwm_dev_0_platform_init(void) {
    GPIO_InitTypeDef gpio_init;

    gpio_init.Pin = GPIO_PIN_3;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    // channel 2 has constant 50% duty cycle since it acts as a clock
    TIM2->CCR2 = TIM2->ARR / 2;
}

static void pwm_dev_1_platform_init(void) {
}

static void pwm_dev_2_platform_init(void) {
}

// NOTE: Official LEGO firmware uses 1.2 kHz PWM for motors. We have changed to
// 12 kHz to reduce the unpleasant noise (similar to the frequency used by the
// official EV3 firmware).

const pbdrv_pwm_stm32_tim_platform_data_t
    pbdrv_pwm_stm32_tim_platform_data[PBDRV_CONFIG_PWM_STM32_TIM_NUM_DEV] = {
    {
        .platform_init = pwm_dev_0_platform_init,
        .TIMx = TIM2,
        .prescalar = 1, // results in 96 MHz clock
        .period = 2930, // 96 MHz divided by 2930 makes 32.765 kHz PWM
        .id = PWM_DEV_0_TIM2,
        // channel 2: Bluetooth 32.768 kHz clock
        .channels = PBDRV_PWM_STM32_TIM_CHANNEL_2_ENABLE,
    },
    {
        .platform_init = pwm_dev_1_platform_init,
        .TIMx = TIM3,
        .prescalar = 8, // results in 12 MHz clock
        .period = 1000, // 12MHz divided by 1k makes 12 kHz PWM
        .id = PWM_DEV_1_TIM3,
        // channel 1/2: Port B motor driver
        .channels = PBDRV_PWM_STM32_TIM_CHANNEL_1_ENABLE | PBDRV_PWM_STM32_TIM_CHANNEL_2_ENABLE
            | PBDRV_PWM_STM32_TIM_CHANNEL_1_INVERT | PBDRV_PWM_STM32_TIM_CHANNEL_2_INVERT,
    },
    {
        .platform_init = pwm_dev_2_platform_init,
        .TIMx = TIM4,
        .prescalar = 8, // results in 12 MHz clock
        .period = 1000, // 12MHz divided by 1k makes 12 kHz PWM
        .id = PWM_DEV_2_TIM4,
        // channel 1/2: Port A
        .channels = PBDRV_PWM_STM32_TIM_CHANNEL_1_ENABLE | PBDRV_PWM_STM32_TIM_CHANNEL_2_ENABLE
            | PBDRV_PWM_STM32_TIM_CHANNEL_1_INVERT | PBDRV_PWM_STM32_TIM_CHANNEL_2_INVERT,
    },
};

const pbdrv_pwm_lp50xx_stm32_platform_data_t
    pbdrv_pwm_lp50xx_stm32_platform_data[PBDRV_CONFIG_PWM_LP50XX_STM32_NUM_DEV] = {
    {
        .i2c = FMPI2C1,
        .i2c_ev_irq = FMPI2C1_EV_IRQn,
        .i2c_er_irq = FMPI2C1_ER_IRQn,
        .rx_dma = DMA1_Stream0,
        .rx_dma_ch = DMA_CHANNEL_7,
        .rx_dma_irq = DMA1_Stream0_IRQn,
        .tx_dma = DMA1_Stream1,
        .tx_dma_ch = DMA_CHANNEL_2,
        .tx_dma_irq = DMA1_Stream1_IRQn,
        .en_gpio = GPIOB,
        .en_gpio_pin = GPIO_PIN_13,
        .id = PWM_DEV_3_LP50XX,
    },
};

void DMA1_Stream0_IRQHandler(void) {
    pbdrv_pwm_lp50xx_stm32_rx_dma_irq(0);
}

void DMA1_Stream1_IRQHandler(void) {
    pbdrv_pwm_lp50xx_stm32_tx_dma_irq(0);
}

void FMPI2C1_EV_IRQHandler(void) {
    pbdrv_pwm_lp50xx_stm32_i2c_ev_irq(0);
}

void FMPI2C1_ER_IRQHandler(void) {
    pbdrv_pwm_lp50xx_stm32_i2c_er_irq(0);
}

void HAL_FMPI2C_MspInit(FMPI2C_HandleTypeDef *hfmpi2c) {
    GPIO_InitTypeDef gpio_init;

    gpio_init.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    gpio_init.Mode = GPIO_MODE_AF_OD;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = GPIO_AF4_FMPI2C1;
    HAL_GPIO_Init(GPIOB, &gpio_init);
}

// Reset

void pbdrv_reset_power_off(void) {
    // setting PB1 low cuts the power
    GPIOB->BSRR = GPIO_BSRR_BR_1;
}


// UART

const pbdrv_uart_stm32f4_ll_irq_platform_data_t
    pbdrv_uart_stm32f4_ll_irq_platform_data[PBDRV_CONFIG_UART_STM32F4_LL_IRQ_NUM_UART] = {
    [UART_PORT_A] = {
        .uart = UART5,
        .irq = UART5_IRQn,
    },
    [UART_PORT_B] = {
        .uart = USART3,
        .irq = USART3_IRQn,
    },
};

void UART5_IRQHandler(void) {
    pbdrv_uart_stm32f4_ll_irq_handle_irq(UART_PORT_A);
}

void USART3_IRQHandler(void) {
    pbdrv_uart_stm32f4_ll_irq_handle_irq(UART_PORT_B);
}

const pbio_uartdev_platform_data_t pbio_uartdev_platform_data[PBIO_CONFIG_UARTDEV_NUM_DEV] = {
    [COUNTER_PORT_A] = {
        .uart_id = UART_PORT_A,
        .counter_id = COUNTER_PORT_A,
    },
    [COUNTER_PORT_B] = {
        .uart_id = UART_PORT_B,
        .counter_id = COUNTER_PORT_B,
    },
};


// STM32 HAL integration

// bootloader gives us 16MHz clock
uint32_t SystemCoreClock = 16000000;

// copied from system_stm32.c in stm32 port
const uint8_t AHBPrescTable[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9 };
const uint8_t APBPrescTable[8] = { 0, 0, 0, 0, 1, 2, 3, 4 };

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc) {
    GPIO_InitTypeDef gpio_init = { 0 };
    ADC_ChannelConfTypeDef adc_ch_config = { 0 };

    // clocks are enabled in SystemInit
    assert_param(__HAL_RCC_TIM8_IS_CLK_ENABLED());
    assert_param(__HAL_RCC_DMA2_IS_CLK_ENABLED());
    assert_param(__HAL_RCC_ADC1_IS_CLK_ENABLED());

    // PA7-AD7, ADC_IBAT

    gpio_init.Pin = GPIO_PIN_7;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    adc_ch_config.Channel = ADC_CHANNEL_7;
    adc_ch_config.Rank = 1;
    adc_ch_config.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    adc_ch_config.Offset = 0;
    HAL_ADC_ConfigChannel(hadc, &adc_ch_config);

    // PA6-AD6, ADCVBAT

    gpio_init.Pin = GPIO_PIN_6;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    adc_ch_config.Channel = ADC_CHANNEL_6;
    adc_ch_config.Rank = 2;
    adc_ch_config.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    adc_ch_config.Offset = 0;
    HAL_ADC_ConfigChannel(hadc, &adc_ch_config);

    // PB0-AD8, BAT_NTC

    gpio_init.Pin = GPIO_PIN_0;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    adc_ch_config.Channel = ADC_CHANNEL_8;
    adc_ch_config.Rank = 3;
    adc_ch_config.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    adc_ch_config.Offset = 0;
    HAL_ADC_ConfigChannel(hadc, &adc_ch_config);

    // PC4-AD14, IBUSBCH

    gpio_init.Pin = GPIO_PIN_4;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    adc_ch_config.Channel = ADC_CHANNEL_14;
    adc_ch_config.Rank = 4;
    adc_ch_config.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    adc_ch_config.Offset = 0;
    HAL_ADC_ConfigChannel(hadc, &adc_ch_config);

    // PC5, PORT_VCC

    gpio_init.Pin = GPIO_PIN_5;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    adc_ch_config.Channel = ADC_CHANNEL_15;
    adc_ch_config.Rank = 5;
    adc_ch_config.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    adc_ch_config.Offset = 0;
    HAL_ADC_ConfigChannel(hadc, &adc_ch_config);
}

void DMA2_Stream0_IRQHandler(void) {
    pbdrv_adc_stm32_hal_handle_irq();
}

// USB

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd) {
    GPIO_InitTypeDef GPIO_InitStruct;

    // Data pins
    GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // VBUS pin
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd) {
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
}

void OTG_FS_IRQHandler(void) {
    extern PCD_HandleTypeDef hpcd;
    HAL_PCD_IRQHandler(&hpcd);
}

// IMU

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c) {
    GPIO_InitTypeDef gpio_init;

    if (hi2c->Instance == I2C3) {
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF4_I2C3;

        // SCL
        gpio_init.Pin = GPIO_PIN_8;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        // SDA
        gpio_init.Pin = GPIO_PIN_9;
        HAL_GPIO_Init(GPIOC, &gpio_init);

        // REVISIT: PC13 is also used in the official LEGO firmware - probably
        // an interrupt back from the IMU chip

        HAL_NVIC_SetPriority(I2C3_ER_IRQn, 3, 1);
        HAL_NVIC_EnableIRQ(I2C3_ER_IRQn);
        HAL_NVIC_SetPriority(I2C3_EV_IRQn, 3, 2);
        HAL_NVIC_EnableIRQ(I2C3_EV_IRQn);
    }
}

void I2C3_ER_IRQHandler(void) {
    extern void mod_experimental_IMU_handle_i2c_er_irq(void);
    mod_experimental_IMU_handle_i2c_er_irq();
}

void I2C3_EV_IRQHandler(void) {
    extern void mod_experimental_IMU_handle_i2c_ev_irq(void);
    mod_experimental_IMU_handle_i2c_ev_irq();
}

// Early initialization

// special memory addresses defined in linker script
extern uint32_t *_fw_isr_vector_src;

// Called from assembly code in startup.s
void SystemInit(void) {
    // enable 8-byte stack alignment for IRQ handlers, in accord with EABI
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;

    // since the firmware starts at 0x08008000, we need to set the vector table offset
    SCB->VTOR = (uint32_t)&_fw_isr_vector_src;

    // bootloader disables interrupts
    __enable_irq();

    // Using external 16Mhz oscillator
    RCC_OscInitTypeDef osc_init;
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc_init.HSEState = RCC_HSE_ON;
    osc_init.LSEState = RCC_LSE_OFF;
    osc_init.HSIState = RCC_HSI_OFF;
    osc_init.LSIState = RCC_LSI_OFF;
    osc_init.PLL.PLLState = RCC_PLL_ON;
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc_init.PLL.PLLM = 8; // VCO_IN 2MHz (16MHz / 8)
    osc_init.PLL.PLLN = 96; // VCO_OUT 192MHz (2MHz * 96)
    osc_init.PLL.PLLP = RCC_PLLP_DIV2; // PLLCLK 96MHz (not using max of 100 because of USB)
    osc_init.PLL.PLLQ = 4; // 48MHz USB clock
    osc_init.PLL.PLLR = 2; // 96MHz system clock

    HAL_RCC_OscConfig(&osc_init);

    RCC_ClkInitTypeDef clk_init;
    clk_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;  // HCLK 96MHz
    clk_init.APB1CLKDivider = RCC_HCLK_DIV2; // 48MHz
    clk_init.APB2CLKDivider = RCC_HCLK_DIV1; // 96MHz

    HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_5);

    // enable clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN |
        RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN | RCC_APB1ENR_USART3EN | RCC_APB1ENR_UART5EN |
        RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM4EN |
        RCC_APB1ENR_I2C3EN | RCC_APB1ENR_FMPI2C1EN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM8EN | RCC_APB2ENR_ADC1EN | RCC_APB2ENR_SYSCFGEN;
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

    // Keep main power on (PB1)
    GPIO_InitTypeDef gpio_init = {
        .Pin = GPIO_PIN_1,
        .Mode = GPIO_MODE_OUTPUT_PP,
    };
    HAL_GPIO_Init(GPIOB, &gpio_init);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}
