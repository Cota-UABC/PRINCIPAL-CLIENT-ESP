#include "adc.h"

adc_oneshot_unit_handle_t adc1_handle;

volatile uint16_t adc_value = 0;

void adc_init()
{
    adc_oneshot_unit_init_cfg_t init_config1 = 
    {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12, 
        .atten = ADC_ATTEN_DB_0,
    };

    //channel 0, pin 14
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, CHANNEL_0, &config)); 
}

void read_adc_input(adc_channel_t channel)
{
    uint16_t adc_raw;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, CHANNEL_0, &adc_raw));//returns n_bits number

    adc_value = adc_raw; 
}