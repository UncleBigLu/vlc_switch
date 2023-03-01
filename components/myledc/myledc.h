//
// Created by unclebiglu on 2/27/23.
//

#ifndef ETHERNET_BASIC_MYLEDC_H
#define ETHERNET_BASIC_MYLEDC_H

#include "esp_err.h"

#define LEDC_FREQ (100000) // 100kHz
#define LEDC_OUTPUT_IO (33)

esp_err_t init_pwm();

#endif //ETHERNET_BASIC_MYLEDC_H
