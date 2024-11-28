#include "mqtt.h"

static const char *TAG_M = "mqtt_publisher";

esp_mqtt_client_handle_t client;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_M, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_M, "MQTT_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
}

void send_mqtt_message()
{
    int msg_id = esp_mqtt_client_publish(client, TOPIC, MESSAGE, 0, 1, 0);
    ESP_LOGI(TAG_M, "Message published, msg_id=%d", msg_id);
}

void mqtt_app_start(void) 
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_IP, // ip local for testing
        .credentials.username = USER,  // Nombre de usuario
        .credentials.authentication.password = PASSWORD,    // Contraseña
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}