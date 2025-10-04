#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "relay_controller.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "nvs_flash.h"

void app_main(void) {
    // Initialize NVS (needed for Wi-Fi)
    nvs_flash_init();

    // Initialize Wi-Fi
    wifi_init_sta();

    // Initialize relay GPIOs
    relay_init();

    // Start HTTP server
    web_server_start();

    ESP_LOGI("APP", "Relay web server started!");

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
