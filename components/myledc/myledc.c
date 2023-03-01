//
// Created by unclebiglu on 2/27/23.
//
#include "myledc.h"
#include "driver/ledc.h"

esp_err_t init_pwm() {
    esp_err_t ret;
    ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = LEDC_FREQ,
            .duty_resolution = 1,
            .clk_cfg = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ledc_timer);
    if(ret == ESP_OK) {
        ledc_channel_config_t ledc_channel = {
                .speed_mode = LEDC_HIGH_SPEED_MODE,
                .channel = LEDC_CHANNEL_0,
                .timer_sel = LEDC_TIMER_0,
                .intr_type = LEDC_INTR_DISABLE,
                .gpio_num = LEDC_OUTPUT_IO,
                .duty = 1,
                .hpoint = 0
        };
        ret = ledc_channel_config(&ledc_channel);
    }
    return ret;
}