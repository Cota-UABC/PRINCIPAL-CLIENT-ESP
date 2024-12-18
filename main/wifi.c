#include "wifi.h"

const char *TAG_W = "WIFI";

static int retry_count = 0;

esp_netif_ip_info_t ip_info;

char ip_addr[IP_LENGHT] = "\0";

SemaphoreHandle_t ip_mutex = 0;
uint8_t ip_flag=0;

//for fast testing
#define MANUAL_SSID_PASS

#define SSID "Totalplay-2.4G-b518"
#define PASS "Qxm2EAzh99Ce7Lfk"

//#define SSID "Totalplay-CF3C"
//#define PASS "C8D6CF3C"

//#define SSID "LuisEnrique"
//#define PASS "luis12345"

//#define SSID "cota_mobil"
//#define PASS "fruit123"

//#define SSID "IoT_AP"
//#define PASS "12345678"

void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG_W, "WiFi connecting WIFI_EVENT_STA_START ...");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG_W,"WiFi connected WIFI_EVENT_STA_CONNECTED ...");
        retry_count = 0;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        LOCK_MUTEX(ip_mutex)
            ip_flag=0;
        UNLOCK_MUTEX(ip_mutex)
        if(retry_count < MAX_RETRY)
        {
            ESP_LOGE(TAG_W, "WiFi lost connection WIFI_EVENT_STA_DISCONNECTED. Retrying ... Attempt %d/%d", retry_count + 1, MAX_RETRY);
            esp_wifi_connect();
            retry_count++;
        }
        else
        {
            esp_wifi_stop();
            esp_wifi_deinit();
            ESP_LOGE(TAG_W,"WiFi stopped.");
            LOCK_MUTEX(ip_mutex)
                ip_flag=0xff;
            UNLOCK_MUTEX(ip_mutex)
        }
        break;
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG_W,"WiFi got IP");
        retry_count=0;

        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_get_ip_info(netif, &ip_info);
        snprintf(ip_addr, IP_LENGHT, "%d.%d.%d.%d", IP2STR(&ip_info.ip));
        LOCK_MUTEX(ip_mutex)
            ip_flag = 1;
        UNLOCK_MUTEX(ip_mutex)
        break;
    }
}

void wifi_connection(char *ssid, char *pass, char *dev_name)
{
    LOCK_MUTEX(ip_mutex)
        ip_flag = 0;
    UNLOCK_MUTEX(ip_mutex)

    esp_err_t error = nvs_flash_init();
    if(error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    set_device_hostname(dev_name);

    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
#ifdef MANUAL_SSID_PASS
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = SSID,
            .password = PASS}};
#else
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "\0",
            .password = "\0"}};

    strncpy((char*)wifi_configuration.sta.ssid, ssid, sizeof(wifi_configuration.sta.ssid) - 1);
    strncpy((char*)wifi_configuration.sta.password, pass, sizeof(wifi_configuration.sta.password) - 1);
#endif
    
    ESP_LOGI(TAG_W, "Wifi SSID: %s", (char*)wifi_configuration.sta.ssid);
    ESP_LOGI(TAG_W, "Wifi PASS: %s", (char*)wifi_configuration.sta.password);
    
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    //esp_wifi_connect();
}