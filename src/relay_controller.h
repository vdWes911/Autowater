#pragma once
#include <stdint.h>
#include <stdbool.h>

#define NUM_RELAYS 4
#define MAX_ON_TIME_SEC 1200 // 20 minutes fallback
#define MAX_ROUTINE_STEPS 16

typedef enum {
    RELAY_MODE_OFF = 0,
    RELAY_MODE_MANUAL,
    RELAY_MODE_TIMED
} relay_mode_t;

typedef struct {
    uint8_t relay_id;
    uint16_t duration_sec;
    char name[32];
} routine_step_t;

typedef struct {
    char name[32];
    uint8_t current_step;
    uint8_t num_steps;
    bool is_running;
    routine_step_t steps[MAX_ROUTINE_STEPS];
} routine_state_t;

// Turn relay on/off
void relay_init(void);
void relay_on(uint8_t relay_num);
void relay_off(uint8_t relay_num);
void relay_on_with_timer(uint8_t relay_num, uint32_t seconds);
void relay_toggle(uint8_t relay_num);
relay_mode_t relay_get_mode(uint8_t relay_num);
bool relay_get_state(uint8_t relay_num);
uint32_t relay_get_remaining_time(uint8_t relay_num);

// Routine management
void relay_start_routine(const char* name, routine_step_t* steps, uint8_t num_steps);
void relay_stop_routine(void);
void relay_skip_routine_step(void);
routine_state_t* relay_get_routine_status(void);
