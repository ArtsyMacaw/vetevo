#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "secret.h" // Not included in repo
/* Defines:
 * - WIFI_SSID
 * - WIFI_PASSWORD
 * - API_PATH (the path portion of the URL for the OpenWeather API, including query parameters)
 */

static EventGroupHandle_t wifi_event_group;
static int retry_num = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY_NUM 5

extern const uint8_t server_cert_pem_start[] asm("_binary_openweather_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_openweather_pem_end");

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    // Most of these events should never happen,
    // but I'm including them for completeness and debugging purposes
    switch (evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            ESP_LOGI("http", "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI("http", "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI("http", "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI("http", "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI("http", "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("http", "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI("http", "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI("http", "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (retry_num < MAX_RETRY_NUM)
        {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI("wifi", "Failed to connect to the AP, retrying... (%d/%d)", retry_num, MAX_RETRY_NUM);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI("wifi", "Disconnected from the AP");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta()
{
    // TODO: implement error handling for everything

    // Default event loop must be created before create_default_wifi_sta() is called
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_create_default());

    // Create Network Interface
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_init(&cfg));

    // Setup event loop and handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(WIFI_EVENT,
                                                                ESP_EVENT_ANY_ID,
                                                                &wifi_event_handler,
                                                                NULL,
                                                                &instance_any_id));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(IP_EVENT,
                                                                IP_EVENT_STA_GOT_IP,
                                                                &wifi_event_handler,
                                                                NULL,
                                                                &instance_got_ip));

    // Configure Wi-Fi connection and start the interface
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD
        }
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start Wi-Fi
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
    ESP_LOGI("wifi", "Wi-Fi initialization completed.");

    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI("wifi", "Connected to AP");
    }
    // TODO: Need to handle failure eventually and retry after 10 minutes
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI("wifi", "Failed to connect to AP");
    } else {
        ESP_LOGE("wifi", "UNEXPECTED EVENT");
    }

}

static void https_get_task()
{
    esp_http_client_config_t config =
    {
        .host = "api.openweathermap.org",
        .path = API_PATH,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_event_handler,
        .cert_pem = (char *)server_cert_pem_start
    };
    ESP_LOGI("http", "HTTP client configured with host=%s, path=%s", config.host, config.path);
    ESP_LOGI("http", "Getting weather forcasts...");

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("http", "HTTP GET request failed: %s", esp_err_to_name(err));
        // TODO: implement error handling
    }
    int content_length = esp_http_client_fetch_headers(client);
    char *buffer = malloc(content_length + 1);
    if (buffer)
    {
        int read_len = esp_http_client_read(client, buffer, content_length);
        if (read_len >= 0)
        {
            buffer[read_len] = '\0';
            ESP_LOGI("http", "Received data: %s", buffer);
        }
        else
        {
            ESP_LOGE("http", "Failed to read response");
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
}

void app_main()
{
    // Wi-Fi requires NVS flash to store credentials otherwise it will fail to initialize
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);

    wifi_init_sta();

    https_get_task();
}
