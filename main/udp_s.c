#include "udp_s.h"
#include "wifi.h"

static const char *TAG_U = "UDP SOCKET";

TaskHandle_t udp_server_handle = NULL;

void udp_server_task(void *pvParameters)
{
    char rx_buffer[STRING_LENGHT], tx_buffer[STRING_LENGHT], *ptr, command[COMMAND_NUM_U][STRING_LENGHT];
    char addr_str[STRING_LENGHT];
    int addr_family = (int)pvParameters, ip_protocol = 0, counter_cmmd, counter_word;
    struct sockaddr_in6 dest_addr;

    while (1)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG_U, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG_U, "Socket created");

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG_U, "Socket unable to bind: errno %d", errno);
        }
        else
            ESP_LOGI(TAG_U, "Socket bound, port %d", PORT);

        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);

        //sprintf(tx_buffer, "texto de prueba");

        while (1)
        {
            //ESP_LOGI(TAG_U, "Waiting for data");
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            if(len > 0)
            {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                rx_buffer[len] = '\0'; // terminator
                ESP_LOGI(TAG_U, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG_U, "%s", rx_buffer);

                ptr = rx_buffer;
                counter_cmmd = 0;
                counter_word = 0;

                //initialize commands
                for(int i=0; i<COMMAND_NUM_U; i++)
                    *command[i] = '\0';

                //separate by delimiter
                while(*ptr != '\0')
                {
                    if(*ptr == ':')
                    {
                        command[counter_cmmd][counter_word] = '\0'; // terminator
                        counter_cmmd++;
                        if(counter_cmmd == COMMAND_NUM_U)
                            break;
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

                //ESP_LOGI(TAG_U,"com: %s %s %s %s %s", command[0], command[1], command[2], command[3], command[4]);

                *tx_buffer = '\0'; //clear string buffer

                //check command
                if(strcmp(command[ID], "UABC") == 0)
                {
                    if(strcmp(command[OPERATION], "R") == 0) //LECTURA
                    {
                        handleRead(command, tx_buffer);
                    }
                    else if(strcmp(command[OPERATION], "W") == 0) //ESCRITURA
                    {
                        handleWrite(command, tx_buffer);
                    }
                    else
                       nackMessage(tx_buffer); 
                }
                else
                    nackMessage(tx_buffer);

                //retrace back  
                sendto(sock, tx_buffer, strlen(tx_buffer), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            }
            //else did not receive data
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG_U, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void handleRead(char command[][128], char *tx_buffer)
{
    char str[STRING_LENGHT] = "NULL";
    uint8_t local_uint = 0xff;

    if(strcmp(command[ELEMENT], "L") == 0)
        sprintf(tx_buffer, "ACK:%d", l_state);
    else if(strcmp(command[ELEMENT], "A") == 0)
        sprintf(tx_buffer, "ACK:%d", adc_value);
    else if(strcmp(command[ELEMENT], "WIFI") == 0)
    {
        read_nvs((char *)key_ssid, str, sizeof(str));
        sprintf(tx_buffer, "ACK:%s", str);
    }
    else if(strcmp(command[ELEMENT], "PASS") == 0)
    {
        read_nvs((char *)key_pass, str, sizeof(str));
        sprintf(tx_buffer, "ACK:%s", str);
    }
    else if(strcmp(command[ELEMENT], "DEV") == 0)
    {
        get_device_hostname(str);
        sprintf(tx_buffer, "ACK:%s", str);
    }
    else if(strcmp(command[ELEMENT], "USER") == 0)
    {
        read_nvs_uint8((char *)key_user, &local_uint);
        sprintf(tx_buffer, "ACK:%u", local_uint);
    }
    else if(strcmp(command[ELEMENT], "DEV_NUM") == 0)
    {
        read_nvs_uint8((char *)key_dev_num, &local_uint);
        sprintf(tx_buffer, "ACK:%u", local_uint);
    }
    else if(strcmp(command[ELEMENT], "IP") == 0)
        sprintf(tx_buffer, "ACK:%s", ip_addr);
    else if(strcmp(command[ELEMENT], "P") == 0)
    {
        uint32_t value_pwm = ledc_get_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
        sprintf(tx_buffer, "ACK:%" PRIu32 "", value_pwm);
    }
    else
        nackMessage(tx_buffer);
}

void handleWrite(char command[][128], char *tx_buffer)
{
    uint8_t local_uint = 0xff;

    if(strcmp(command[ELEMENT], "L") == 0)
    {
        if(strcmp(command[VALUE], "1") == 0)
        {
            sprintf(tx_buffer, "ACK");
            l_state = 1;
        }
        else if(strcmp(command[VALUE], "0") == 0)
        {
            sprintf(tx_buffer, "ACK");
            l_state = 0;
        }
        else
            nackMessage(tx_buffer);
    }
    else if(strcmp(command[ELEMENT], "WIFI") == 0)
    {
        write_nvs((char *)key_ssid, command[VALUE]);
        sprintf(tx_buffer, "ACK:%s", command[VALUE]);
    }
    else if(strcmp(command[ELEMENT], "PASS") == 0)
    {
        write_nvs((char *)key_pass, command[VALUE]);
        sprintf(tx_buffer, "ACK:%s", command[VALUE]);
    }
    else if(strcmp(command[ELEMENT], "DEV") == 0)
    {
        //set_device_hostname(command[VALUE]);
        write_nvs((char *)key_dev_name, command[VALUE]);
        sprintf(tx_buffer, "ACK:%s", command[VALUE]);
    }
    else if(strcmp(command[ELEMENT], "USER") == 0)
    {
        local_uint = string_to_uint8(command[VALUE]);
        write_nvs_uint8((char *)key_user, local_uint);
        sprintf(tx_buffer, "ACK:%u", local_uint);
    }
    else if(strcmp(command[ELEMENT], "DEV_NUM") == 0)
    {
        local_uint = string_to_uint8(command[VALUE]);
        write_nvs_uint8((char *)key_dev_num, local_uint);
        sprintf(tx_buffer, "ACK:%u", local_uint);
    }
    else if(strcmp(command[ELEMENT], "P") == 0)
    {
        if(strcmp(command[VALUE], "25") == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION * 0.25)); //25%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(tx_buffer, "ACK:%s", command[VALUE]);
        }
        else if(strcmp(command[VALUE], "50") == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION * 0.5)); //50%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(tx_buffer, "ACK:%s", command[VALUE]);
        }
        else if(strcmp(command[VALUE], "100") == 0)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, 
                        LEDC_CHANNEL_0, 
                        (uint32_t)(MAX_DUTY_RESOLUTION)); //100%
            ledc_update_duty(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0);
            sprintf(tx_buffer, "ACK:%s", command[VALUE]);
        }
        else
        nackMessage(tx_buffer);
    }
    else
        nackMessage(tx_buffer);
}

void nackMessage(char *str)
{
    sprintf(str, "NACK");
}
