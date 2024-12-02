#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "gpio.h"
#include "adc.h"
#include "wifi.h"
#include "udp_s.h"
#include "nvs_esp.h"
#include "ap.c"
#include "host_name.h"
#include "tcp_s.h"
#include "pwm.h"
#include "mqtt.h"

#define STR_LEN 128

const char *TAG_MAIN = "MAIN";

TaskHandle_t get_button_handle = NULL;

//nvs
char ssid[STR_LEN] = "\0", pass[STR_LEN] = "\0", dev_name[STR_LEN] = "esp_default_name", user[STR_LEN] = "\0", dev_num[STR_LEN] = "\0";
uint8_t valid_f = 0;

typedef struct 
{
    char *str1;
    char *str2;
    char *str3;
} task_params_t;

int validate_read_nvs_values(char *ssid, char *pass, char *dev_name, char *user, char *dev_num);

void activate_access_point()
{
    init_ap();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{

    init_gpio();
    adc_init();
    ledc_init();
    ESP_ERROR_CHECK(init_nvs());

    //button task
    xTaskCreate(get_button_task, "get_button_task", 4096, NULL, 4, NULL);

    //validate flash values
    valid_f = validate_read_nvs_values(ssid, pass, dev_name, user, dev_num);

    if(!valid_f)
        activate_access_point();
    else
    { 
        //wifi station
        wifi_connection(ssid, pass, dev_name);
        
        ESP_LOGE(TAG_MAIN, "WIFI waiting....");
        
        while(ip_flag == 0)
            vTaskDelay(pdMS_TO_TICKS(50));
    }

    //if got IP
    if(ip_flag)
    {
        //tcp clock task
        xTaskCreate(clock_task, "clock_task", 4096, NULL, 5, NULL);

        task_params_t params = {
            .str1 = user,
            .str2 = dev_num,
            .str3 = "\0",
        };
        //tcp iot server
        //xTaskCreate(tcp_server_task, "tcp_server_task", 4096, &params, 5, &tcp_server_handle);

        //mqtt_app_start();
    }
    else    
        xTaskCreate(udp_server_task, "udp_server_task", 4096, (void *)AF_INET, 5, &udp_server_handle);

    xTaskCreate(button_event_task, "button_event_task", 4096, NULL, 4, NULL);

    //set led and read adc
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
        set_led(l_state);
        read_adc_input(CHANNEL_0);

        //TODO: wifi led

        //if sudden disconect and no ap active
        //if(!connected_w && !active_ap) 
           // activate_access_point();
    }
}

int validate_read_nvs_values(char *ssid, char *pass, char *dev_name, char *user, char *dev_num)
{
    uint8_t valid_f = 0;

    //not needed for wifi station
    read_nvs((char *)key_dev_name, dev_name, STR_LEN); 
    read_nvs((char *)key_pass, pass, STR_LEN); //can be blank for no password

    if(!(read_nvs((char *)key_ssid, ssid, STR_LEN) ==  ESP_ERR_NVS_NOT_FOUND || 
        read_nvs((char *)key_user, user, STR_LEN) == ESP_ERR_NVS_NOT_FOUND ||
        read_nvs((char *)key_dev_num, dev_num, STR_LEN) == ESP_ERR_NVS_NOT_FOUND || 
        strcmp(ssid, "RESET") == 0))
        valid_f = 1;

    return valid_f;
}