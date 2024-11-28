#include "gpio.h"

static const char *TAG_G = "GPIO";

volatile uint8_t l_state = 0, b_state = 0, b_state_old = 0, edge = 0;

void init_gpio(void)
{
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_level(LED, 0);

    gpio_set_direction(LED_PWM, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PWM, 0);

    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
    gpio_pulldown_en(BUTTON);
}


void set_led(uint8_t state)
{
    if(state)
    {
        gpio_set_level(LED, 1);
        l_state = 1;
    }
    else
    {
        gpio_set_level(LED, 0);
        l_state = 0;
    }
}

/*
void get_button(void)
{
    b_state = gpio_get_level(BUTTON);
}
*/

void get_button_task(void *pvParameters)
{
    while(1)
    {
        b_state = gpio_get_level(BUTTON);
        if( b_state_old == 0 && b_state == 1)
        {
            edge = 1;
            vTaskDelay(pdMS_TO_TICKS(20)); //delay antirrebote
            ESP_LOGI(TAG_G, "Button pressed");
        }
        else if( b_state_old == 1 && b_state == 0)
            edge = 0;

        b_state_old = b_state;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
