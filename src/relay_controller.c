#include "relay_controller.h"

#include <esp_log.h>

#include "driver/gpio.h"

static const gpio_num_t relay_pins[NUM_RELAYS] =  { 4, 5, 6, 7 }; // Adjust your GPIOs
static bool relay_states[NUM_RELAYS] = {0};

void relay_init(void) {
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 0,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    for (int i = 0; i < NUM_RELAYS; i++) {
        io_conf.pin_bit_mask |= (1ULL << relay_pins[i]);
        gpio_set_level(relay_pins[i], 0); // Start OFF
        relay_states[i] = false;
    }

    gpio_config(&io_conf);
}

void relay_on(uint8_t relay_num) {
    if (relay_num < NUM_RELAYS) {
        gpio_set_level(relay_pins[relay_num], 1);
        relay_states[relay_num] = true;
        ESP_LOGI("RELAY", "Relay %d turned ON", relay_num + 1);
    }
}

void relay_off(uint8_t relay_num) {
    if (relay_num < NUM_RELAYS) {
        gpio_set_level(relay_pins[relay_num], 0);
        relay_states[relay_num] = false;
        ESP_LOGI("RELAY", "Relay %d turned OFF", relay_num + 1);
    }
}

bool relay_get_state(uint8_t relay_num) {
    if (relay_num < NUM_RELAYS) return relay_states[relay_num];
    return false;
}
