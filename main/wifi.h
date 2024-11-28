#ifndef WIFI
#define WIFI

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_system.h"

#include <string.h>

#include "nvs_flash.h"
#include "ping/ping_sock.h"

#include "host_name.h"
#include "nvs_esp.h"

#define MAX_RETRY 3
#define IP_LENGHT 50

extern const char *TAG_W;
extern uint8_t connected_w;

extern esp_netif_ip_info_t ip_info;

extern uint8_t ip_flag;

extern char ip_addr[IP_LENGHT];

void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void wifi_connection(char *ssid, char *pass, char *dev_name);

#endif 