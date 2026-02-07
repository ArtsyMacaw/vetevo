#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "secret.h" // Not included in repo
/* Defines:
 * - WIFI_SSID
 * - WIFI_PASSWORD
 *   LATITUDE
 *   LONGITUDE
 *   API_KEY
 */

// TODO: At some point see if I can check with Valgrind for memory leaks

static EventGroupHandle_t wifi_event_group;
static int retry_num = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY_NUM 5

// Root server certificate for api.openweathermap.org embedded by CMake
extern const uint8_t server_cert_pem_start[] asm("_binary_openweather_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_openweather_pem_end");

// Current weather conditions doesn't store all returned data from API just
// those that are displayed
static struct weather_today
{
    char description[32];
    float current_temp;
    float high_temp;
    float low_temp;
    float wind_speed;
    float precipitation; // mm of rain/snow in the last hour
    uint8_t cloudiness;
};

static struct weather_today weather;

#define FORECAST_DAYS 5

static struct weather_forecast
{
    char description[32];
    float high_temp;
    float low_temp;
    float wind_speed;
    float precipitation_chance;
    uint8_t cloudiness;
};

static struct weather_forecast forecast[FORECAST_DAYS];

// TODO: Check esp_err_t return value
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
    // TODO: If ESP_ERROR returns an error restart the board and try again.
    // preferably the ULP should continue to update the time on the display

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
        .path = "/data/2.5/weather?units=imperial&lat=" LATITUDE "&lon=" LONGITUDE "&appid=" API_KEY,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_event_handler,
        .cert_pem = (char *)server_cert_pem_start
    };
    ESP_LOGI("http", "HTTP client configured with host=%s, path=%s", config.host, config.path);
    ESP_LOGI("http", "Getting todays weather...");

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("http", "HTTP GET request failed: %s", esp_err_to_name(err));
        // TODO: Schedule retry after 10 minutes
    }
    int content_length = esp_http_client_fetch_headers(client);

    char *buffer = malloc(content_length + 1);
    assert(buffer != NULL); // malloc shouldn't unless there's a memory leak
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

    // Parse JSON response and populate the weather struct
    cJSON *json = cJSON_ParseWithLength(buffer, content_length);
    if (json == NULL)
    {
        // TODO: Schedule retry after 10 minutes
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE("json", "Error before: %s", error_ptr);
        }
    } else {
        /* Assertions are used here because if the API response doesn't match the expected format then
         * it means the API has changed and the code needs to be updated. */
        cJSON *weather_array = cJSON_GetObjectItem(json, "weather");
        assert(cJSON_IsArray(weather_array));
        // weather is an array but only ever returns one item
        assert(cJSON_GetArraySize(weather_array) > 0);
        cJSON *weather_item = cJSON_GetArrayItem(weather_array, 0);
        cJSON *description = cJSON_GetObjectItem(weather_item, "description");
        if (cJSON_IsString(description))
        {
            strncpy(weather.description, description->valuestring, sizeof(weather.description) - 1);
            weather.description[sizeof(weather.description) - 1] = '\0';
            ESP_LOGI("weather", "Description: %s", weather.description);
        }

        cJSON *main = cJSON_GetObjectItem(json, "main");
        assert(cJSON_IsObject(main));
        cJSON *temp = cJSON_GetObjectItem(main, "temp");
        if (cJSON_IsNumber(temp))
        {
            weather.current_temp = temp->valuedouble;
            ESP_LOGI("weather", "Current Temp: %.2f F", weather.current_temp);
        }
        cJSON *temp_min = cJSON_GetObjectItem(main, "temp_min");
        if (cJSON_IsNumber(temp_min))
        {
            weather.low_temp = temp_min->valuedouble;
            ESP_LOGI("weather", "Low Temp: %.2f F", weather.low_temp);
        }
        cJSON *temp_max = cJSON_GetObjectItem(main, "temp_max");
        if (cJSON_IsNumber(temp_max))
        {
            weather.high_temp = temp_max->valuedouble;
            ESP_LOGI("weather", "High Temp: %.2f F", weather.high_temp);
        }

        cJSON *wind = cJSON_GetObjectItem(json, "wind");
        assert(cJSON_IsObject(wind));
        cJSON *wind_speed = cJSON_GetObjectItem(wind, "speed");
        if (cJSON_IsNumber(wind_speed))
        {
            weather.wind_speed = wind_speed->valuedouble;
            ESP_LOGI("weather", "Wind Speed: %.2f mph", weather.wind_speed);
        }

        cJSON *clouds = cJSON_GetObjectItem(json, "clouds");
        assert(cJSON_IsObject(clouds));
        cJSON *cloudiness = cJSON_GetObjectItem(clouds, "all");
        if (cJSON_IsNumber(cloudiness))
        {
            weather.cloudiness = cloudiness->valueint;
            ESP_LOGI("weather", "Cloudiness: %d%%", weather.cloudiness);
        }

        // Won't return anything if it hasn't rained in the last hour
        cJSON *rain = cJSON_GetObjectItem(json, "rain");
        if (cJSON_IsObject(rain))
        {
            cJSON *rain_1h = cJSON_GetObjectItem(rain, "1h");
            if (cJSON_IsNumber(rain_1h))
            {
                weather.precipitation = rain_1h->valuedouble;
                ESP_LOGI("weather", "Precipitation (rain) in last hour: %.2f mm", weather.precipitation);
            }
        } else {
            ESP_LOGI("weather", "Not currently raining");
            weather.precipitation = -1; // Set negative to indicate no precipitation data
        }
    }
    free(buffer);
    cJSON_Delete(json);
    esp_http_client_close(client);

    config.path = "/data/2.5/forecast?units=imperial&lat=" LATITUDE "&lon=" LONGITUDE "&appid=" API_KEY;
    ESP_LOGI("http", "Getting weather forcasts...");

    client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("http", "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    content_length = esp_http_client_fetch_headers(client);
    buffer = malloc(content_length + 1);
    assert(buffer != NULL);
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

    // Parse JSON response and populate the forecast array; data updates every three hours
    json = cJSON_ParseWithLength(buffer, content_length);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE("json", "Error before: %s", error_ptr);
        }
    } else {
        for (int i = 0; i < FORECAST_DAYS; i++)
        {
            cJSON *list = cJSON_GetObjectItem(json, "list");
            assert(cJSON_IsArray(list));
            cJSON *forecast_item = cJSON_GetArrayItem(list, i);
            cJSON *main = cJSON_GetObjectItem(forecast_item, "main");
            assert(cJSON_IsObject(main));
            cJSON *temp_min = cJSON_GetObjectItem(main, "temp_min");
            if (cJSON_IsNumber(temp_min))
            {
                forecast[i].low_temp = temp_min->valuedouble;
                ESP_LOGI("forecast", "Day %d Low Temp: %.2f F", i + 1, forecast[i].low_temp);
            }
            cJSON *temp_max = cJSON_GetObjectItem(main, "temp_max");
            if (cJSON_IsNumber(temp_max))
            {
                forecast[i].high_temp = temp_max->valuedouble;
                ESP_LOGI("forecast", "Day %d High Temp: %.2f F", i + 1, forecast[i].high_temp);
            }
            cJSON *wind = cJSON_GetObjectItem(forecast_item, "wind");
            assert(cJSON_IsObject(wind));
            cJSON *wind_speed = cJSON_GetObjectItem(wind, "speed");
            if (cJSON_IsNumber(wind_speed))
            {
                forecast[i].wind_speed = wind_speed->valuedouble;
                ESP_LOGI("forecast", "Day %d Wind Speed: %.2f mph", i + 1, forecast[i].wind_speed);
            }
            cJSON *clouds = cJSON_GetObjectItem(forecast_item, "clouds");
            assert(cJSON_IsObject(clouds));
            cJSON *cloudiness = cJSON_GetObjectItem(clouds, "all");
            if (cJSON_IsNumber(cloudiness))
            {
                forecast[i].cloudiness = cloudiness->valueint;
                ESP_LOGI("forecast", "Day %d Cloudiness: %d%%", i + 1, forecast[i].cloudiness);
            }
            cJSON *precipitation_chance = cJSON_GetObjectItem(forecast_item, "pop");
            if (cJSON_IsNumber(precipitation_chance))
            {
                forecast[i].precipitation_chance = precipitation_chance->valuedouble;
                ESP_LOGI("forecast", "Day %d Precipitation Chance: %.2f%%", i + 1, forecast[i].precipitation_chance * 100);
            }
            cJSON *weather_array = cJSON_GetObjectItem(forecast_item, "weather");
            assert(cJSON_IsArray(weather_array));
            assert(cJSON_GetArraySize(weather_array) > 0);
            cJSON *weather_item = cJSON_GetArrayItem(weather_array, 0);
            cJSON *description = cJSON_GetObjectItem(weather_item, "description");
            if (cJSON_IsString(description))
            {
                strncpy(forecast[i].description, description->valuestring, sizeof(forecast[i].description) - 1);
                forecast[i].description[sizeof(forecast[i].description) - 1] = '\0';
                ESP_LOGI("forecast", "Day %d Description: %s", i + 1, forecast[i].description);
            }
        }
    }

    cJSON_Delete(json);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
}

void sync_sntp_time()
{
    // RTC clock can drift so sync it needs to be synced with an NTP server periodically
    time_t now = 0;
    struct tm timeinfo = { 0 };
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp_config);
    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();

    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK)
    {
        ESP_LOGE("sntp", "Failed to synchronize time");
    }
    else
    {
        ESP_LOGI("sntp", "Time synchronized successfully");
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI("sntp", "Current time: %s", asctime(&timeinfo));
    esp_netif_sntp_deinit();
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
    sync_sntp_time();

    https_get_task();
}
