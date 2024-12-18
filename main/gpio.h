#ifndef GPIO
#define GPIO

#include "esp_log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "adc.h"

#define LED GPIO_NUM_2
#define LED_PWM GPIO_NUM_15
#define LED_AP GPIO_NUM_18
#define BUTTON GPIO_NUM_23

extern SemaphoreHandle_t edge_mutex;

extern volatile uint8_t l_state, b_state, b_state_old, edge;

void init_gpio(void);

void set_led(uint8_t state);

void update_gpio_value_task(void *pvParameters);

void get_button_task(void *pvParameters);

#endif 