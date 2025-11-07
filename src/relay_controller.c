#include "relay_controller.h"

#include <esp_log.h>

#include "driver/gpio.h"

static const gpio_num_t relay_pins[NUM_RELAYS] = {(gpio_num_t) 5, (gpio_num_t) 6, (gpio_num_t) 7, (gpio_num_t) 10};
// Adjust your GPIOs
static bool relay_states[NUM_RELAYS] = {0};

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
        relay_states[i] = false;
    }

    gpio_config(&io_conf);
}

void relay_on(const uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS) return;

    gpio_set_level(relay_pins[relay_num], 0); // LOW = ON for active-low relays
    relay_states[relay_num] = true;
    ESP_LOGI("RELAY", "Relay %d turned ON", relay_num + 1);
}

void relay_off(const uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS) return;

    gpio_set_level(relay_pins[relay_num], 1); // HIGH = OFF for active-low relays
    relay_states[relay_num] = false;
    ESP_LOGI("RELAY", "Relay %d turned OFF", relay_num + 1);
}

bool relay_get_state(const uint8_t relay_num) {
    if (relay_num < NUM_RELAYS) return relay_states[relay_num];
    return false;
}

void relay_toggle(const uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS) return;

    if (relay_states[relay_num]) {
        relay_off(relay_num);
    } else {
        relay_on(relay_num);
    }
}
