#ifndef MQTT_S
#define MQTT_s

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h"

extern esp_mqtt_client_handle_t client;

//#define BROKER_IP "mqtt://192.168.100.11"
#define BROKER_IP "mqtt://82.180.173.228" //iot-uabc.site

#define TOPIC "device/led"
#define USER "mqtt"
#define PASSWORD "mqtt"

#define MESSAGE "0:BCR"

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void send_mqtt_message();

void mqtt_app_start(void);

#endif
