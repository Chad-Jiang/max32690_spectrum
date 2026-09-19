#include "tft.h"

int16_t old_y[PLOT_WIDTH];
float smooth_db[FFT_BINS];
uint8_t first_frame = 1;

void TFT_Print(char *str, int len, int x, int y, int font)
{
    // fonts id
    text_t text;
    text.data = str;
    text.len = len;
    MXC_TFT_PrintFont(x, y, font, &text, NULL);
}

float clamp_float(float value, float min, float max)
{
    if (value < min) {
        return min;
    }

    if (value > max) {
        return max;
    }

    return value;
}

int spectrum_db_to_y(float db)
{
    float ratio;

    db = clamp_float(db, DB_MIN, DB_MAX);

    ratio = (db - DB_MIN) / (DB_MAX - DB_MIN);

    /*
     * 屏幕坐标向下增大：
     * DB_MAX映射到顶部，DB_MIN映射到底部。
     * 留一个像素，防止曲线擦掉边框。
     */
    return (PLOT_Y1 - 1) -
           (int)(ratio * (float)(PLOT_HEIGHT - 3));
}

void Spectrum_Init(void)
{
    int i;
    // tft init
    MXC_Delay(MXC_DELAY_SEC(2));

    /* Enable cache */
    MXC_ICC_Enable(MXC_ICC0);

    /* Set system clock to 100 MHz */
    MXC_SYS_Clock_Select(MXC_SYS_CLOCK_IPO);
    SystemCoreClockUpdate();

    /* Initialize TFT display */
    MXC_TFT_Init();

    // MXC_TFT_SetBackGroundColor(COLOR_BG);
    MXC_TFT_SetBackGroundColor(COLOR_BG); // Clear screen
    MXC_TFT_SetForeGroundColor(WHITE); // set chars to white
    MXC_TFT_ClearScreen();

    /* 频谱区域边框 */
    MXC_TFT_Rectangle(PLOT_X0,
                      PLOT_Y0,
                      PLOT_X1,
                      PLOT_Y1,
                      COLOR_AXIS);

    /*
     * Fs = 180kHz，所以横轴范围为0～90kHz。
     * 如果文字太挤，可以暂时不显示。
     */
    
    TFT_Print("0", 1, 2, 113, (int)&Liberation_Sans12x12[0]);
    TFT_Print("45k", 3, 52, 113, (int)&Liberation_Sans12x12[0]);
    TFT_Print("90k", 3, 98, 113, (int)&Liberation_Sans12x12[0]);

    for (i = 0; i < PLOT_WIDTH; i++) {
        old_y[i] = PLOT_Y1 - 1;
    }

    for (i = 0; i < FFT_BINS; i++) {
        smooth_db[i] = DB_MIN;
    }

    first_frame = 1;
}

void Spectrum_Draw(const float *fft_mag)
{
    int x;
    int k;
    int k_start;
    int k_end;
    uint16_t new_y[PLOT_WIDTH];

    float max_mag;
    float db;
    float pixel_db;
    int max_bin;

    /*
     * 找到这一帧的最大幅值。
     *
     * 忽略bin 0，因为它主要是ADC直流偏置。
     * 第一版用相对最大值显示，因此最强信号显示为0dB。
     */
    max_mag = 0.000001f;

    for (k = 1; k < FFT_BINS; k++) {
        if (fft_mag[k] > max_mag) {
            max_mag = fft_mag[k];
        }
    }

    /*
     * 转换为相对dB并进行平滑。
     * 0.75代表保留上一帧，0.25代表加入当前帧。
     */
    for (k = 1; k < FFT_BINS; k++) {
        db = 20.0f * log10f((fft_mag[k] + 0.000001f) / max_mag);

        db = clamp_float(db, DB_MIN, DB_MAX);

        smooth_db[k] =
            0.75f * smooth_db[k] +
            0.25f * db;
    }
    max_bin = Spectrum_FindMaxBin();

    /*
     * 把127个有效bin映射到屏幕横向像素。
     *
     * bin 0不显示。
     * 如果多个bin落到同一个像素，取其中最大的值，
     * 这样窄带尖峰不会被平均掉。
     */
    for (x = 0; x < PLOT_WIDTH; x++) {
        k_start = 1 +
                  x * (FFT_BINS - 1) / PLOT_WIDTH;

        k_end = 1 +
                (x + 1) * (FFT_BINS - 1) / PLOT_WIDTH;

        if (k_end <= k_start) {
            k_end = k_start + 1;
        }

        if (k_end > FFT_BINS) {
            k_end = FFT_BINS;
        }

        pixel_db = DB_MIN;

        for (k = k_start; k < k_end; k++) {
            if (smooth_db[k] > pixel_db) {
                pixel_db = smooth_db[k];
            }
        }

        new_y[x] = spectrum_db_to_y(pixel_db);
    }

    /* 1. 擦除旧的Peak文字和标记 */
    Spectrum_ClearMaxPeak();
    /*
     * 2. 擦除旧频谱曲线
     * 先用背景色擦除上一条曲线。
     * 第一帧没有旧曲线，不需要擦除。
     */
    if (!first_frame) {
        for (x = 0; x < PLOT_WIDTH - 1; x++) {
            MXC_TFT_Line(PLOT_X0 + x,
                         old_y[x],
                         PLOT_X0 + x + 1,
                         old_y[x + 1],
                         COLOR_BG);
        }
    }

    /* 3. 绘制当前频谱 */
    for (x = 0; x < PLOT_WIDTH - 1; x++) {
        MXC_TFT_Line(PLOT_X0 + x,
                     new_y[x],
                     PLOT_X0 + x + 1,
                     new_y[x + 1],
                     COLOR_TRACE);
    }

    /* 4. 最后标记最大峰，并显示频率 */
    Spectrum_DrawMaxPeak(max_bin);

    /*
     * 擦除曲线时可能碰到边框，所以重新画边框。
     */
    MXC_TFT_Rectangle(PLOT_X0,
                      PLOT_Y0,
                      PLOT_X1,
                      PLOT_Y1,
                      COLOR_AXIS);

    memcpy(old_y, new_y, sizeof(old_y));
    first_frame = 0;
}


/************* peak  **************/

uint8_t old_peak_valid = 0;
int16_t old_peak_x;
int16_t old_peak_y;
int16_t old_label_x;
int16_t old_label_y;

int Spectrum_FindMaxBin(void)
{
    int k;
    int max_bin = 1;
    float max_db = smooth_db[1];

    for (k = 2; k < FFT_BINS; k++) {
        if (smooth_db[k] > max_db) {
            max_db = smooth_db[k];
            max_bin = k;
        }
    }

    return max_bin;
}

int Spectrum_BinToX(int bin)
{
    return PLOT_X0 + 1 +
           (bin - 1) * (PLOT_WIDTH - 3) /
           (FFT_BINS - 2);
}

uint32_t Spectrum_BinToHz(int bin)
{
    return ((uint32_t)bin * SAMPLE_RATE_HZ +
            FFT_SIZE / 2) / FFT_SIZE;
}

void Spectrum_ClearRectangle(int x,
                                    int y,
                                    int width,
                                    int height)
{
    int row;

    for (row = 0; row < height; row++) {
        MXC_TFT_Line(x,
                     y + row,
                     x + width - 1,
                     y + row,
                     COLOR_BG);
    }
}

// 擦除上一帧的 Peak 标记和频率
void Spectrum_ClearMaxPeak(void)
{
    if (!old_peak_valid) {
        return;
    }

    Spectrum_ClearRectangle(old_label_x,
                            old_label_y,
                            PEAK_LABEL_W,
                            PEAK_LABEL_H);

    MXC_TFT_Line(old_peak_x - 2,
                 old_peak_y,
                 old_peak_x + 2,
                 old_peak_y,
                 COLOR_BG);

    MXC_TFT_Line(old_peak_x,
                 old_peak_y - 2,
                 old_peak_x,
                 old_peak_y + 2,
                 COLOR_BG);

    old_peak_valid = 0;
}

// 绘制最大 Peak
void Spectrum_DrawMaxPeak(int bin)
{
    char label[12];

    uint32_t frequency_hz;
    uint32_t integer_part;
    uint32_t decimal_part;

    int peak_x;
    int peak_y;
    int label_x;
    int label_y;

    frequency_hz = Spectrum_BinToHz(bin);

    if (frequency_hz >= 1000U) {
        integer_part = frequency_hz / 1000U;
        decimal_part = (frequency_hz % 1000U) / 100U;

        snprintf(label,
                 sizeof(label),
                 "%lu.%luk",
                 (unsigned long)integer_part,
                 (unsigned long)decimal_part);
    } else {
        snprintf(label,
                 sizeof(label),
                 "%luHz",
                 (unsigned long)frequency_hz);
    }

    peak_x = Spectrum_BinToX(bin);
    peak_y = spectrum_db_to_y(smooth_db[bin]);

    /* 默认把频率放在峰值右侧 */
    label_x = peak_x + 4;
    label_y = peak_y - PEAK_LABEL_H - 2;

    /* 右侧放不下就放到左侧 */
    if (label_x + PEAK_LABEL_W > PLOT_X1 - 1) {
        label_x = peak_x - PEAK_LABEL_W - 4;
    }

    /* 上方放不下就放到峰值下方 */
    if (label_y < PLOT_Y0 + 1) {
        label_y = peak_y + 4;
    }

    if (label_y + PEAK_LABEL_H > PLOT_Y1 - 1) {
        label_y = PLOT_Y1 - PEAK_LABEL_H - 1;
    }

    /* 峰顶十字 */
    MXC_TFT_Line(peak_x - 2,
                 peak_y,
                 peak_x + 2,
                 peak_y,
                 PEAK_COLOR);

    MXC_TFT_Line(peak_x,
                 peak_y - 2,
                 peak_x,
                 peak_y + 2,
                 PEAK_COLOR);

    /* 峰值频率 */
    MXC_TFT_SetForeGroundColor(PEAK_COLOR);

    TFT_Print(label,
              strlen(label),
              label_x,
              label_y,
              (int)&Liberation_Sans12x12[0]);

    old_peak_x = peak_x;
    old_peak_y = peak_y;
    old_label_x = label_x;
    old_label_y = label_y;
    old_peak_valid = 1;
}