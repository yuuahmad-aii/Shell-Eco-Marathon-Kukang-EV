#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include "esp_netif_sntp.h"


// -----------------------------------------------------------------------------
// USER CONFIGURATION (Silakan ganti sesuai dengan pengaturan Anda)
// -----------------------------------------------------------------------------
#define WIFI_SSID "ESP32TEST"
#define WIFI_PASS "PASS_TEST"
#define FIREBASE_URL                                                           \
  "https://kukang-ev-default-rtdb.asia-southeast1.firebasedatabase.app/"       \
  "telemetry.json?auth=AIzaSyCkwhZaXiJ1YkHU792jhgdiCZVIZLXcLu0"

// UART Configuration
#define UART_NUM UART_NUM_1
#define UART_TX_PIN 5
#define UART_RX_PIN 4
#define UART_BAUD_RATE 115200
#define BUF_SIZE 1024

static const char *TAG = "Telemetry_Coprocessor";

// FreeRTOS event group to signal when we are connected to WiFi
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 10) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASS,
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  // Wait until either the connection is established or failed
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

static void obtain_time(void) {
  ESP_LOGI(TAG, "Initializing SNTP for HTTPS certificate verification...");
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config);
  
  int retry = 0;
  const int retry_count = 15;
  while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
  }
  
  time_t now = 0;
  struct tm timeinfo = { 0 };
  time(&now);
  localtime_r(&now, &timeinfo);
  ESP_LOGI(TAG, "Current time: %s", asctime(&timeinfo));
}

static void uart_init(void) {
  uart_config_t uart_config = {
      .baud_rate = UART_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  int intr_alloc_flags = 0;

  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL,
                                      intr_alloc_flags));
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) { return ESP_OK; }

void send_to_firebase(const char *json_data) {
  esp_http_client_config_t config = {
      .url = FIREBASE_URL,
      .event_handler = _http_event_handler,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .skip_cert_common_name_check = true, // Bypass cert verification for testing
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    ESP_LOGE(TAG, "Failed to initialise HTTP connection");
    return;
  }

  esp_http_client_set_method(client, HTTP_METHOD_PUT);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json_data, strlen(json_data));

  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Firebase PUT Status = %d, content_length = %lld",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  } else {
    ESP_LOGE(TAG, "HTTP PUT request failed: %s", esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
}

static void uart_rx_task(void *arg) {
  uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
  char line_buffer[BUF_SIZE];
  int line_len = 0;

  while (1) {
    int rxBytes =
        uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
    if (rxBytes > 0) {
      data[rxBytes] = 0;

      for (int i = 0; i < rxBytes; i++) {
        char c = (char)data[i];
        if (c == '\n') {
          line_buffer[line_len] = '\0';

          // Filter out empty lines or garbage
          if (line_len > 10 && line_buffer[0] == '{') {
            ESP_LOGI(TAG, "Received Telemetry: %s", line_buffer);
            // Forward to Firebase if WiFi is connected
            EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
            if (bits & WIFI_CONNECTED_BIT) {
              send_to_firebase(line_buffer);
            }
          }
          line_len = 0;
        } else if (c != '\r') {
          if (line_len < BUF_SIZE - 1) {
            line_buffer[line_len++] = c;
          } else {
            // Buffer overflow, drop line
            line_len = 0;
          }
        }
      }
    }
  }
  free(data);
}

void app_main(void) {
  // Initialize NVS (required for WiFi)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "Initializing WiFi...");
  wifi_init_sta();

  // Sync time via NTP so that HTTPS certificates can be validated
  obtain_time();

  ESP_LOGI(TAG, "Initializing UART...");
  uart_init();

  ESP_LOGI(TAG, "Starting UART RX Task...");
  xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 8, NULL,
              configMAX_PRIORITIES - 1, NULL);
}
