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

#include "macros.h"
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

//#define CONNECT_TO_TIME_SERVER
#define CONNECT_TO_IOT_SERVER
//#define AVTIVE_MQTT

#define STR_LEN 128

const char *TAG_MAIN = "MAIN";

TaskHandle_t get_button_handle = NULL;

//nvs
char ssid[STR_LEN] = "\0", pass[STR_LEN] = "\0", dev_name[STR_LEN] = "esp_default_name", user[STR_LEN] = "\0", dev_num[STR_LEN] = "\0";
uint8_t valid_f = 0;

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

    //mutexes
    ip_mutex = xSemaphoreCreateMutex();
    ap_mutex = xSemaphoreCreateMutex();

    //read button task
    xTaskCreate(get_button_task, "get_button_task", 4096, NULL, 4, NULL);

    //validate flash values
    valid_f = validate_read_nvs_values(ssid, pass, dev_name, user, dev_num);

    //make tcp client or udp server
    LOCK_MUTEX(ip_mutex)
        tcp_or_udp(ip_flag);
    UNLOCK_MUTEX(ip_mutex)

    //gpio R/W task
    xTaskCreate(update_gpio_value_task, "update_gpio_value_task", 4096, NULL, 5, NULL);

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));

        LOCK_MUTEX(ip_mutex)
        LOCK_MUTEX(ap_mutex)
            //if sudden disconnect and no ap active
            if(ip_flag==0xff && !active_ap) 
            {
                ESP_LOGE(TAG_MAIN, "Wifi disconnected. Creating access point...");
                activate_access_point();
            }
        UNLOCK_MUTEX(ap_mutex)
        UNLOCK_MUTEX(ip_mutex)
    }
    vSemaphoreDelete(ip_mutex);
    vSemaphoreDelete(ap_mutex);
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

    uint8_t local_ap_flag=0;
    while(1)
    {
        LOCK_MUTEX(ap_mutex)
            local_ap_flag = active_ap;
        UNLOCK_MUTEX(ap_mutex)
        if(local_ap_flag != 0)
            break;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
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
        
        uint8_t local_ip_flag=0;
        //wait for connect or disconect
        while(1)
        {
            LOCK_MUTEX(ip_mutex)
                local_ip_flag = ip_flag;
            UNLOCK_MUTEX(ip_mutex)
            if(local_ip_flag != 0)
                break;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if(ip_flag==0xff)
        {
            ESP_LOGE(TAG_MAIN, "Could not connect to wifi network. Creating access point...");
            activate_access_point();
        }
    }
}

void tcp_or_udp(uint8_t ip_flag)
{
    //if got IP
    if(ip_flag==1)
    {
        #ifdef CONNECT_TO_TIME_SERVER
            //tcp clock task
            xTaskCreate(clock_task, "clock_task", 4096, NULL, 5, NULL);

        #ifdef CONNECT_TO_SERVER
            params.str1 = user;
            params.str2 = dev_num;
            params.str3 = "\0";
            
            //printf("param str: %s", params.str1);
            xTaskCreate(tcp_server_task, "tcp_server_task", 8192, &params, 5, &tcp_server_handle);
        #endif

        #ifdef AVTIVE_MQTT
            mqtt_app_start();
        #endif
    }
    else
    {    
        ESP_LOGW(TAG_MAIN, "Creating UDP server...");
        xTaskCreate(udp_server_task, "udp_server_task", 4096, (void *)AF_INET, 5, &udp_server_handle);
    }
}
