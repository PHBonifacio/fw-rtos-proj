#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "defines.h"

static const char *TAG = "MQTT_SAMPLE";

static EventGroupHandle_t program_event_group;
QueueHandle_t xQueue_Input = NULL;
QueueHandle_t xQueue_Led = NULL;


static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) 
    {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI("WIFI EVENT", "SYSTEM_EVENT_STA_START");
            esp_wifi_connect();
        break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI("WIFI EVENT", "SYSTEM_EVENT_STA_GOT_IP");
            xEventGroupSetBits(program_event_group, WIFI_CONNECTED);

        break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI("WIFI EVENT", "SYSTEM_EVENT_STA_DISCONNECTED");
            esp_wifi_connect();
            xEventGroupClearBits(program_event_group, WIFI_CONNECTED);
        break;
        default:
        break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(program_event_group, WIFI_CONNECTED, false, true, portMAX_DELAY);
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_URI,
        .event_handle = mqtt_event_handler,
        .username = CONFIG_MQTT_USER,
        .password = CONFIG_MQTT_PWRD
        // .user_context = (void *)your_context
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void Task_Init_Comm( void *pvParameter )
{
    printf("Aguardando Evento para iniciar a comunicação\r\n");

    for(;;)
    {
        EventBits_t events = xEventGroupWaitBits(program_event_group, \
            (WAIT_TO_CONNECT | WIFI_CONNECTED | MQTT_CONNECTED), \
            true, false, portMAX_DELAY);
        
        if (0 != (events & WAIT_TO_CONNECT))
        {
            printf("Event Clean: WAIT_TO_CONNECT \r\n");
            xEventGroupClearBits(program_event_group, WAIT_TO_CONNECT);

            printf("Iniciando Conexão Wifi\r\n");
            nvs_flash_init();
            wifi_init();
        }
        else if (0 != (events & WIFI_CONNECTED))
        {
            printf("Iniciando Conexão MQTT\r\n");            
            mqtt_app_start();            
        }
    }

    vTaskDelete(NULL);
}

void Task_GPIO( void *pvParameter )
{
    uint16_t last_read = 1;
    uint16_t new_read = 1;
    printf("Configurando GPIO\r\n");

    /*  CONFIGURA LED E ATIVA */
    gpio_pad_select_gpio(GPIO_LED); 
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED, 1);

    /*  CONFIGURA PINO DE ENTRADA PARA DISPARAR EVENTOS */
	gpio_pad_select_gpio(GPIO_INPUT);	
    gpio_set_direction(GPIO_INPUT, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_INPUT, GPIO_PULLUP_ONLY);

    printf("Aguardando acionamento\r\n");
    
    program_event_group = xEventGroupCreate();

    xEventGroupSetBits(program_event_group, WAIT_FIST_INPUT);

    for(;;)
    {
        new_read = gpio_get_level(GPIO_INPUT);

        if (last_read != new_read)  
        {
            EventBits_t events = xEventGroupGetBits(program_event_group);
            
            if ((0 == new_read) && (0 != (events & WAIT_FIST_INPUT)))
            {
                xEventGroupClearBits(program_event_group, WAIT_FIST_INPUT);

                xEventGroupSetBits(program_event_group, WAIT_TO_CONNECT);
            }
            else if (0 != (events & (WIFI_CONNECTED | MQTT_CONNECTED)))
            {
                xEventGroupSetBits(program_event_group, GPIO_EVENT);
            }
            last_read = new_read;
        }
        vTaskDelay(100);
    } 
    vTaskDelete(NULL);
}

void app_main()
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    xQueue_Input = xQueueCreate(1, sizeof(uint8_t));
    xQueue_Led = xQueueCreate(1, sizeof(uint8_t));
    
    printf("Iniciando configurações\r\n");

    xTaskCreate(Task_GPIO, "Task_GPIO", configMINIMAL_STACK_SIZE, NULL, \
        (tskIDLE_PRIORITY + 1), NULL);
    xTaskCreate(Task_Init_Comm, "Task_Init_Comm", configMINIMAL_STACK_SIZE, NULL, \
        (tskIDLE_PRIORITY + 1), NULL);
}
