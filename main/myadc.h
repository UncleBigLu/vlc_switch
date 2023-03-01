#ifndef _MYADC_H
#define _MYADC_H

#include "esp_adc/adc_continuous.h"


#define ADC_READ_LEN                    1024
#define MAX_STORE_BUF                   2048
#define SAMPLE_FREQ                     400*1000
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_0
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH


void continuous_adc_init(adc_channel_t channel, adc_continuous_handle_t *out_handle);




#endif