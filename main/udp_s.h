#ifndef UDP_S
#define UDP_S

/*
USAGE:

Port: 4080

Example: UABC:W:L:1:example_commd

Commands:
UABC:
    R:
        L
        A
        WIFI
        PASS
        DEV
        USER:
        DEV_NUM
        IP
        P
    W:
        L:
            1
            0
        WIFI
        PASS
        DEV
        USER
        DEV_NUN
        P
*/

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"

#include <string.h>
#include <inttypes.h>

#include "gpio.h"
#include "adc.h"
#include "nvs_esp.h"
#include "host_name.h"
#include "pwm.h"

#define PORT 4080

//string
#define STRING_LENGHT 128

#define COMMAND_NUM_U 5

//command parts
#define ID 0
#define OPERATION 1
#define ELEMENT 2
#define VALUE 3
#define COMMENT 4

extern TaskHandle_t udp_server_handle;

void handleRead(char command[][128], char *tx_buffer);

void handleWrite(char command[][128], char *tx_buffer);

void nackMessage(char *str);

void udp_server_task(void *pvParameters);

#endif