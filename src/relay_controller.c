#include "relay_controller.h"

#include <esp_log.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const gpio_num_t relay_pins[NUM_RELAYS] = {(gpio_num_t) 7, (gpio_num_t) 6, (gpio_num_t) 5, (gpio_num_t) 10};
// Adjust your GPIOs
static relay_mode_t relay_modes[NUM_RELAYS] = {RELAY_MODE_OFF};
static TimerHandle_t relay_timers[NUM_RELAYS] = {0};

static void relay_timer_callback(TimerHandle_t xTimer) {
    uint32_t relay_num = (uint32_t)pvTimerGetTimerID(xTimer);
    ESP_LOGI("RELAY", "Relay %d safety timeout reached", (int)relay_num + 1);
    relay_off(relay_num);
}

void relay_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    for (int i = 0; i < NUM_RELAYS; i++) {
        io_conf.pin_bit_mask |= (1ULL << relay_pins[i]);
        gpio_set_level(relay_pins[i], 1); // Start with HIGH = OFF for active-low relays
        relay_modes[i] = RELAY_MODE_OFF;

        // Create timers but don't start them
        char timer_name[16];
        snprintf(timer_name, sizeof(timer_name), "relay_t%d", i);
        relay_timers[i] = xTimerCreate(timer_name, pdMS_TO_TICKS(1000), pdFALSE, (void*)(uintptr_t)i, relay_timer_callback);
    }

    gpio_config(&io_conf);
    ESP_LOGI("RELAY", "Relay controller initialized");
}

void relay_on(const uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS) return;

    // Turn on
    gpio_set_level(relay_pins[relay_num], 0); // LOW = ON for active-low relays
    relay_modes[relay_num] = RELAY_MODE_MANUAL;

    // Start safety timer (20 minutes)
    if (relay_timers[relay_num]) {
        xTimerChangePeriod(relay_timers[relay_num], pdMS_TO_TICKS(MAX_ON_TIME_SEC * 1000), 0);
        xTimerStart(relay_timers[relay_num], 0);
    }

    ESP_LOGI("RELAY", "Relay %d turned ON (Manual, 20m safety)", relay_num + 1);
}

void relay_off(const uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS) return;

    // Stop timer if running
    if (relay_timers[relay_num]) {
        xTimerStop(relay_timers[relay_num], 0);
    }

    gpio_set_level(relay_pins[relay_num], 1); // HIGH = OFF for active-low relays
    relay_modes[relay_num] = RELAY_MODE_OFF;
    ESP_LOGI("RELAY", "Relay %d turned OFF", relay_num + 1);
}

void relay_on_with_timer(const uint8_t relay_num, uint32_t seconds) {
    if (relay_num >= NUM_RELAYS) return;
    
    // Cap at maximum on time
    if (seconds > MAX_ON_TIME_SEC) {
        seconds = MAX_ON_TIME_SEC;
    }

    if (seconds == 0) {
        relay_off(relay_num);
        return;
    }

    // Turn on
    gpio_set_level(relay_pins[relay_num], 0);
    relay_modes[relay_num] = RELAY_MODE_TIMED;

    // Start/Restart timer
    if (relay_timers[relay_num]) {
        xTimerChangePeriod(relay_timers[relay_num], pdMS_TO_TICKS(seconds * 1000), 0);
        xTimerStart(relay_timers[relay_num], 0);
    }

    ESP_LOGI("RELAY", "Relay %d turned ON for %u seconds", relay_num + 1, (unsigned int)seconds);
}

relay_mode_t relay_get_mode(const uint8_t relay_num) {
    if (relay_num < NUM_RELAYS) return relay_modes[relay_num];
    return RELAY_MODE_OFF;
}

bool relay_get_state(const uint8_t relay_num) {
    if (relay_num < NUM_RELAYS) return relay_modes[relay_num] != RELAY_MODE_OFF;
    return false;
}

uint32_t relay_get_remaining_time(const uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS || !relay_timers[relay_num]) return 0;
    
    if (xTimerIsTimerActive(relay_timers[relay_num]) == pdFALSE) return 0;

    TickType_t expiry = xTimerGetExpiryTime(relay_timers[relay_num]);
    TickType_t now = xTaskGetTickCount();
    
    if (expiry > now) {
        return (expiry - now) / configTICK_RATE_HZ;
    }
    return 0;
}

void relay_toggle(const uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS) return;

    if (relay_modes[relay_num] != RELAY_MODE_OFF) {
        relay_off(relay_num);
    } else {
        relay_on(relay_num);
    }
}
