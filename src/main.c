#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "relay_controller.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"

void app_main(void) {
    // Check if we need to confirm the new firmware
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // In a real application, you might run more extensive diagnostics here.
            // For now, we'll consider successful boot to app_main as a success.
            ESP_LOGI("APP", "First boot of new firmware. Marking as valid...");
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                ESP_LOGI("APP", "Rollback cancelled successfully!");
            } else {
                ESP_LOGE("APP", "Failed to cancel rollback!");
            }
        }
    }

    // Initialize NVS (needed for Wi-Fi)
    nvs_flash_init();

    // Initialize Wi-Fi
    wifi_init_sta();

    // Initialize SPIFFS for web files
    if (init_spiffs() != ESP_OK) {
        ESP_LOGE("APP", "Failed to initialize SPIFFS");
    }

    // Initialize relay GPIOs
    relay_init();

    // Start HTTP server
    web_server_start();

    ESP_LOGI("APP", "Relay web server started!");

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
