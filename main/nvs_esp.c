#include "nvs_esp.h"

char *TAG_NVS = "NVS", *key_ssid = "SSID", *key_pass = "PASS", *key_dev_name = "DEV", *key_user = "USER", *key_dev_num = "DEV_NUM";

nvs_handle_t esp_nvs_handle;

esp_err_t init_nvs()
{
    esp_err_t error;
    nvs_flash_init();

    error = nvs_open(TAG_NVS, NVS_READWRITE, &esp_nvs_handle);
    if(error != ESP_OK)
        ESP_LOGE(TAG_NVS, "Error in flash");
    else 
        ESP_LOGI(TAG_NVS, "Init complete");

    return error;
}

esp_err_t read_nvs(char *key, char *value, size_t len)
{
    esp_err_t error;
    error = nvs_get_str(esp_nvs_handle, key, value, &len);
    if(error != ESP_OK){
        ESP_LOGE(TAG_NVS, "Error reading nvs key: %s", key);
        //printf("error: %d", error);
        switch(error){
            case ESP_FAIL:
                ESP_LOGE(TAG_NVS, "Internal error; most likely due to corrupted NVS partition");
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGE(TAG_NVS, "Requested key doesn't exist");
                break;
            case ESP_ERR_NVS_INVALID_HANDLE:
                ESP_LOGE(TAG_NVS, "Handle has been closed or is NULL");
                break;
            case ESP_ERR_NVS_INVALID_NAME:
                ESP_LOGE(TAG_NVS, " Key name doesn't satisfy constraints");
                break;
            case ESP_ERR_NVS_INVALID_LENGTH:
                ESP_LOGE(TAG_NVS, "Length is not sufficient to store data");
                break;
            default:
                ESP_LOGE(TAG_NVS, "Other Error");
        }
    }
    else 
        ESP_LOGI(TAG_NVS, "Value read: %s", value);

    return error;
}

esp_err_t read_nvs_uint8(char *key, uint8_t *value)
{
    esp_err_t error;
    error = nvs_get_u8(esp_nvs_handle, key, value);
    if (error != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Error reading NVS key: %s", key);
        switch (error) {
            case ESP_FAIL:
                ESP_LOGE(TAG_NVS, "Internal error; most likely due to corrupted NVS partition");
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGE(TAG_NVS, "Requested key doesn't exist");
                break;
            case ESP_ERR_NVS_INVALID_HANDLE:
                ESP_LOGE(TAG_NVS, "Handle has been closed or is NULL");
                break;
            case ESP_ERR_NVS_INVALID_NAME:
                ESP_LOGE(TAG_NVS, "Key name doesn't satisfy constraints");
                break;
            case ESP_ERR_NVS_INVALID_LENGTH:
                ESP_LOGE(TAG_NVS, "Data length mismatch for uint8_t");
                break;
            default:
                ESP_LOGE(TAG_NVS, "Other Error");
        }
    } else {
        ESP_LOGI(TAG_NVS, "Value read for key %s: %u", key, *value);
    }

    return error;
}

esp_err_t write_nvs(char *key, char *value)
{
    esp_err_t error;
    error = nvs_set_str(esp_nvs_handle, key, value);
    if(error != ESP_OK)
        ESP_LOGE(TAG_NVS, "Error writing nvs key: %s", key);
    else 
        ESP_LOGI(TAG_NVS, "Value writen: %s", value);

    return error;
}

esp_err_t write_nvs_uint8(char *key, uint8_t value)
{
    esp_err_t error;
    error = nvs_set_u8(esp_nvs_handle, key, value);
    if(error != ESP_OK)
        ESP_LOGE(TAG_NVS, "Error writing nvs key: %s", key);
    else 
        ESP_LOGI(TAG_NVS, "Value writen: %u", value);

    return error;
}