#pragma once
#include <stdint.h>
#include <stdbool.h>

#define NUM_RELAYS 4
#define MAX_ON_TIME_SEC 1200 // 20 minutes fallback

typedef enum {
    RELAY_MODE_OFF = 0,
    RELAY_MODE_MANUAL,
    RELAY_MODE_TIMED
} relay_mode_t;

// Turn relay on/off
void relay_init(void);
void relay_on(uint8_t relay_num);
void relay_off(uint8_t relay_num);
void relay_on_with_timer(uint8_t relay_num, uint32_t seconds);
void relay_toggle(uint8_t relay_num);
relay_mode_t relay_get_mode(uint8_t relay_num);
bool relay_get_state(uint8_t relay_num);
uint32_t relay_get_remaining_time(uint8_t relay_num);
