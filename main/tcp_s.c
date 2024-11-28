#include "tcp_s.h"

int sock, sock2;

TimerHandle_t timer1;
const int timerId = 1;

static const char *TAG_T = "TCP";

TaskHandle_t tcp_server_handle = NULL;
TaskHandle_t button_handle = NULL;

char user_tcp[STR_LEN];

uint8_t send_f = 0;
uint64_t button_press_time=0, current_time, diff_time;

uint16_t counter, counter_no_ack=0;

volatile uint8_t send_ack_f = 0;

char host[30], rx_buffer[STR_LEN], tx_buffer[STR_LEN], *ptr, command[COMMANDS_QUANTITY][STR_LEN], 
    pasword_iot[50], pasword_iot_desif[50], login[STR_LEN], keep_alive[STR_LEN], time_api_message[STR_LEN], rx_buffer_time[STR_LEN*4];

typedef struct {
    char *str1;
    char *str2;
    char *str3;
} task_params_t;

//patch
void buildCommand(char *string_com, ...);

void vTimerCallback(TimerHandle_t pxTimer)
{
	ESP_LOGI(TAG_T, "Timer called");

    sprintf(tx_buffer, keep_alive);
    #ifdef DECODE_COM
        codeMessage(tx_buffer, pasword_iot_desif);
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

void button_task(void *pvParameters) //evento de boton
{
    while(1)
    {
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
        
        vTaskDelay(pdMS_TO_TICKS(50));// TODO: bajar el delay y revisar que no se active el watchdog
    }
}

void tcp_server_task(void *pvParameters)
{
    ESP_LOGW(TAG_T, "Conecting to iot tcp server...");
    connect_to_host(pvParameters, HOST_IOT_UABC, PORT_IOT_UABC);

    ESP_LOGW(TAG_T, "Conecting to local tcp server...");
    connect_to_host(pvParameters, HOST_LOCAL, PORT_LOCAL);
    
    ESP_LOGW(TAG_T, "TCP task closed.");
    vTaskDelete(NULL);
}

void connect_to_host(void *pvParameters, char *host_parameter, int port)
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

        
        buildCommand(login, SERVER_ID, user_tcp, dev_num, "L", "S", "login_server", "\0");
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
        buildCommand(keep_alive, SERVER_ID, user_tcp, dev_num, "K", "S", "keep_alive", "\0");
        sprintf(tx_buffer, keep_alive);
        #ifdef DECODE_COM
            codeMessage(tx_buffer, pasword_iot_desif); 
        #endif
        send(sock, tx_buffer, strlen(tx_buffer), 0);
        ESP_LOGI(TAG_T, "Message send to %s: %s", host, tx_buffer);

        setTimer();
        xTaskCreate(button_task, "button_task", 4096, NULL, 5, &button_handle);

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
            //ESP_LOGW(TAG_T, "WAITING MESSAGE");
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
                if(strcmp(command[ID_T], SERVER_ID) == 0 && (strcmp(command[USER_T], user_tcp) == 0 || strcmp(command[USER_T], "LAN") == 0) && strcmp(command[DEV_NUM_T], dev_num) == 0 )
                {
                    if(strcmp(command[OPERATION_T], "R") == 0) //LECTURA
                        handleRead_tcp(command, tx_buffer);
                        
                    else if(strcmp(command[OPERATION_T], "W") == 0) //ESCRITURA
                        handleWrite_tcp(command, tx_buffer);
                        
                    else
                        nackMessage_tcp(tx_buffer); 
                }
                else
                    nackMessage_tcp(tx_buffer);

                //vTaskDelay(pdMS_TO_TICKS(50));
                #ifdef DECODE_COM
                                codeMessage(tx_buffer, pasword_iot_desif);
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
            //vTaskDelay(pdMS_TO_TICKS(50));
        }
    
        ESP_LOGE(TAG_T, "Stoping timer...");
        if (xTimerStop(timer1, 0) == pdPASS)
            xTimerDelete(timer1, 0);
        else 
            ESP_LOGE(TAG_T, "Failed to stop timer");
        ESP_LOGE(TAG_T, "Deleting button task...");
        vTaskDelete(button_handle);
        break;
    }
    ESP_LOGE(TAG_T, "Closing socket...");
    shutdown(sock, 0);
    close(sock);
}

void tcp_time_task(void *pvParameters)
{
    ESP_LOGW(TAG_T, "Conecting to time tcp server...");
    connect_to_host_time(HOST_TIME, PORT_TIME);

    ESP_LOGW(TAG_T, "Time task closed.");
    vTaskDelete(NULL);
}

void connect_to_host_time(char *host_parameter, int port)
{
    int addr_family = AF_INET6;
    int ip_protocol = IPPROTO_TCP;

    struct sockaddr_in6 dest_addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(port),
    };

    inet_pton(AF_INET6, host_parameter, &dest_addr.sin6_addr);

    while(1)
    {
        sock2 = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock2 < 0) {
            ESP_LOGE(TAG_T, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_T, "Socket created, connecting to %s:%d", host_parameter, port);

        // Set timeout
        //struct timeval timeout;
        //timeout.tv_sec = 1;
        //timeout.tv_usec = 0;
        //setsockopt(sock2, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = connect(sock2, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG_T, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_T, "Successfully connected to: %s", host_parameter);

        strcpy(time_api_message, "GET / HTTP/1.1\nHost: https://worldtimeapi.org/api//ip.txt\nConnection: close\n\n");

        send(sock2, time_api_message, strlen(tx_buffer), 0);  
        ESP_LOGI(TAG_T, "Message send to %s: %s", host_parameter, time_api_message);

        int len = recv(sock2, rx_buffer_time, sizeof(rx_buffer_time) - 1, 0);
        if(len < 0) 
        {
            ESP_LOGE(TAG_T, "Error occurred during receiving: errno %d", errno);
            break;
        }

        rx_buffer_time[len] = '\0'; // terminator
        ESP_LOGI(TAG_T, "Received %d bytes", len);
        ESP_LOGI(TAG_T, "%s", rx_buffer_time);

        //process time on text

        break;
    }
    ESP_LOGE(TAG_T, "Closing time socket...");
    shutdown(sock2, 0);
    close(sock2);
}

void handleRead_tcp(char command[][128], char *tx_buffer)
{
    char str[STR_LEN] = "NULL";

    if(strcmp(command[ELEMENT_T], "L") == 0)
        sprintf(tx_buffer, "ACK:%d", l_state);
    else if(strcmp(command[ELEMENT_T], "A") == 0)
        sprintf(tx_buffer, "ACK:%d", adc_value);
    else if(strcmp(command[ELEMENT_T], "WIFI") == 0)
    {
        read_nvs(key_ssid, str, sizeof(str));
        sprintf(tx_buffer, "ACK:%s", str);
    }
    else if(strcmp(command[ELEMENT_T], "PASS") == 0)
    {
        read_nvs((const char*)key_pass, str, sizeof(str));
        sprintf(tx_buffer, "ACK:%s", str);
    }
    else if(strcmp(command[ELEMENT_T], "P") == 0)
    {
        uint32_t value_pwm = ledc_get_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
        sprintf(tx_buffer, "ACK:%" PRIu32 "", value_pwm);
    }
    else
        nackMessage_tcp(tx_buffer);
}

void handleWrite_tcp(char command[][128], char *tx_buffer)
{
    if(strcmp(command[ELEMENT_T], "L") == 0)
    {
        if(strcmp(command[VALUE_T], "1") == 0)
        {
            sprintf(tx_buffer, "ACK");
            l_state = 1;
        }
        else if(strcmp(command[VALUE_T], "0") == 0)
        {
            sprintf(tx_buffer, "ACK");
            l_state = 0;
        }
        else
            nackMessage_tcp(tx_buffer);
    }
    else if(strcmp(command[ELEMENT_T], "WIFI") == 0)
    {
        write_nvs(key_ssid, command[VALUE_T]);
        sprintf(tx_buffer, "ACK:%s", command[VALUE_T]);
    }
    else if(strcmp(command[ELEMENT_T], "PASS") == 0)
    {
        write_nvs((const char*)key_pass, command[VALUE_T]);
        sprintf(tx_buffer, "ACK:%s", command[VALUE_T]);
    }
    else if(strcmp(command[ELEMENT_T], "P") == 0)
    {
        if(strcmp(command[VALUE_T], "0") == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(0)); //0%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(tx_buffer, "ACK:%s", command[VALUE_T]);
        }
        else if(strcmp(command[VALUE_T], "25") == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION * 0.25)); //25%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(tx_buffer, "ACK:%s", command[VALUE_T]);
        }
        else if(strcmp(command[VALUE_T], "50") == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION * 0.5)); //50%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(tx_buffer, "ACK:%s", command[VALUE_T]);
        }
        else if(strcmp(command[VALUE_T], "100") == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION)); //100%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(tx_buffer, "ACK:%s", command[VALUE_T]);
        }
        else
        nackMessage_tcp(tx_buffer);
    }
    else
        nackMessage_tcp(tx_buffer);
}

void nackMessage_tcp(char *str)
{
    sprintf(str, "NACK");
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

void buildCommand(char *string_com, ...)
{
    va_list args;
    va_start(args, string_com);
    char str_temp[STR_LEN];

    for(int i=0; i<COMMANDS_QUANTITY; i++)
    {
        sprintf(str_temp, va_arg(args, char *));
        if(!strcmp(str_temp, "\0")) //if last argument
        {
            string_com[strlen(string_com) - 1] = '\0';
            break;
        }

        strcat(string_com, str_temp);
        strcat(string_com, ":");
    }
    va_end(args);
}