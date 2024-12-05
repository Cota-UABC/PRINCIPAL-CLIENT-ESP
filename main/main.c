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

#define CONNECT_TO_SERVER

#define STR_LEN 128

const char *TAG_MAIN = "MAIN";

TaskHandle_t get_button_handle = NULL;

//nvs
char ssid[STR_LEN] = "\0", pass[STR_LEN] = "\0", dev_name[STR_LEN] = "esp_default_name";
uint8_t valid_f = 0, user, dev_num;

typedef struct 
{
    uint8_t user;
    uint8_t dev;
    uint8_t dummy;
} task_params_t;

task_params_t params;

int validate_read_nvs_values(char *ssid, char *pass, char *dev_name, uint8_t *user, uint8_t *dev_num);

void activate_access_point();

void ap_or_station_mode(uint8_t valid_f);

void tcp_or_udp(uint8_t ip_flag);


void app_main(void)
{

    init_gpio();
    adc_init();
    ledc_init();
    ESP_ERROR_CHECK(init_nvs());

    //read button task
    xTaskCreate(get_button_task, "get_button_task", 4096, NULL, 4, NULL);

    //validate flash values
    valid_f = validate_read_nvs_values(ssid, pass, dev_name, &user, &dev_num);

    ap_or_station_mode(valid_f);
    tcp_or_udp(ip_flag);

    //button event task
    xTaskCreate(button_event_task, "button_event_task", 4096, NULL, 4, NULL);

    //gpio R/W task
    xTaskCreate(update_gpio_value_task, "update_gpio_value_task", 4096, NULL, 5, NULL);

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));

        //if sudden disconnect and no ap active
        if(!connected_w && !active_ap) 
        {
            ESP_LOGE(TAG_MAIN, "Wifi disconnected. Creating access point...");
            activate_access_point();
        }
    }
}

int validate_read_nvs_values(char *ssid, char *pass, char *dev_name, uint8_t *user, uint8_t *dev_num)
{
    uint8_t valid_f = 0;

    //not needed for wifi station
    read_nvs((char *)key_dev_name, dev_name, STR_LEN); 
    read_nvs((char *)key_pass, pass, STR_LEN); //can be blank for no password

    if(!(read_nvs((char *)key_ssid, ssid, STR_LEN) ==  ESP_ERR_NVS_NOT_FOUND || 
        read_nvs_uint8((char *)key_user, user) == ESP_ERR_NVS_NOT_FOUND ||
        read_nvs_uint8((char *)key_dev_num, dev_num) == ESP_ERR_NVS_NOT_FOUND || 
        strcmp(ssid, "RESET") == 0))
        valid_f = 1;

    return valid_f;
}

void activate_access_point()
{
    init_ap();
    while(!active_ap)
        vTaskDelay(pdMS_TO_TICKS(30));
}

void ap_or_station_mode(uint8_t valid_f)
{
    if(!valid_f)
    {
        ESP_LOGW(TAG_MAIN, "Could not validate NVS values. Creating access point...");
        activate_access_point();
    }
    else
    { 
        ESP_LOGI(TAG_MAIN, "NVS values validated.");
        //wifi station
        wifi_connection(ssid, pass, dev_name);
        
        ESP_LOGW(TAG_MAIN, "WIFI waiting....");
        
        while(ip_flag == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            if(connected_w==0xff)
            {
                ESP_LOGE(TAG_MAIN, "Could not connect to wifi network. Creating access point...");
                activate_access_point();
                break;
            }
        }
    }
}

void tcp_or_udp(uint8_t ip_flag)
{
     //if got IP
    if(ip_flag)
    {
        //tcp clock task
        xTaskCreate(clock_task, "clock_task", 4096, NULL, 5, NULL);

        #ifdef CONNECT_TO_SERVER
            params.user = user;
            params.dev = dev_num;
            params.dummy = 0;

            //printf("param str: %u", params.user);
            xTaskCreate(tcp_server_task, "tcp_server_task", 16384, &params, 5, &tcp_server_handle);
        #endif

        //mqtt_app_start();
    }
    else
    {    
        ESP_LOGW(TAG_MAIN, "Creating UDP server...");
        xTaskCreate(udp_server_task, "udp_server_task", 4096, (void *)AF_INET, 5, &udp_server_handle);
    }
}
