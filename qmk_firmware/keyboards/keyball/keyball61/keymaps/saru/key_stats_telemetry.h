#pragma once

#include "quantum.h"

// KeyStats QMK telemetry protocol v1 over the VIA Raw HID endpoint.
void key_stats_telemetry_record(uint16_t keycode, keyrecord_t *record);
void key_stats_telemetry_layer_state(layer_state_t state);
void key_stats_telemetry_task(void);
