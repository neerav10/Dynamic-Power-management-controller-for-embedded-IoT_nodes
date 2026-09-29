#include "stm32l476xx.h"
#include <stdint.h>

/* =========================================================
 * PIN DEFINITIONS
 * ========================================================= */

/* LM35 -> PA0 -> ADC1_IN5 */
#define LM35_PIN        0

/* MQ-2 Analog Output -> PA1 -> ADC1_IN6 */
#define MQ2_PIN         1


/* =========================================================
 * CLOCK INITIALIZATION
 *
 * HSI16 = 16 MHz
 * ========================================================= */
void Clock_Init(void)
{
    /* Enable HSI16 */
    RCC->CR |= RCC_CR_HSION;

    /* Wait until HSI is ready */
    while (!(RCC->CR & RCC_CR_HSIRDY));

    /* Select HSI as SYSCLK */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSI;

    /* Wait until HSI becomes system clock */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
}


/* =========================================================
 * TIM2 INITIALIZATION
 *
 * TIM2 clock = 16 MHz
 *
 * Prescaler:
 * 16000 - 1
 *
 * Timer frequency:
 *
 * 16 MHz / 16000 = 1 kHz
 *
 * Therefore:
 *
 * 1 timer count = 1 ms
 *
 * This gives us delay_ms() without crude software loops.
 * ========================================================= */
void TIM2_Init(void)
{
    /* Enable TIM2 clock */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

    /*
     * Timer clock = 16 MHz
     *
     * 16,000,000 / 16,000 = 1,000 Hz
     */
    TIM2->PSC = 16000 - 1;

    /* Maximum ARR */
    TIM2->ARR = 0xFFFFFFFF;

    /* Reset counter */
    TIM2->CNT = 0;

    /* Enable timer */
    TIM2->CR1 |= TIM_CR1_CEN;
}


/* =========================================================
 * DELAY IN MILLISECONDS
 *
 * Uses TIM2.
 *
 * Example:
 *
 * delay_ms(1000);
 *
 * = approximately 1 second
 * ========================================================= */
void delay_ms(uint32_t ms)
{
    uint32_t start = TIM2->CNT;

    while ((uint32_t)(TIM2->CNT - start) < ms)
    {
        /* Wait for timer */
    }
}


/* =========================================================
 * USART1 INITIALIZATION
 *
 * PA9  = USART1_TX
 * PA10 = USART1_RX
 *
 * Baud = 115200
 * 8 data bits
 * No parity
 * 1 stop bit
 *
 * HSI16 = 16 MHz
 * BRR = 16000000 / 115200 ≈ 139
 * ========================================================= */
void USART1_Init(void)
{
    /* Enable GPIOA clock */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* Enable USART1 clock */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;


    /* -----------------------------------------------------
     * Select HSI16 as USART1 clock source
     * ----------------------------------------------------- */

    RCC->CCIPR &= ~RCC_CCIPR_USART1SEL;

    /*
     * USART1SEL = 01
     * HSI16
     */
    RCC->CCIPR |= RCC_CCIPR_USART1SEL_1;


    /* -----------------------------------------------------
     * PA9 and PA10 -> Alternate Function mode
     * ----------------------------------------------------- */

    GPIOA->MODER &= ~((3U << 18) | (3U << 20));

    GPIOA->MODER |= ((2U << 18) | (2U << 20));


    /* -----------------------------------------------------
     * AF7 = USART1
     * ----------------------------------------------------- */

    GPIOA->AFR[1] &= ~((0xFU << 4) | (0xFU << 8));

    GPIOA->AFR[1] |= ((7U << 4) | (7U << 8));


    /* -----------------------------------------------------
     * USART configuration
     * ----------------------------------------------------- */

    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = 0;

    /*
     * 16 MHz / 115200 ≈ 138.89
     */
    USART1->BRR = 139;


    /* Enable transmitter and receiver */
    USART1->CR1 |= USART_CR1_TE;
    USART1->CR1 |= USART_CR1_RE;

    /* Enable USART */
    USART1->CR1 |= USART_CR1_UE;


    /* Wait for transmitter ready */
    while (!(USART1->ISR & USART_ISR_TEACK));
}


/* =========================================================
 * USART1 SEND CHARACTER
 * ========================================================= */
void USART1_SendChar(char c)
{
    while (!(USART1->ISR & USART_ISR_TXE));

    USART1->TDR = (uint8_t)c;
}


/* =========================================================
 * USART1 SEND STRING
 * ========================================================= */
void USART1_SendString(const char *str)
{
    while (*str)
    {
        USART1_SendChar(*str++);
    }
}


/* =========================================================
 * SEND UNSIGNED INTEGER
 * ========================================================= */
void USART1_SendNumber(uint32_t value)
{
    char buffer[11];
    int i = 0;

    if (value == 0)
    {
        USART1_SendChar('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        USART1_SendChar(buffer[--i]);
    }
}


/* =========================================================
 * SEND FLOAT WITH ONE DECIMAL PLACE
 * ========================================================= */
void USART1_SendFloat(float value)
{
    if (value < 0.0f)
    {
        USART1_SendChar('-');
        value = -value;
    }

    uint32_t integer_part = (uint32_t)value;

    uint32_t fractional_part =
        (uint32_t)((value - (float)integer_part) * 10.0f);

    USART1_SendNumber(integer_part);

    USART1_SendChar('.');

    USART1_SendNumber(fractional_part);
}


/* =========================================================
 * ADC1 INITIALIZATION
 *
 * PA0 = ADC1_IN5
 * PA1 = ADC1_IN6
 *
 * ADC resolution = 12-bit
 * Range = 0 ... 4095
 * ========================================================= */
void ADC1_Init(void)
{
    /* Enable GPIOA clock */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* Enable ADC clock */
    RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;


    /* -----------------------------------------------------
     * PA0 and PA1 -> Analog mode
     * ----------------------------------------------------- */

    GPIOA->MODER |= (3U << 0);
    GPIOA->MODER |= (3U << 2);

    /* No pull-up / pull-down */
    GPIOA->PUPDR &= ~(3U << 0);
    GPIOA->PUPDR &= ~(3U << 2);


    /* Enable analog switches */
    GPIOA->ASCR |= GPIO_ASCR_ASC0;
    GPIOA->ASCR |= GPIO_ASCR_ASC1;


    /* -----------------------------------------------------
     * ADC clock
     *
     * HCLK / 1
     * ===================================================== */

    ADC123_COMMON->CCR &= ~ADC_CCR_CKMODE;
    ADC123_COMMON->CCR |= ADC_CCR_CKMODE_0;


    /* -----------------------------------------------------
     * Exit deep power down
     * ----------------------------------------------------- */

    ADC1->CR &= ~ADC_CR_DEEPPWD;

    /* Enable ADC voltage regulator */
    ADC1->CR |= ADC_CR_ADVREGEN;


    /* Allow regulator to stabilize */
    delay_ms(1);


    /* -----------------------------------------------------
     * ADC CALIBRATION
     * ----------------------------------------------------- */

    ADC1->CR &= ~ADC_CR_ADCALDIF;

    ADC1->CR |= ADC_CR_ADCAL;

    while (ADC1->CR & ADC_CR_ADCAL);


    /* -----------------------------------------------------
     * Basic ADC configuration
     * ----------------------------------------------------- */

    ADC1->CFGR = 0;


    /* -----------------------------------------------------
     * Sampling time
     *
     * Channel 5 and Channel 6:
     * 247.5 ADC clock cycles
     *
     * Long sampling time is useful for sensor signals.
     * ----------------------------------------------------- */

    /* Channel 5 -> SMPR1 bits 17:15 */
    ADC1->SMPR1 &= ~(7U << 15);
    ADC1->SMPR1 |=  (6U << 15);

    /* Channel 6 -> SMPR1 bits 20:18 */
    ADC1->SMPR1 &= ~(7U << 18);
    ADC1->SMPR1 |=  (6U << 18);


    /* -----------------------------------------------------
     * ADC sequence
     *
     * Conversion 1 = Channel 5 -> LM35
     * Conversion 2 = Channel 6 -> MQ2
     *
     * L = 1 means 2 conversions
     * ----------------------------------------------------- */

    ADC1->SQR1 = 0;

    /* L = 1 */
    ADC1->SQR1 |= (1U << 0);

    /* SQ1 = Channel 5 */
    ADC1->SQR1 |= (5U << 6);

    /* SQ2 = Channel 6 */
    ADC1->SQR1 |= (6U << 12);


    /* -----------------------------------------------------
     * Clear ADC ready flag
     * ----------------------------------------------------- */

    ADC1->ISR |= ADC_ISR_ADRDY;


    /* Enable ADC */
    ADC1->CR |= ADC_CR_ADEN;

    /* Wait for ADC ready */
    while (!(ADC1->ISR & ADC_ISR_ADRDY));
}


/* =========================================================
 * ADC READ
 *
 * Reads two channels:
 *
 * adc_lm35 -> channel 5
 * adc_mq2  -> channel 6
 * ========================================================= */
void ADC1_ReadSensors(uint16_t *lm35_adc,
                      uint16_t *mq2_adc)
{
    /* Clear EOC/EOS flags */
    ADC1->ISR |= ADC_ISR_EOC;
    ADC1->ISR |= ADC_ISR_EOS;


    /* Start ADC conversion sequence */
    ADC1->CR |= ADC_CR_ADSTART;


    /* -----------------------------------------------
     * Conversion 1
     * Channel 5 = LM35
     * ----------------------------------------------- */

    while (!(ADC1->ISR & ADC_ISR_EOC));

    *lm35_adc = (uint16_t)ADC1->DR;


    /* -----------------------------------------------
     * Conversion 2
     * Channel 6 = MQ2
     * ----------------------------------------------- */

    while (!(ADC1->ISR & ADC_ISR_EOC));

    *mq2_adc = (uint16_t)ADC1->DR;


    /* Wait until entire sequence is complete */
    while (!(ADC1->ISR & ADC_ISR_EOS));

    /* Clear EOS */
    ADC1->ISR |= ADC_ISR_EOS;
}


/* =========================================================
 * CONVERT LM35 ADC TO TEMPERATURE
 *
 * ADC:
 * 0 ... 4095
 *
 * ADC reference:
 * 3.3 V
 *
 * LM35:
 * 10 mV / °C
 *
 * Therefore:
 *
 * Temperature =
 * ADC * 330 / 4095
 * ========================================================= */
float LM35_GetTemperature(uint16_t adc_value)
{
    return ((float)adc_value * 330.0f) / 4095.0f;
}


/* =========================================================
 * MQ-2 VOLTAGE
 *
 * ADC -> voltage
 *
 * V = ADC * 3.3 / 4095
 * ========================================================= */
float MQ2_GetVoltage(uint16_t adc_value)
{
    return ((float)adc_value * 3.3f) / 4095.0f;
}


/* =========================================================
 * MQ-2 PPM ESTIMATION
 *
 * IMPORTANT:
 *
 * This is a BASELINE / PROTOTYPE mapping.
 *
 * It maps:
 *
 * 0 V    -> 0 ppm
 * 3.3 V  -> 1000 ppm
 *
 * A real MQ-2 ppm value requires sensor calibration,
 * R0 measurement and the appropriate Rs/R0 gas curve.
 *
 * This function can later be replaced by the calibrated
 * MQ-2 conversion.
 * ========================================================= */
float MQ2_GetPPM(uint16_t adc_value)
{
    float voltage = MQ2_GetVoltage(adc_value);

    float ppm = (voltage / 3.3f) * 1000.0f;

    return ppm;
}


/* =========================================================
 * MAIN
 * ========================================================= */
int main(void)
{
    uint16_t lm35_adc;
    uint16_t mq2_adc;

    float temperature;
    float mq2_ppm;


    /* -----------------------------------------------------
     * INITIALIZATION
     * ----------------------------------------------------- */

    Clock_Init();

    TIM2_Init();

    USART1_Init();

    ADC1_Init();


    /* -----------------------------------------------------
     * MAIN LOOP
     * ----------------------------------------------------- */

    while (1)
    {
        /* Read both sensors */
        ADC1_ReadSensors(&lm35_adc, &mq2_adc);


        /* Convert sensor values */
        temperature = LM35_GetTemperature(lm35_adc);

        mq2_ppm = MQ2_GetPPM(mq2_adc);


        /* -------------------------------------------------
         * UART PACKET
         *
         * Example:
         *
         * TEMP:28.4,MQ2:325.6
         *
         * This goes directly to ESP32.
         * ------------------------------------------------- */

        USART1_SendString("TEMP:");

        USART1_SendFloat(temperature);

        USART1_SendString(",MQ2:");

        USART1_SendFloat(mq2_ppm);

        USART1_SendString("\r\n");


        /* -------------------------------------------------
         * Sampling interval = 1 second
         *
         * TIM2 based delay
         * ------------------------------------------------- */

        delay_ms(1000);
    }
}
