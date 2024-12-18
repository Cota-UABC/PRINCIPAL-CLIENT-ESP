#include "tcp_s.h"

uint8_t bit_lengths[] = {VALUE_BIT_LEN, ELEMENT_BIT_LEN, OPERATION_BIT_LEN, DEV_BIT_LEN, USER_BIT_LEN, ID_BIT_LEN};
size_t num_elements = sizeof(bit_lengths) / sizeof(bit_lengths[0]);

int sock, sock2, sock3;

TimerHandle_t timer1;
const int timerId = 1;

static const char *TAG_T = "TCP";
static const char *TAG_T_CLOCK = "TCP_CLOCK";

TaskHandle_t tcp_server_handle = NULL;
TaskHandle_t button_handle = NULL;


//---server---
char host[30], *ptr, pasword_iot[50], pasword_iot_desif[50], time_api_message[STR_LEN];
uint16_t command[COMMAND_QUANTITY];
uint32_t tx_buffer, rx_buffer, login, keep_alive;

SemaphoreHandle_t tx_buffer_mutex = 0, id_pck_mutex = 0;

uint8_t id_pck = 0;
char id_pck_str[STR_LEN];

char user_tcp[STR_LEN];
char dev_num_tcp[STR_LEN];

//---BUTTON---
uint8_t send_f = 0;
uint64_t button_press_time=0, current_time, diff_time;

volatile uint8_t send_ack_f = 0;

//---CLOCK---
volatile uint16_t hours_true=0, minutes_true=0, seconds_true=0xffff, hours=0, minutes=0, seconds=0;

SemaphoreHandle_t real_time_key = 0, check_time_key = 0;

//---ACKNOWLEGE---
uint16_t counter, counter_no_ack=0;

typedef struct {
    uint8_t user;
    uint8_t dev;
    uint8_t dummy;
} task_params_t;

//patch
void build_command(char *string_com, ...);

void clock_task(void *pvParameters)
{
    real_time_key = xSemaphoreCreateBinary();

    xTaskCreate(tcp_time_task, "tcp_time_task", 4096, NULL, 5, NULL);
    xSemaphoreGive(real_time_key);

    #ifdef CHECK_TIME_OFFSET
    xTaskCreate(check_time_offset_task, "check_time_offset_task", 4096, NULL, 5, NULL);
    #endif

    //wait to get real time
    while(seconds_true>60)
        vTaskDelay(pdMS_TO_TICKS(100));
    
    if(hours_true>24)
    {
        ESP_LOGE(TAG_T_CLOCK, "Could not get real time. Set to 00:00:00");
        hours = 0;
        minutes = 0;
        seconds = 0;
    }
    else
    {
        ESP_LOGI(TAG_T_CLOCK, "Succesfully got real time.");
        hours = hours_true;
        minutes = minutes_true;
        seconds = seconds_true;
    }
    seconds_true = 0xffff;

    uint16_t internal_timer = 0;

    ESP_LOGI(TAG_T_CLOCK, "Starting local clock...");
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        seconds++;
        internal_timer++;
        if(seconds == 60)
        {
            seconds=0;
            minutes++;
            if(minutes == 60) 
            {
                minutes=0;
                hours++;
                if(hours == 24)
                    hours=0;
            }
        }
        #ifdef PRINT_TIME
        ESP_LOGI(TAG_T_CLOCK, "ESP local time: %d:%d:%d", hours, minutes, seconds);
        #endif

        //check offset every certain minutes
        #ifdef CHECK_TIME_OFFSET
        if(!(internal_timer % (60*CLOCK_MIN_TO_CHECK)))
        {
            xSemaphoreGive(real_time_key);
            xSemaphoreGive(check_time_key);
        }

        #endif
    }
    vSemaphoreDelete(real_time_key);
    ESP_LOGW(TAG_T, "Clock task closed.");
    vTaskDelete(NULL);
}

void check_time_offset_task(void *pvParameters)
{
    ESP_LOGI(TAG_T_CLOCK, "Starting time offset task...");
    check_time_key = xSemaphoreCreateBinary();

    while(1)
    {
        if(xSemaphoreTake(check_time_key, portMAX_DELAY))
        {
            ESP_LOGW(TAG_T_CLOCK, "Waiting to get real time...");
            while(seconds_true>60)
                vTaskDelay(pdMS_TO_TICKS(30));
            if(hours_true<24)
            {
                int dif_h = hours - hours_true;
                int dif_m = minutes - minutes_true;
                int dif_s = seconds - seconds_true;
                ESP_LOGW(TAG_T_CLOCK, "Offset by %d:%d:%d", dif_h,dif_m,dif_s);
            }
            else
                ESP_LOGE(TAG_T_CLOCK, "Could not get real time to check offset.");
            seconds_true = 0xffff;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vSemaphoreDelete(check_time_key);
}

void vTimerCallback(TimerHandle_t pxTimer)
{
	ESP_LOGI(TAG_T, "Timer called");

    LOCK_MUTEX(tx_buffer_mutex)
        sprintf(tx_buffer, keep_alive);
    UNLOCK_MUTEX(tx_buffer_mutex)
    #ifdef DECODE_COM
        LOCK_MUTEX(tx_buffer_mutex)
        codeMessage(tx_buffer, pasword_iot_desif);
        UNLOCK_MUTEX(tx_buffer_mutex)
    #endif
    send(sock, tx_buffer, strlen(tx_buffer), 0);
    ESP_LOGI(TAG_T, "Message send to %s: %s", host, tx_buffer);

	send_ack_f=1;
    
/*
    int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, pdMS_TO_TICKS(500));
    if(len > 0)
    {
        rx_buffer[len] = '\0';
        if(strncmp(rx_buffer, "ACK", 3) == 0) //ACK from keep alive
            ESP_LOGI(TAG_T, "Received acknowledge in timer callback");
        else
        {
            ESP_LOGE(TAG_T, "No acknowledge in timer callback: %s", rx_buffer);
            counter_no_ack++;
        }
    }
    else
    {
        ESP_LOGE(TAG_T, "No message received in timer callback");
        counter_no_ack++;
    }
    */
}

void setTimer()
{
	ESP_LOGI(TAG_T, "Timer initialized");
	timer1 = xTimerCreate("timer",
                        (pdMS_TO_TICKS(TIMER_RESET)),
                        pdTRUE,
                        (void *)timerId,
                        vTimerCallback);

	if(timer1 == NULL)
		ESP_LOGE(TAG_T, "Timer not created");
	else
	{
		if( xTimerStart(timer1, 0) != pdPASS)
			ESP_LOGI(TAG_T, "Timer could not be set to Active state");
	}
}

void button_event_task(void *pvParameters) //evento de boton
{
    ESP_LOGI(TAG_T, "Starting button event task...");
    while(1)
    {
       /*
        //send sms unused or  deprecated
        current_time = esp_timer_get_time();
        current_time =+ 60000000; 
        diff_time = current_time - button_press_time;
        if(!send_f && edge && diff_time >= 60000000)
        {
            send_f++;
            sprintf(tx_buffer, SEND_MESSAGE);
            #ifdef DECODE_COM
                codeMessage(tx_buffer, pasword_iot_desif);
            #endif
            //SMS message
            //send(sock, tx_buffer, strlen(tx_buffer), 0);
            //ESP_LOGI(TAG_T, "Message send to %s: %s", HOST_TCP, SEND_MESSAGE);

            //Mqtt message
            //send_mqtt_message();

            button_press_time = esp_timer_get_time();
        }
        else if(!edge)
            send_f = 0;
       */
        LOCK_MUTEX(edge_mutex)
        //if button pressed
        if(edge)
        {
            ESP_LOGI(TAG_T, "ESP local time: %d:%d:%d", hours, minutes, seconds);
            edge = 0;
        }
        UNLOCK_MUTEX(edge_mutex)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void tcp_server_task(void *pvParameters)
{
    ESP_LOGI(TAG_T, "Starting tcp server task...");

    tx_buffer_mutex = xSemaphoreCreateMutex();
    
    uint8_t local_counter = 0;

    while(1)
    {
        ESP_LOGI(TAG_T, "Conecting to iot tcp server...");
        connect_to_server(pvParameters, HOST_IOT_UABC, PORT_IOT_UABC);

        if(local_counter >= TCP_SERVER_MAX_RETRY)
            break;

        if(!check_internet_connection())
        {
            ESP_LOGE(TAG_T, "Could not validate internet connection.");
            break;
        }
        
        ESP_LOGW(TAG_T, "Retrying connection to IOT server... Attempt %d/%d", local_counter+1, TCP_SERVER_MAX_RETRY);  
        local_counter++;

        vTaskDelay(pdMS_TO_TICKS(4000));
    }

    ESP_LOGW(TAG_T, "Conecting to local tcp server...");
    connect_to_server(pvParameters, HOST_LOCAL, PORT_LOCAL);
    
    ESP_LOGE(TAG_T, "Could not connect to local tcp server.");
    ESP_LOGE(TAG_T, "TCP task closed.");
    vTaskDelete(NULL);
}

void connect_to_server(void *pvParameters, char *host_parameter, int port)
{
    sprintf(host, host_parameter);

    struct sockaddr_in dest_addr;
    int counter_cmmd, counter_word;
    dest_addr.sin_addr.s_addr = inet_addr(host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    while(1)
    {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG_T, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_T, "Socket created, connecting to %s:%d", host, port);

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG_T, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_T, "Successfully connected to: %s", host);

        //get struct from parameter
        task_params_t *params = (task_params_t *)pvParameters;
        char *user_tcp = params->str1;
        char *dev_num = params->str2;

        ESP_LOGI(TAG_T, "User: %s", user_tcp);
        ESP_LOGI(TAG_T, "Device number: %s", dev_num);

        
        build_command(login, SERVER_ID, user_tcp, dev_num, "L", "S", "login_server", "\0");
        sprintf(tx_buffer, login);

        send(sock, tx_buffer, strlen(tx_buffer), 0);  
        ESP_LOGI(TAG_T, "Message send to %s: %s", host, tx_buffer);

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if(len < 0) 
        {
            ESP_LOGE(TAG_T, "Error occurred during receiving: errno %d", errno);
            break;
        }

        rx_buffer[len] = '\0'; // terminator
        ESP_LOGI(TAG_T, "Received %d bytes", len);
        ESP_LOGI(TAG_T, "%s", rx_buffer);

        if(strncmp(rx_buffer, "ACK", 3) != 0) 
        {
            ESP_LOGE(TAG_T, "Login failed");
            break;
        }
        //login success
        #ifdef DECODE_COM
            //get pass
            strcpy(pasword_iot, "");
            if( strlen(rx_buffer) >= 4 )
            {
                ptr = &rx_buffer[4];
                counter = 0;
                while(*ptr != '\0' && *ptr != '\r')
                {
                    pasword_iot[counter] = *ptr;
                    ptr++;
                    counter++;
                    //printf("Char: %x\n", *ptr);
                }
                pasword_iot[counter] = '\0';

                //cifrado
                aplicarXor(pasword_iot, pasword_iot_desif);

                ESP_LOGI(TAG_T, "contrasena recibida: %s", pasword_iot);
                ESP_LOGI(TAG_T, "contrasena desifrada: %s", pasword_iot_desif);
            }
        #endif 
        //first keep alive
        build_command(keep_alive, SERVER_ID, user_tcp, dev_num, "K", "S", "keep_alive", "\0");
        sprintf(tx_buffer, keep_alive);
        #ifdef DECODE_COM
            LOCK_MUTEX(tx_buffer_mutex)
            codeMessage(tx_buffer, pasword_iot_desif);
            UNLOCK_MUTEX(tx_buffer_mutex)
        #endif
        send(sock, tx_buffer, strlen(tx_buffer), 0);
        ESP_LOGI(TAG_T, "Message send to %s: %s", host, tx_buffer);

        setTimer();

        //deprecated
        //xTaskCreate(button_event_task, "button_event_task", 4096, NULL, 4, &button_handle);

        counter_no_ack = 0;

        // Capturar comandos  
        while(1) 
        {
            if(counter_no_ack >= MAX_NO_ACK)
            {
                ESP_LOGE(TAG_T, "No keep alive response %d times from host: %s", MAX_NO_ACK, host);
                break;
            }

            if(send_ack_f) //patch for ack check
                send_ack_f++;

            strcpy(rx_buffer, "\0");
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if(len > 0)
            {
                rx_buffer[len] = '\0'; 
                #ifdef DECODE_COM
                    //decode
                    codeMessage(rx_buffer, pasword_iot_desif);
                #endif

                ESP_LOGI(TAG_T, "Received %d bytes", len);
                ESP_LOGI(TAG_T, "%s", rx_buffer);

                if(strncmp(rx_buffer, "ACK", 3) == 0) //ACK from keep alive
                {
                    ESP_LOGI(TAG_T, "Received acknowledge for keep alive");
                    send_ack_f = 0;
                    continue;
                }
                if(strncmp(rx_buffer, "NACK", 4) == 0)
                {
                    //ESP_LOGE(TAG_T, "Conection lost, restarting...");
                    //break;
                    ESP_LOGE(TAG_T, "Received NACK");
                    continue;
                }

                ptr = rx_buffer;
                counter_cmmd = 0;
                counter_word = 0;

                //initialize commands
                for(int i=0; i<COMMANDS_QUANTITY; i++)
                    *command[i] = '\0';

                //separate by delimiter
                while(*ptr != '\0')
                {
                    if(*ptr == ':')
                    {
                        command[counter_cmmd][counter_word] = '\0'; // terminator
                        counter_cmmd++;
                        if(counter_cmmd == COMMANDS_QUANTITY)
                            continue;
                        counter_word = 0;
                    }
                    else
                    {
                        command[counter_cmmd][counter_word] = *ptr;
                        counter_word++;
                    }
                    ptr++;
                }
                command[counter_cmmd][counter_word] = '\0';

                //ESP_LOGI(TAG_T,"com: %s %s %s %s %s %s", command[0], command[1], command[2], command[3], command[4], command[5]);

                //check command
                if(strcmp(command[ID_T], SERVER_ID) == 0 && strcmp(command[USER_T], user_tcp) == 0  && strcmp(command[DEV_NUM_T], dev_num) == 0 )
                {
                    if(strcmp(command[OPERATION_T], "R") == 0) //LECTURA
                        handle_read_tcp(command, tx_buffer);
                        
                    else if(strcmp(command[OPERATION_T], "W") == 0) //ESCRITURA
                        handle_write_tcp(command, tx_buffer);
                        
                    else
                        nack_message_tcp(tx_buffer); 
                }
                else
                    nack_message_tcp(tx_buffer);

                //vTaskDelay(pdMS_TO_TICKS(50));
                #ifdef DECODE_COM
                    LOCK_MUTEX(tx_buffer_mutex)
                    codeMessage(tx_buffer, pasword_iot_desif);
                    UNLOCK_MUTEX(tx_buffer_mutex)
                #endif
                send(sock, tx_buffer, strlen(tx_buffer), 0);
                ESP_LOGI(TAG_T, "Message send to %s: %s", host, tx_buffer);
            }
            else if(send_ack_f>1)
            {
                ESP_LOGE(TAG_T, "No message received after keep alive");
                counter_no_ack++;
                send_ack_f=0;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    
        ESP_LOGE(TAG_T, "Stoping timer...");
        if (xTimerStop(timer1, 0) == pdPASS)
            xTimerDelete(timer1, 0);
        else 
            ESP_LOGE(TAG_T, "Failed to stop timer");
        //ESP_LOGE(TAG_T, "Deleting button task...");
        //vTaskDelete(button_handle);
        break;
    }
    ESP_LOGE(TAG_T, "Closing socket...");
    shutdown(sock, 0);
    close(sock);
}

void callback_keep_alive(TimerHandle_t pxTimer)
{
	ESP_LOGI(TAG_T, "Timer called");

    LOCK_MUTEX(tx_buffer_mutex)
        increment_id_packet();
        sprintf(id_pck_str, "%u", id_pck);
        build_command(keep_alive, SERVER_ID, id_pck_str, user_tcp, dev_num_tcp, "K", "S", "keep_alive", "\0");
        sprintf(tx_buffer, keep_alive);
    UNLOCK_MUTEX(tx_buffer_mutex)
    #ifdef DECODE_COM
        LOCK_MUTEX(tx_buffer_mutex)
        codeMessage(tx_buffer, pasword_iot_desif);
        UNLOCK_MUTEX(tx_buffer_mutex)
    #endif
    send(sock, tx_buffer, strlen(tx_buffer), 0);
    ESP_LOGI(TAG_T, "Message send to %s: %s", host, tx_buffer);

	send_ack_f=1;
}

void setTimerKeepalieve()
{
	ESP_LOGI(TAG_T, "Timer initialized");
	timer1 = xTimerCreate("timer",
                        (pdMS_TO_TICKS(TIMER_RESET)),
                        pdTRUE,
                        (void *)timerId,
                        callback_keep_alive);

	if(timer1 == NULL)
		ESP_LOGE(TAG_T, "Timer not created");
	else
	{
		if( xTimerStart(timer1, 0) != pdPASS)
			ESP_LOGI(TAG_T, "Timer could not be set to Active state");
	}
}

//connection to time api 
void clock_task(void *pvParameters)
{
    real_time_key = xSemaphoreCreateBinary();

    xTaskCreate(tcp_real_time_task, "tcp_real_time_task", 4096, NULL, 5, NULL);
    xSemaphoreGive(real_time_key); //get real time semaphore 

    #ifdef CHECK_TIME_OFFSET
    xTaskCreate(check_time_offset_task, "check_time_offset_task", 4096, NULL, 5, NULL);
    #endif

    //wait to get real time
    while(seconds_true>60)
        vTaskDelay(pdMS_TO_TICKS(100));
    
    //if hour not valid
    if(hours_true>24)
    {
        ESP_LOGE(TAG_T_CLOCK, "Could not get real time. Set to 00:00:00");
        hours = 0;
        minutes = 0;
        seconds = 0;
    }
    else
    {
        ESP_LOGI(TAG_T_CLOCK, "Succesfully got real time.");
        hours = hours_true;
        minutes = minutes_true;
        seconds = seconds_true;
    }
    seconds_true = 0xffff; //for next check on real time

    uint16_t internal_timer = 0;

    ESP_LOGI(TAG_T_CLOCK, "Starting local clock...");
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        seconds++;
        internal_timer++;
        if(seconds == 60)
        {
            seconds=0;
            minutes++;
            if(minutes == 60) 
            {
                minutes=0;
                hours++;
                if(hours == 24)
                    hours=0;
            }
        }
        #ifdef PRINT_TIME
        ESP_LOGI(TAG_T_CLOCK, "ESP local time: %d:%d:%d", hours, minutes, seconds);
        #endif

        //check offset every certain minutes
        #ifdef CHECK_TIME_OFFSET
        if(!(internal_timer % (60*CLOCK_MIN_TO_CHECK)))
        {
            xSemaphoreGive(real_time_key); //get real time semaphore 
            xSemaphoreGive(check_time_key); //check offset semaphore 
        }

        #endif
    }
    vSemaphoreDelete(real_time_key);
    ESP_LOGW(TAG_T, "Clock task closed.");
    vTaskDelete(NULL);
}

void tcp_real_time_task(void *pvParameters)
{
    ESP_LOGI(TAG_T_CLOCK, "Starting tcp time server task...");

    while(1)
    {
        if(xSemaphoreTake(real_time_key, portMAX_DELAY))
        {
            ESP_LOGI(TAG_T_CLOCK, "Conecting to worldtime api ...");
            connect_to_host_time(HOST_TIME, PORT_TIME);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGE(TAG_T_CLOCK, "Time task closed.");
    vTaskDelete(NULL);
}

void connect_to_host_time(char *host_parameter, int port)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_parameter);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    while(1)
    {
        char local_buffer[1024];
        
        sock2 = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock2 < 0) {
            ESP_LOGE(TAG_T_CLOCK, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_T_CLOCK, "Socket created, connecting to %s:%d", host_parameter, port);

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock2, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = connect(sock2, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG_T_CLOCK, "Socket unable to connect: errno %d", errno);
            hours_true = 0xffff;
            seconds_true = 0;
            break;
        }
        ESP_LOGI(TAG_T_CLOCK, "Successfully connected to: %s", host_parameter);

        strcpy(time_api_message, "GET /api/timezone/America/Tijuana.txt HTTP/1.1\r\n"
                                "Host: worldtimeapi.org\r\n"
                                "Connection: close\r\n"
                                "\r\n");

        //send http like request
        send(sock2, time_api_message, strlen(time_api_message), 0);  
        ESP_LOGI(TAG_T_CLOCK, "Message send to %s (%d bytes)", host_parameter, strlen(time_api_message));

        //receive text
        int len = recv(sock2, local_buffer, sizeof(local_buffer) - 1, 0);
        if(len < 0) 
        {
            ESP_LOGE(TAG_T_CLOCK, "Error occurred during receiving: errno %d", errno);
            hours_true = 0xffff;
            seconds_true = 0;
            break;
        }

        local_buffer[len] = '\0'; // terminator
        ESP_LOGI(TAG_T_CLOCK, "Received %d bytes", len);
        //ESP_LOGI(TAG_T_CLOCK, "%s", local_buffer);

        //process time on text
        char *ptr_time_1 = strstr(local_buffer, "datetime");
        char time_string[TIME_STRING_LEN] = "\0";
        if(ptr_time_1 != NULL)
        {
            char *ptr_time_2 = strstr(ptr_time_1+1, "datetime");
            if(ptr_time_2 != NULL)
            {
                strncpy(time_string, ptr_time_2+21, sizeof(time_string));
                time_string[TIME_STRING_LEN-1] = '\0';
            }
            else
            {
                ESP_LOGI(TAG_T_CLOCK, "No se encontro segunda instancia de texto");
                hours_true = 0xffff;
                seconds_true = 0;
                break;
            }
        }
        else
        {
            ESP_LOGI(TAG_T_CLOCK, "No se encontro instancia de texto");
            hours_true = 0xffff;
            seconds_true = 0;
            break;
        }

        //ESP_LOGW(TAG_T_CLOCK, "time: %s", time_string);

        //seperate time parts HH:MM:SS.SS
        char str[10];
        if(strlen(time_string) > 1)
        {
            strcpy(str, time_string);
            str[2] = '\0';
            hours_true = atoi(str);

            ptr = strstr(time_string, ":") +1;
            str[0] = *ptr;
            str[1] = *(ptr+1);
            str[2] = '\0';
            minutes_true = atoi(str);

            ptr = strstr(time_string+3, ":") +1;
            str[0] = *ptr;
            str[1] = *(ptr+1);
            str[2] = '\0';
            seconds_true = atoi(str);

            ESP_LOGI(TAG_T_CLOCK, "Time got: %d:%d:%d", hours_true, minutes_true, seconds_true);
        }
        break;
    }
    ESP_LOGW(TAG_T_CLOCK, "Closing time socket...");
    shutdown(sock2, 0);
    close(sock2);
}

void check_time_offset_task(void *pvParameters)
{
    ESP_LOGI(TAG_T_CLOCK, "Starting time offset task...");
    check_time_key = xSemaphoreCreateBinary();

    while(1)
    {
        if(xSemaphoreTake(check_time_key, portMAX_DELAY))
        {
            ESP_LOGW(TAG_T_CLOCK, "Waiting to get real time...");
            while(seconds_true>60)
                vTaskDelay(pdMS_TO_TICKS(30));
            if(hours_true<24)
            {
                int dif_h = hours - hours_true;
                int dif_m = minutes - minutes_true;
                int dif_s = seconds - seconds_true;
                ESP_LOGW(TAG_T_CLOCK, "Offset by %d:%d:%d", dif_h,dif_m,dif_s);
            }
            else
                ESP_LOGE(TAG_T_CLOCK, "Could not get real time to check offset.");
            seconds_true = 0xffff;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vSemaphoreDelete(check_time_key);
}

//check connection to google
uint8_t check_internet_connection()
{
    uint8_t connection_f = 0;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(HOST_GOOGLE);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT_GOOGLE);

    while(1)
    {
        char local_buffer[1024];

        sock3 = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock3 < 0) {
            ESP_LOGE(TAG_T, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_T, "Socket created, connecting to %s:%d", HOST_GOOGLE, PORT_GOOGLE);

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock3, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = connect(sock3, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG_T, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_T, "Successfully connected to: %s (Google)", HOST_GOOGLE);

        strcpy(time_api_message, "GET / HTTP/1.1\r\nHost: google.com\r\nConnection: close\r\n\r\n");

        //send hhtp like request
        send(sock3, time_api_message, strlen(time_api_message), 0);  
        ESP_LOGI(TAG_T, "Message send to %s (%d bytes)", HOST_GOOGLE, strlen(time_api_message));

        //receive text
        int len = recv(sock3, local_buffer, sizeof(local_buffer) - 1, 0);
        if(len < 0) 
        {
            ESP_LOGE(TAG_T, "Error occurred during receiving: errno %d", errno);
            break;
        }

        local_buffer[len] = '\0'; // terminator
        ESP_LOGI(TAG_T, "Received %d bytes", len);
        ESP_LOGI(TAG_T, "There is an internet connection.");
        connection_f = 1;

        break;
    }
    ESP_LOGE(TAG_T, "Closing google socket...");
    shutdown(sock3, 0);
    close(sock3);

    return connection_f;
}

void handle_read_tcp(char command[][128], char *tx_buffer)
{
    uint32_t local_buffer;

    if(command[ELEMENT_T] == LED_HEX)
    {
        sprintf(local_buffer, "ACK:%d", l_state);
        message_to_buffer(tx_buffer, local_buffer);
    }
    else if(command[ELEMENT_T] == ADC_HEX)
    {
        sprintf(local_buffer, "ACK:%d", adc_value);
        message_to_buffer(tx_buffer, local_buffer);
    }
    else if(strcmp(command[ELEMENT_T], "WIFI") == 0)
    {
        read_nvs(key_ssid, nvs_value, sizeof(nvs_value));
        sprintf(local_buffer, "ACK:%s", nvs_value);
        message_to_buffer(tx_buffer, local_buffer);
    }
    else if(strcmp(command[ELEMENT_T], "PASS") == 0)
    {
        read_nvs(key_pass, nvs_value, sizeof(nvs_value));
        sprintf(local_buffer, "ACK:%s", nvs_value);
        message_to_buffer(tx_buffer, local_buffer);
    }
    else if(command[ELEMENT_T] == PWM)
    {
        uint32_t value_pwm = ledc_get_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
        sprintf(local_buffer, "ACK:%" PRIu32 "", value_pwm);
        message_to_buffer(tx_buffer, local_buffer);
    }
    else
        nack_message_tcp(&tx_buffer);
}

void handle_write_tcp(char command[][128], char *tx_buffer)
{
    uint32_t local_buffer;

    if(command[ELEMENT_T] == LED_HEX)
    {
        if(command[VALUE_T] == 0x01)
        {
            sprintf(local_buffer, "ACK");
            message_to_buffer(tx_buffer, local_buffer);
            l_state = 1;
        }
        else if(command[VALUE_T] == 0x00)
        {
            sprintf(local_buffer, "ACK");
            message_to_buffer(tx_buffer, local_buffer);
            l_state = 0;
        }
        else
            nack_message_tcp(&tx_buffer);
    }
    else if(strcmp(command[ELEMENT_T], "WIFI") == 0)
    {
        write_nvs(key_ssid, command[VALUE_T]);
        sprintf(local_buffer, "ACK:%s", command[VALUE_T]);
        message_to_buffer(tx_buffer, local_buffer);
    }
    else if(strcmp(command[ELEMENT_T], "PASS") == 0)
    {
        write_nvs(key_pass, command[VALUE_T]);
        sprintf(local_buffer, "ACK:%s", command[VALUE_T]);
        message_to_buffer(tx_buffer, local_buffer);
    }
    else if(strcmp(command[ELEMENT_T], "P") == 0)
    {
        if(command[VALUE_T] == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(0)); //0%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(local_buffer, "ACK:%s", command[VALUE_T]);
            message_to_buffer(tx_buffer, local_buffer);
        }
        else if(command[VALUE_T] == 25)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION * 0.25)); //25%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(local_buffer, "ACK:%s", command[VALUE_T]);
            message_to_buffer(tx_buffer, local_buffer);
        }
        else if(command[VALUE_T] == 50)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION * 0.5)); //50%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(local_buffer, "ACK:%s", command[VALUE_T]);
            message_to_buffer(tx_buffer, local_buffer);
        }
        else if(command[VALUE_T] == 100)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION)); //100%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(local_buffer, "ACK:%s", command[VALUE_T]);
            message_to_buffer(tx_buffer, local_buffer);
        }
        else
        nack_message_tcp(&tx_buffer);
    }
    else
        nack_message_tcp(&tx_buffer);
}

void message_to_buffer(uint32_t *buffer, uint32_t message)
{
    LOCK_MUTEX(tx_buffer_mutex)
    *buffer = message;
    UNLOCK_MUTEX(tx_buffer_mutex)
}

void nack_message_tcp(uint32_t *buffer)
{
    LOCK_MUTEX(tx_buffer_mutex)
    *buffer = NACK;
    UNLOCK_MUTEX(tx_buffer_mutex)
}

void aplicarXor(char *original_message, char *xor_message)
{
    for(int i=0; i < strlen(original_message); i++)
        xor_message[i] = original_message[i] ^ 0x55;

    xor_message[strlen(original_message)] = '\0';
}

void codeMessage(char *message, char *password)
{
    counter = 0;
    
    for(int i=0; i < strlen(message); i++)
    {
        message[i] = message[i] ^ password[counter];
        counter++;

        if( counter >= strlen(password) )
            counter = 0;
    }
}

void build_command(uint32_t *comm, ...)
{
    *comm = 0;

    va_list args;
    va_start(args, comm);

    for(int i=0; i<num_elements; i++)
    {
        uint16_t bits = va_arg(args, int) & bit_lengths[i]; 
        printf("%16X\n", bits);

        *comm = (*comm << bit_lengths[i]) | bits;
        printf("com: %X\n", (unsigned int)*comm);
    }
    va_end(args);
}