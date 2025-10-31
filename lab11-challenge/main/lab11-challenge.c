#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define LDR_CHANNEL ADC1_CHANNEL_7   // GPIO35 (ADC1_CH7)
#define BUZZER_PIN  18               // GPIO18 ต่อ Buzzer
#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   64

static const char *TAG = "LDR_BUZZER";
static esp_adc_cal_characteristics_t *adc_chars;

static void ledc_init(void) {
    // ตั้งค่า Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT, // 0-1023
        .freq_hz          = 1000,              // ค่า default
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // ตั้งค่า Channel
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = BUZZER_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);
}

void app_main(void) {
    // ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_CHANNEL, ADC_ATTEN_DB_11);
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

    // ตั้งค่า PWM (Buzzer)
    ledc_init();

    ESP_LOGI(TAG, "เริ่มระบบวัดแสง + เสียง Buzzer");

    while (1) {
        uint32_t adc_reading = 0;
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            adc_reading += adc1_get_raw((adc1_channel_t)LDR_CHANNEL);
        }
        adc_reading /= NO_OF_SAMPLES;

        float lightLevel = (adc_reading / 4095.0) * 100.0;
        int freq = 0;
        const char *status;

        if (lightLevel < 20) {
            status = "มืด";
            freq = 0; // ปิดเสียง
        } else if (lightLevel < 50) {
            status = "แสงน้อย";
            freq = 500; // เสียงต่ำ
        } else if (lightLevel < 80) {
            status = "แสงปานกลาง";
            freq = 1000; // เสียงกลาง
        } else {
            status = "แสงจ้า";
            freq = 2000; // เสียงสูง
        }

        if (freq == 0) {
            // ปิดเสียง
            ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
        } else {
            // เปลี่ยนความถี่ตามระดับแสง
            ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, freq);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 512); // 50% duty
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
        }

        ESP_LOGI(TAG, "ADC: %d | แสง: %.1f%% | สถานะ: %s | Freq: %d Hz",
                 adc_reading, lightLevel, status, freq);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
