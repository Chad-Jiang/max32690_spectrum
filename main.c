/******************************************************************************
 *
 * Copyright (C) 2022-2023 Maxim Integrated Products, Inc. (now owned by 
 * Analog Devices, Inc.),
 * Copyright (C) 2023-2025 Analog Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

/**
 * @file    main.c
 * @brief   ADC demo application
 * @details Continuously monitors the ADC channels
 */

/***** Includes *****/
#include <stdio.h>
#include <stdint.h>
#include "mxc_device.h"
#include "mxc_delay.h"
#include "mxc_errors.h"
#include "adc.h"
#include "dma.h"
#include "fcr_regs.h"
#include "led.h"
#include "tmr.h"
#include "nvic_table.h"
/***** Definitions *****/

//#define POLLING // Uncomment to perform ADC conversions using blocking/polling method
// #define INTERRUPT    // Uncomment to perform ADC conversions using interrupt driven method
// #define DMA          // Uncomment to perform ADC conversions using DMA driven method.

#define REF_TRIM //Uncomment to perform the reference trim after initialization

#define SOFTWARE // Uncomment to perform ADC conversions using a software trigger
// #define HARDWARE     // Uncomment to perform ADC conversions using a hardware trigger

#define CH MXC_ADC_CH_0

/* Supported three ADC examples */
#define SINGLE_CH 1
#define TEMP_SENSOR 2
#define MULTI_CHS 3
#define TEMP_SENSOR_READ_OUT 1

/***** Globals *****/
#ifdef INTERRUPT
volatile unsigned int adc_done = 0;
#endif



#ifdef REF_TRIM
//These values are dependent on the sample rate of the ADC. For 250ksps, the
//values are below. See table 11-4 in the user guide for other sample rates
#define TRIM_BIAS_COUNT 5
#define TRIM_WAKE_COUNT 3
#endif

/* Temperature Sensor firmware average*/
#define SAMPLE_AVG 16
float TEMP_SAMPLES[SAMPLE_AVG] = {};
float sum = 0;
unsigned int temp_samples = 0;


/* Single Channel */
mxc_adc_slot_req_t single_slot = { MXC_ADC_CH_0 };

/* It is recommended to use below sequence if only user wants to measure only temperature measurement.*/
mxc_adc_slot_req_t three_slots[3] = { { MXC_ADC_CH_VDDA_DIV2 },
                                      { MXC_ADC_CH_TEMP_SENS },
                                      { MXC_ADC_CH_VCOREA } };

mxc_adc_slot_req_t multi_slots[8] = { { MXC_ADC_CH_0 }, { MXC_ADC_CH_1 }, { MXC_ADC_CH_2 },
                                      { MXC_ADC_CH_3 }, { MXC_ADC_CH_4 }, { MXC_ADC_CH_5 },
                                      { MXC_ADC_CH_6 }, { MXC_ADC_CH_7 } };



/***** Functions *****/






#ifdef REF_TRIM
/* Performs the 1.25V Internal Reference Trim
 * Derived from UG7618 Section 11.6.3 - 1.25V Internal Reference Trim
 */
void adc_internal_1v25_ref_trim(void)
{
    uint32_t tempData;

    //Enter nap state
    MXC_ADC->ctrl0 &= ~MXC_F_ADC_CTRL0_ADC_EN;

    //Clear Skip Cal
    MXC_ADC->ctrl0 &= ~MXC_F_ADC_CTRL0_SKIP_CAL;

    MXC_ADC->sfraddr = 0x0B;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xC0;
    tempData |= ((MXC_FCR->adcreftrim0 & MXC_F_FCR_ADCREFTRIM0_VX2_TUNE) >>
                 MXC_F_FCR_ADCREFTRIM0_VX2_TUNE_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x0C;
    tempData = ((MXC_FCR->adcreftrim2 & MXC_F_FCR_ADCREFTRIM2_IBOOST_1P25) >>
                MXC_F_FCR_ADCREFTRIM2_IBOOST_1P25_POS)
               << 7;
    tempData |=
        ((MXC_FCR->adcreftrim0 & MXC_F_FCR_ADCREFTRIM0_VREFP) >> MXC_F_FCR_ADCREFTRIM0_VREFP_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x0D;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0x80;
    tempData |=
        ((MXC_FCR->adcreftrim0 & MXC_F_FCR_ADCREFTRIM0_VREFM) >> MXC_F_FCR_ADCREFTRIM0_VREFM_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x0E;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0x0C;
    tempData |= ((MXC_FCR->adcreftrim2 & MXC_F_FCR_ADCREFTRIM2_IDRV_1P25) >>
                 MXC_F_FCR_ADCREFTRIM2_IDRV_1P25_POS)
                << 4;
    tempData |=
        ((MXC_FCR->adcreftrim0 & MXC_F_FCR_ADCREFTRIM0_VCM) >> MXC_F_FCR_ADCREFTRIM0_VCM_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x05;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xF0;
    tempData |= TRIM_BIAS_COUNT;
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x06;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xF0;
    tempData |= TRIM_WAKE_COUNT;
    MXC_ADC->sfrwrdata = tempData;

    //Enable
    MXC_ADC->ctrl0 |= MXC_F_ADC_CTRL0_ADC_EN;

    //Wait for 30us
    MXC_Delay(30);

    //wait for calibration to complete
    while (!(MXC_ADC->intfl & MXC_F_ADC_INTFL_READY)) {}
}

/* Performs the 2.048V Internal Reference Trim
 * Derived from UG7618 Section 11.6.4 - 2.048V Internal Reference Trim
 */
void adc_internal_2v048_ref_trim(void)
{
    uint32_t tempData;

    //Enter nap state
    MXC_ADC->ctrl0 &= ~MXC_F_ADC_CTRL0_ADC_EN;

    //Clear Skip Cal
    MXC_ADC->ctrl0 &= ~MXC_F_ADC_CTRL0_SKIP_CAL;

    MXC_ADC->sfraddr = 0x0B;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xC0;
    tempData |= ((MXC_FCR->adcreftrim1 & MXC_F_FCR_ADCREFTRIM1_VX2_TUNE) >>
                 MXC_F_FCR_ADCREFTRIM1_VX2_TUNE_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x0C;
    tempData = ((MXC_FCR->adcreftrim2 & MXC_F_FCR_ADCREFTRIM2_IBOOST_2P048) >>
                MXC_F_FCR_ADCREFTRIM2_IBOOST_2P048_POS)
               << 7;
    tempData |=
        ((MXC_FCR->adcreftrim1 & MXC_F_FCR_ADCREFTRIM1_VREFP) >> MXC_F_FCR_ADCREFTRIM1_VREFP_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x0D;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0x80;
    tempData |=
        ((MXC_FCR->adcreftrim1 & MXC_F_FCR_ADCREFTRIM1_VREFM) >> MXC_F_FCR_ADCREFTRIM1_VREFM_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x0E;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0x0C;
    tempData |= ((MXC_FCR->adcreftrim2 & MXC_F_FCR_ADCREFTRIM2_IDRV_2P048) >>
                 MXC_F_FCR_ADCREFTRIM2_IDRV_2P048_POS)
                << 4;
    tempData |=
        ((MXC_FCR->adcreftrim1 & MXC_F_FCR_ADCREFTRIM1_VCM) >> MXC_F_FCR_ADCREFTRIM1_VCM_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x05;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xF0;
    tempData |= TRIM_BIAS_COUNT;
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x06;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xF0;
    tempData |= TRIM_WAKE_COUNT;
    MXC_ADC->sfrwrdata = tempData;

    //Enable
    MXC_ADC->ctrl0 |= MXC_F_ADC_CTRL0_ADC_EN;

    //Wait for 30us
    MXC_Delay(30);

    //wait for calibration to complete
    while (!(MXC_ADC->intfl & MXC_F_ADC_INTFL_READY)) {}
}

/* Performs the External Reference Trim
 * Derived from UG7618 Section 11.6.5 - External Reference Trim
 */
void adc_external_ref_trim(void)
{
    uint32_t tempData;

    //Enter nap state
    MXC_ADC->ctrl0 &= ~MXC_F_ADC_CTRL0_ADC_EN;

    //Clear Skip Cal
    MXC_ADC->ctrl0 &= ~MXC_F_ADC_CTRL0_SKIP_CAL;

    MXC_ADC->sfraddr = 0x0B;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0x80;
    tempData |= ((MXC_FCR->adcreftrim2 & MXC_F_FCR_ADCREFTRIM2_VX2_TUNE) >>
                 MXC_F_FCR_ADCREFTRIM2_VX2_TUNE_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x0E;
    tempData = MXC_ADC->sfrrddata;
    tempData |=
        ((MXC_FCR->adcreftrim2 & MXC_F_FCR_ADCREFTRIM2_VCM) >> MXC_F_FCR_ADCREFTRIM2_VCM_POS);
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x05;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xF0;
    tempData |= TRIM_BIAS_COUNT;
    MXC_ADC->sfrwrdata = tempData;

    MXC_ADC->sfraddr = 0x06;
    tempData = MXC_ADC->sfrrddata;
    tempData &= 0xF0;
    tempData |= TRIM_WAKE_COUNT;
    MXC_ADC->sfrwrdata = tempData;

    //Enable
    MXC_ADC->ctrl0 |= MXC_F_ADC_CTRL0_ADC_EN;

    //Wait for 30us
    MXC_Delay(30);

    //wait for calibration to complete
    while (!(MXC_ADC->intfl & MXC_F_ADC_INTFL_READY)) {}
}

#endif

// USER CODE 
#define ADC_BLOCK_SIZE 256

uint32_t adc_index = 0;
static int adc_val[ADC_BLOCK_SIZE];
static volatile unsigned int dma_done;
static volatile int dma_error;
mxc_adc_conversion_req_t adc_conv;

/*********** dma ***********/

void adc_dma_callback(int ch, int err)
{
    adc_index = (err == E_NO_ERROR) ? ADC_BLOCK_SIZE : 0;
 LED_Off(0);
    MXC_ADC_DisableConversion();

    
    dma_done = 1;
}

void DMA_IRQHandler(void)
{
    MXC_DMA_Handler();
}


/*ADC initialization*/
void adc_init(void)
{
    mxc_adc_req_t adc_cfg;

    adc_cfg.clock = MXC_ADC_HCLK;
    adc_cfg.clkdiv = MXC_ADC_CLKDIV_16;
    adc_cfg.cal = MXC_ADC_EN_CAL;
    adc_cfg.trackCount = 4;
    adc_cfg.idleCount = 17;
    adc_cfg.ref = MXC_ADC_REF_INT_1V25;

    /* Initialize ADC */
    if (MXC_ADC_Init(&adc_cfg) != E_NO_ERROR) {
        printf("Error Bad Parameter\n");
        while (1) {}
    }

#ifdef REF_TRIM
    switch (adc_cfg.ref) {
    case MXC_ADC_REF_INT_1V25:
        adc_internal_1v25_ref_trim();
        break;
    case MXC_ADC_REF_INT_2V048:
        adc_internal_2v048_ref_trim();
        break;
    case MXC_ADC_REF_EXT:
        adc_external_ref_trim();
        break;
    }
#endif
}

/* Single channel */
void adc_example1_configuration(void)
{
    adc_conv.mode = MXC_ADC_CONTINUOUS_CONV; //MXC_ADC_CONTINUOUS_CONV;
    adc_conv.trig = MXC_ADC_TRIG_SOFTWARE;
    //adc_conv.trig = MXC_ADC_TRIG_HARDWARE;
    //adc_conv.hwTrig = MXC_ADC_TRIG_SEL_TEMP_SENS;
    adc_conv.avg_number = MXC_ADC_AVG_1;
    adc_conv.fifo_format = MXC_ADC_DATA; //MXC_ADC_DATA_STATUS;

    // The threshold for DMA should be set to num_slots - 1 to trigger DMA request
    adc_conv.fifo_threshold = 0;

    adc_conv.lpmode_divder = MXC_ADC_DIV_2_5K_50K_ENABLE;
    adc_conv.num_slots = 1; // 这里表示每次只有一个通道，不代表只采一个时间点

    MXC_ADC_Configuration(&adc_conv);

    MXC_ADC_SlotConfiguration(&single_slot, 1);
}
/*********** dma end ***********/

/*********** fft start ***********/
#include <math.h>
#include <arm_math.h>

#define FFT_SIZE       ADC_BLOCK_SIZE
#define ADC_FS_HZ      180000.0f
#define PI_F           3.14159265358979323846f

static arm_rfft_fast_instance_f32 fft_instance;
static int fft_adc[FFT_SIZE]; // adc(处理无效值后的)原始数据
static float32_t fft_input[FFT_SIZE]; 
static float32_t fft_output[FFT_SIZE];
static float32_t fft_magnitude[FFT_SIZE / 2 + 1];
static float32_t hann_window[FFT_SIZE];
int fft_init(void)
{
    arm_status status;

    status = arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);

    if (status != ARM_MATH_SUCCESS) {
        return -1;
    }

    for (uint32_t i = 0; i < FFT_SIZE; i++) {
        hann_window[i] =
            0.5f -
            0.5f * cosf((2.0f * PI_F * i) /
                        (FFT_SIZE - 1));
    }

    return 0;
}
// 预处理数据，返回值1代表有数据
static uint8_t prepare_adc_data(){
    int cnt=0;
    uint32_t raw;
    uint8_t invalid;
    for (int i=0; i < adc_index; i++) {
        if (adc_conv.fifo_format == MXC_ADC_DATA) {
            raw = (uint32_t)adc_val[i];
            invalid = (raw >> 24) & 0x01;
            if(invalid==0){
                fft_adc[cnt++] = raw & 0xFFFF;
                //printf("%d,%06d\n", cnt, data);
                continue;
            }
        }
        fft_adc[cnt++] = 0; // 如果值无效，置0
    }
    return cnt>0?1:0;
}
static void fft_prepare_input(const int *adc_data)
{
    float32_t mean = 0.0f;

    /* 1. 计算256点平均值 */
    for (uint32_t i = 0; i < FFT_SIZE; i++) {
        uint32_t sample = ((uint32_t)adc_data[i]) & 0x0FFF;
        mean += (float32_t)sample;
    }

    mean /= (float32_t)FFT_SIZE;

    /* 2. 去直流并乘Hann窗 */
    for (uint32_t i = 0; i < FFT_SIZE; i++) {
        uint32_t sample = ((uint32_t)adc_data[i]) & 0x0FFF;

        fft_input[i] =
            ((float32_t)sample - mean) *
            hann_window[i];
    }
}
static void fft_calculate_magnitude(void)
{
    fft_magnitude[0] = fabsf(fft_output[0]);

    for (uint32_t k = 1; k < FFT_SIZE / 2; k++) {
        float32_t real = fft_output[2 * k];
        float32_t imag = fft_output[2 * k + 1];

        fft_magnitude[k] =
            sqrtf(real * real + imag * imag);
    }

    /* Nyquist频点是特殊的纯实数 */
    fft_magnitude[FFT_SIZE / 2] =
        fabsf(fft_output[1]);
}
static void fft_find_peak(void)
{
    uint32_t peak_bin = 1;
    float32_t peak_value = fft_magnitude[1];

    for (uint32_t k = 2; k < FFT_SIZE / 2; k++) {
        if (fft_magnitude[k] > peak_value) {
            peak_value = fft_magnitude[k];
            peak_bin = k;
        }
    }

    uint32_t frequency_hz =
        (uint32_t)(((float32_t)peak_bin *
                    ADC_FS_HZ /
                    FFT_SIZE) + 0.5f);

    printf("peak bin=%lu, frequency=%lu Hz\n",
           (unsigned long)peak_bin,
           (unsigned long)frequency_hz);
}
/*********** fft end ***********/

void ShowAdcResult(void)
{
    unsigned int loop_count;
    uint32_t raw;
    uint32_t data;
    uint32_t channel;
    uint32_t invalid;
    uint32_t clipped;
    uint32_t cnt=0;


    // Stop the ADC
    // MXC_ADC_DisableConversion();

    if (adc_conv.fifo_format == MXC_ADC_DATA_STATUS) {
        //printf("CH : Data\n");
    } else {
        //printf("Data\n");
    }
    for (loop_count = 0; loop_count < adc_index; loop_count++) {
        if (adc_conv.fifo_format == MXC_ADC_DATA_STATUS) {
            printf("%02X : %03X\n", (adc_val[loop_count] >> 16), (adc_val[loop_count] & 0x0FFF));
        } else {
            // printf("%03X \n", adc_val[loop_count]);
            raw = (uint32_t)adc_val[loop_count];
            data = raw & 0xFFFF;
            channel = (raw >> 16) & 0x1F;
            invalid = (raw >> 24) & 0x01;
            clipped = (raw >> 31) & 0x01;
            // printf("raw=%08lX ch=%lu data=%03lX invalid=%lu clipped=%lu\n",
            //    (unsigned long)raw,
            //    (unsigned long)channel,
            //    (unsigned long)(data & 0x0FFF),
            //    (unsigned long)invalid,
            //    (unsigned long)clipped);
            if(invalid==0){
                cnt++;
                printf("%d,%06d\n", cnt, data);
            }

    
        }
    }

    adc_index = 0;

}





 extern int MXC_ADC_StartConversionDMA2(mxc_adc_conversion_req_t *req, int *data, int size, void (*callback)(int, int));
int main(void)
{
    int ret;

    printf("********** ADC Example **********\n");
    printf("\nThe voltage applied to analog pin continuously\n");
    printf("measured and the result is printed to the terminal.\n");
    printf("\nThe example can be configured to take the measurements\n");
    printf("by polling, using interrupts, or using DMA.\n\n");

    /* ADC Initialization */
    adc_init();
    /* Start with Single Channel Example */
    adc_example1_configuration();
    //NVIC_EnableIRQ(ADC_IRQn);

    // dma init
    MXC_DMA_Init();
    // printf("Acquire DMA...\n");
    int dma_channel = MXC_DMA_AcquireChannel();
    // printf("DMA channel = %d\n", dma_channel);
    if (dma_channel < 0) {
        printf("Acquire DMA failed: %d\n", dma_channel);
        goto MAINEND;
    }
    adc_conv.dma_channel = dma_channel;
    MXC_NVIC_SetVector(MXC_DMA_CH_GET_IRQ(dma_channel), DMA_IRQHandler);
    NVIC_EnableIRQ(MXC_DMA_CH_GET_IRQ(dma_channel));

    // fft init
    if (fft_init() != 0) {
        printf("FFT init failed\n");
        goto MAINEND;
    }

    // 查看当前频率  120MHz
    SystemCoreClockUpdate();
    printf("HCLK = %lu Hz\n",  (unsigned long)SystemCoreClock);

    while (1) {
        LED_On(0);
       // MXC_TMR_Delay(MXC_TMR0, MSEC(10));
        dma_done = 0;
        //  Start ADC DMA
        ret = MXC_ADC_StartConversionDMA2(&adc_conv, &adc_val[0], ADC_BLOCK_SIZE, adc_dma_callback);
        if (ret != E_NO_ERROR) {
            printf("Start ADC DMA failed\n");
            while (1) {}
        }

        while (!dma_done) {}

        // DMA completed
        // ShowAdcResult();
        if(prepare_adc_data()){
            fft_prepare_input(fft_adc);
            arm_rfft_fast_f32(&fft_instance, fft_input, fft_output, 0);
            fft_calculate_magnitude();
            fft_find_peak();

        }else{
            printf("prepare adc none.\n");
        }

    }

    MXC_DMA_ReleaseChannel(adc_conv.dma_channel);

MAINEND:
    while(1){}
}

//End
