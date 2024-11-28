#ifndef PWM_S
#define PWM_S

#include <stdio.h>

#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define TAG_P "PWM"

#define LED_GPIO GPIO_NUM_15
#define MAX_DUTY_RESOLUTION 1023

void ledc_init(void);

#endif
