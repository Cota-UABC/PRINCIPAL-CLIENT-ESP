#ifndef NVS_ESP
#define NVS_ESP

#include <string.h>
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

extern const char *TAG_NVS, *key_ssid, *key_pass, *key_dev_name, *key_user, *key_dev_num;

esp_err_t init_nvs();

esp_err_t read_nvs(char *key, char *value, size_t len);

esp_err_t write_nvs(char *key, char *value);

#endif