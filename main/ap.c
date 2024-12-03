#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "host_name.h"
#include "nvs_esp.h"
#include "gpio.h"

#define WIFI_SSID      "local_AP_ESP"
#define WIFI_PASS      "12345678"
#define MAX_STA_CONN    5

static const char *TAG_AP = "softAP";
uint8_t volatile active_ap = 0;

static void wifi_e_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG_AP, "estacion %X:%X:%X:%X:%X:%X se unio, AID=%d", event->mac[5], event->mac[4],event->mac[3],event->mac[2],event->mac[1],event->mac[0], event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG_AP, "estacion %X:%X:%X:%X:%X:%X se desconecto, AID=%d", event->mac[5], event->mac[4],event->mac[3],event->mac[2],event->mac[1],event->mac[0], event->aid);
    }
}

esp_err_t wifi_init_softap()
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_e_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_AP, "Inicializacion de softAP terminada. SSID: %s password: %s", WIFI_SSID, WIFI_PASS);
    active_ap++;
    return ESP_OK;
}

void init_ap()
{
    active_ap = 0;

    /*esp_err_t error = nvs_flash_init();
    if(error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        nvs_flash_erase();
        nvs_flash_init();
    }*/
    
    //activate led indicator
    gpio_set_level(LED_AP, 1);

    esp_netif_init();
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG_AP, "Event loop already created");
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_LOGI(TAG_AP, "Access Point initiated");
    ESP_ERROR_CHECK(wifi_init_softap());
}