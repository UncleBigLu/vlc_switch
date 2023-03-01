#include "myadc.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

static const char * TAG = "myadc.c";

void continuous_adc_init(adc_channel_t channel, adc_continuous_handle_t *out_handle)
{
    /** Resource Allocation **/
    adc_continuous_handle_t handle = NULL;
    adc_continuous_handle_cfg_t adc_config = {
            // Buffer size for ADC driver to store ADC conversation result
            .max_store_buf_size = MAX_STORE_BUF,
            // Size of ADC conversation frame, in bytes
            .conv_frame_size = ADC_READ_LEN,
    };
    /** Initialize ADC continuous mode driver **/
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));
    /** ADC Configurations **/
    adc_continuous_config_t dig_cfg = {
            .sample_freq_hz = SAMPLE_FREQ,
            .conv_mode = EXAMPLE_ADC_CONV_MODE,
            .format = EXAMPLE_ADC_OUTPUT_TYPE,
            // Number of ADC channels that will be used
            .pattern_num = 1,
    };
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    adc_pattern[0].atten = EXAMPLE_ADC_ATTEN;
    adc_pattern[0].channel = channel & 0x7;
    adc_pattern[0].unit = EXAMPLE_ADC_UNIT;
    adc_pattern[0].bit_width = EXAMPLE_ADC_BIT_WIDTH;

    ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, 0, adc_pattern[0].atten);
    ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, 0, adc_pattern[0].channel);
    ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, 0, adc_pattern[0].unit);

    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}
