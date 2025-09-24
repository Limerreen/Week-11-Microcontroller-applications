#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "driver/ledc.h"

#define LDR_CHANNEL ADC1_CHANNEL_7  // GPIO35 (ADC1_CH7)
#define LED_PIN     18              // GPIO18 สำหรับ LED
#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   64

static const char *TAG = "LDR_LED";
static esp_adc_cal_characteristics_t *adc_chars;

// ฟังก์ชันตั้งค่า PWM สำหรับ LED
static void ledc_init(void) {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT, // ความละเอียด 10 บิต (0-1023)
        .freq_hz = 5000,                      // ความถี่ 5 kHz
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = LED_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);
}

void app_main(void) {
    // ตั้งค่า ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_CHANNEL, ADC_ATTEN_DB_11);

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

    // ตั้งค่า PWM LED
    ledc_init();

    ESP_LOGI(TAG, "เริ่มการควบคุม LED ตามค่า LDR");

    while (1) {
        uint32_t adc_reading = 0;
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            adc_reading += adc1_get_raw((adc1_channel_t)LDR_CHANNEL);
        }
        adc_reading /= NO_OF_SAMPLES;

        // แปลงเป็นเปอร์เซ็นต์
        float lightLevel = (adc_reading / 4095.0) * 100.0;

        // ควบคุม LED ตามระดับแสง
        int duty = 0;  
        const char *status;

        if (lightLevel < 20) {
            status = "มืด";
            duty = 0; // LED ดับ
        } else if (lightLevel < 50) {
            status = "แสงน้อย";
            duty = 256; // สว่างน้อย (25%)
        } else if (lightLevel < 80) {
            status = "แสงปานกลาง";
            duty = 512; // สว่างกลาง (50%)
        } else {
            status = "แสงจ้า";
            duty = 1023; // สว่างสุด (100%)
        }

        // สั่งให้ LEDC ทำงาน
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);

        ESP_LOGI(TAG, "ADC: %d | แสง: %.1f%% | สถานะ: %s | Duty: %d", 
                 adc_reading, lightLevel, status, duty);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}