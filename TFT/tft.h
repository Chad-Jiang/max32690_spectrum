/******************************************************************************
 *
 * Copyright (C) 2022-2023 Maxim Integrated Products, Inc. (now owned by 
 * Analog Devices, Inc.),
 * Copyright (C) 2023-2024 Analog Devices, Inc.
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
 * @brief   TFT Demo Example!
 *
 * @details
 */

/***** Includes *****/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "icc.h"
#include "mxc_device.h"
#include "mxc_delay.h"
#include "led.h"
#include "pb.h"
#include "tft_st7735.h"

void TFT_Print(char *str, int len, int x, int y, int font);

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "tft_st7735.h"

#define FFT_SIZE       256
#define FFT_BINS       (FFT_SIZE / 2)

/* 绘图区，底部留出频率文字的位置 */
#define PLOT_X0        2
#define PLOT_X1        125
#define PLOT_Y0        3
#define PLOT_Y1        108

#define PLOT_WIDTH     (PLOT_X1 - PLOT_X0 + 1)
#define PLOT_HEIGHT    (PLOT_Y1 - PLOT_Y0 + 1)

/* RGB565颜色 */
#define COLOR_BG       0x0000
#define COLOR_AXIS     0xFFFF
#define COLOR_TRACE    0x07E0

/*
 * 第一版先使用相对dB。
 * 后续完成ADC幅度标定后，再改成真正的dBFS。
 */
#define DB_MAX         0.0f
#define DB_MIN        (-80.0f)

extern int16_t old_y[PLOT_WIDTH];
extern float smooth_db[FFT_BINS];
extern uint8_t first_frame;

float clamp_float(float value, float min, float max);

int spectrum_db_to_y(float db);

void Spectrum_Init(void);

void Spectrum_Draw(const float *fft_mag);


/************* peak  **************/
#define SAMPLE_RATE_HZ   180000U
#define PEAK_COLOR       0xFFE0  /* 黄色 */
#define PEAK_LABEL_W     38
#define PEAK_LABEL_H     12

extern uint8_t old_peak_valid;
extern int16_t old_peak_x;
extern int16_t old_peak_y;
extern int16_t old_label_x;
extern int16_t old_label_y;

int Spectrum_FindMaxBin(void);

int Spectrum_BinToX(int bin);

uint32_t Spectrum_BinToHz(int bin);

void Spectrum_ClearRectangle(int x, int y, int width, int height);
// 绘制最大 Peak
void Spectrum_DrawMaxPeak(int bin);
// 擦除上一帧的 Peak 标记和频率
void Spectrum_ClearMaxPeak(void);