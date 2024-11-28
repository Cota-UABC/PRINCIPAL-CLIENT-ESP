#include "host_name.h"

#include <string.h>

#include "esp_netif.h"
#include "esp_log.h"

const char *TAG_H = "Host_name";

esp_netif_t *netif;

void set_device_hostname(char *name) {
    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(netif == NULL) 
    {
        ESP_LOGE(TAG_H, "Error en manejador de interfaz");
        return;
    }

    esp_err_t ret = esp_netif_set_hostname(netif, (const char *)name);
    if(ret == ESP_OK)
        ESP_LOGI(TAG_H, "Nuevo nombre: %s", name);
    else
        ESP_LOGE(TAG_H, "Error: %s", esp_err_to_name(ret));
}

void get_device_hostname(char *name) {
    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(netif == NULL) 
    {
        ESP_LOGE(TAG_H, "Error en manejador de interfaz");
        return;
    }

    const char *hostname = NULL;
    esp_err_t ret = esp_netif_get_hostname(netif, &hostname);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG_H, "Nombre: %s", hostname);
        strcpy(name, hostname);
    }
    else
        ESP_LOGE(TAG_H, "Error: %s", esp_err_to_name(ret));
}
