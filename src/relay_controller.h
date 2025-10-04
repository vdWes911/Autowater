#pragma once
#include <stdint.h>
#include <stdbool.h>

#define NUM_RELAYS 4

// Turn relay on/off
void relay_init(void);
void relay_on(uint8_t relay_num);
void relay_off(uint8_t relay_num);
bool relay_get_state(uint8_t relay_num);
