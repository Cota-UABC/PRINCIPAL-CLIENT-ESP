#ifndef TCP_S
#define TCP_S

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include "nvs_flash.h"
#include "ping/ping_sock.h"

#include "freertos/timers.h"

#include "esp_timer.h"

#include "macros.h"
#include "gpio.h"
#include "adc.h"
#include "nvs_esp.h"
#include "pwm.h"
#include "mqtt.h"

#define TCP_SERVER_MAX_RETRY 0

//communication type
#define NORMAL_COM
//#define DECODE_COM

#define TIMER_RESET 7000 //miliseconds

#define STR_LEN 128
#define COMMAND_QUANTITY 10

#ifdef NORMAL_COM
#define PORT_IOT_UABC 8876
//#define PORT_IOT_UABC 8877
#endif
#ifdef DECODE_COM
#define PORT_IOT_UABC 8277 //codificado
#endif

#define HOST_IOT_UABC "82.180.173.228" //iot-uabc.site
//#define HOST_IOT_UABC "192.168.1.200" //iot server local

#define PORT_LOCAL 8266
#define HOST_LOCAL "192.168.100.14" //esp local server

#define PORT_TIME 80
#define HOST_TIME "213.188.196.246" //worldtime api 

#define PORT_GOOGLE 80
#define HOST_GOOGLE "142.250.188.14" //Google

//unsused
//#define SEND_MESSAGE "UABC:BCR:X:X:test"
//#define SEND_MESSAGE "UABC:BCR:M:S:6644871544:Hola_desde_esp"

#define SERVER_ID "UABC"

//command parts
#define ID_T 0
#define USER_T 1
#define DEV_NUM_T 2
#define OPERATION_T 3
#define ELEMENT_T 4
#define VALUE_T 5
#define COMMENT_T 6

#define TIME_STRING_LEN 16

#define ACK_LEN 3

extern int sock;

extern TimerHandle_t timer1;
extern const int timerId;

extern TaskHandle_t tcp_server_handle;
extern TaskHandle_t button_handle;

//extern char user_tcp[STR_LEN];

extern char host[30], *ptr, pasword_iot[50], pasword_iot_desif[50],time_api_message[STR_LEN];
extern uint16_t command[COMMAND_QUANTITY];
extern uint32_t tx_buffer, rx_buffer, login, keep_alive;

extern uint8_t id_pck;
extern char id_pck_str[STR_LEN];
#define DUMMY_TEXT "1"

extern SemaphoreHandle_t tx_buffer_mutex, id_pck_mutex;

extern uint8_t id_pck;
extern char id_pck_str[STR_LEN];

extern char user_tcp[STR_LEN];
extern char dev_num_tcp[STR_LEN];

//---BUTTON---
extern uint8_t send_f;
extern uint64_t button_press_time, current_time, diff_time;

extern volatile uint8_t send_ack_f;

//---CLOCK---
//#define PRINT_TIME
#define CHECK_TIME_OFFSET
#define CLOCK_MIN_TO_CHECK 5

extern volatile uint16_t hours_true, minutes_true, seconds_true, hours, minutes, seconds;

extern SemaphoreHandle_t real_time_key, check_time_key;

//---ACKNOWLEGE---
#define MAX_NO_ACK 3

extern uint16_t counter, counter_no_ack;


void clock_task(void *pvParameters);

void check_time_offset_task(void *pvParameters);

void vTimerCallback(TimerHandle_t pxTimer);

void setTimer();

void tcp_server_task(void *pvParameters);

void connect_to_server(void *pvParameters, char *host_parameter, int port);

void tcp_real_time_task(void *pvParameters);

void connect_to_host_time(char *host_parameter, int port);

void check_time_offset_task(void *pvParameters);

//check connection to google
uint8_t check_internet_connection();

//tcp exclusive button events
void button_tcp_event_task(void *pvParameters);

void handle_read_tcp(char command[][128], char *tx_buffer);

void handle_write_tcp(char command[][128], char *tx_buffer);

void message_to_buffer(uint32_t *buffer, uint32_t message);

void nack_message_tcp(uint32_t *buffer);

void aplicarXor(char *original_message, char *mensaje_cifrado);

void codeMessage(char *message, char *password);

void build_command(char *string_com, ...);

#endif
