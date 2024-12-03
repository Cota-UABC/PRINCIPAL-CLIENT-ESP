#include "gpio.h"

static const char *TAG_G = "GPIO";

SemaphoreHandle_t edge_mutex = 0;

volatile uint8_t l_state = 0, b_state = 0, b_state_old = 0, edge = 0;

void init_gpio(void)
{
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_level(LED, 0);

    gpio_set_direction(LED_PWM, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PWM, 0);

    gpio_set_direction(LED_AP, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_AP, 0);

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

void update_gpio_value_task(void *pvParameters)
{
    while(1)
    {
        set_led(l_state);
        read_adc_input(CHANNEL_0);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}

void get_button_task(void *pvParameters)
{
    ESP_LOGI(TAG_G, "Starting get button task...");
    edge_mutex = xSemaphoreCreateMutex();

    while(1)
    {
        b_state = gpio_get_level(BUTTON);
        if( b_state_old == 0 && b_state == 1)
        {
            if(xSemaphoreTake(edge_mutex, portMAX_DELAY))
            {
                edge = 1;
                xSemaphoreGive(edge_mutex);
            }
            ESP_LOGI(TAG_G, "Button pressed");
            vTaskDelay(pdMS_TO_TICKS(20)); //delay antirrebote
        }
        else if( b_state_old == 1 && b_state == 0){
            if(xSemaphoreTake(edge_mutex, portMAX_DELAY))
            {
                edge = 0;
                xSemaphoreGive(edge_mutex);
            }
        }

        b_state_old = b_state;

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vSemaphoreDelete(edge_mutex);
    vTaskDelete(NULL);
}
