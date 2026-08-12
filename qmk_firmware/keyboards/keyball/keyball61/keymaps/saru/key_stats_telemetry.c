#include QMK_KEYBOARD_H

#include "key_stats_telemetry.h"
#include "raw_hid.h"
#include "timer.h"
#include "via.h"

#include <string.h>

#define KEY_STATS_MAGIC_0 'K'
#define KEY_STATS_MAGIC_1 'S'
#define KEY_STATS_MAGIC_2 'T'
#define KEY_STATS_PROTOCOL_VERSION 1
#define KEY_STATS_REPORT_SIZE 32

#define KEY_STATS_MESSAGE_KEY_EVENT 1
#define KEY_STATS_MESSAGE_LAYER_STATE 2
#define KEY_STATS_MESSAGE_HEARTBEAT 3
#define KEY_STATS_MESSAGE_HOST_CONTROL 0x80

#define KEY_STATS_FLAG_PRESSED (1 << 0)
#define KEY_STATS_FLAG_TAP_RESOLVED (1 << 1)
#define KEY_STATS_FLAG_INTERRUPTED (1 << 2)
#define KEY_STATS_FLAG_DUAL_ROLE (1 << 3)
#define KEY_STATS_FLAG_LAYER_TAP (1 << 4)
#define KEY_STATS_FLAG_MOD_TAP (1 << 5)

#define KEY_STATS_CAPABILITIES 0x0F
#define KEY_STATS_HEARTBEAT_INTERVAL_MS 5000
#define KEY_STATS_HOST_TIMEOUT_MS 10000

static uint16_t key_stats_sequence;
static uint32_t key_stats_last_heartbeat;
static uint32_t key_stats_last_host_control;
static bool     key_stats_host_enabled;

static void key_stats_write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8);
}

static void key_stats_write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8);
    target[2] = (uint8_t)(value >> 16);
    target[3] = (uint8_t)(value >> 24);
}

static uint8_t key_stats_checksum(const uint8_t *packet) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < KEY_STATS_REPORT_SIZE - 1; ++i) {
        checksum ^= packet[i];
    }
    return checksum;
}

static bool key_stats_host_active(void) {
    return key_stats_host_enabled && TIMER_DIFF_32(timer_read32(), key_stats_last_host_control) < KEY_STATS_HOST_TIMEOUT_MS;
}

static uint32_t key_stats_event_time(const keyrecord_t *record) {
    uint32_t now        = timer_read32();
    uint32_t event_time = (now & 0xFFFF0000UL) | record->event.time;
    if ((uint16_t)now < record->event.time) {
        event_time -= 0x10000UL;
    }
    return event_time;
}

static void key_stats_send(uint8_t message_type, uint8_t flags, uint8_t source_layer, uint8_t active_layer, uint8_t row, uint8_t col, uint16_t keycode, uint8_t tap_count, uint32_t uptime, layer_state_t state) {
    uint8_t packet[KEY_STATS_REPORT_SIZE];
    memset(packet, 0, sizeof(packet));

    packet[0] = KEY_STATS_MAGIC_0;
    packet[1] = KEY_STATS_MAGIC_1;
    packet[2] = KEY_STATS_MAGIC_2;
    packet[3] = KEY_STATS_PROTOCOL_VERSION;
    packet[4] = message_type;
    packet[5] = flags;
    key_stats_write_u16(&packet[6], key_stats_sequence++);
    packet[8] = source_layer;
    packet[9] = active_layer;
    packet[10] = row;
    packet[11] = col;
    key_stats_write_u16(&packet[12], keycode);
    packet[14] = tap_count;
    packet[15] = get_mods() | get_oneshot_mods();
    key_stats_write_u32(&packet[16], uptime);
    key_stats_write_u32(&packet[20], (uint32_t)state);
    key_stats_write_u32(&packet[24], (uint32_t)default_layer_state);
    packet[28] = KEY_STATS_CAPABILITIES;
    packet[29] = MATRIX_ROWS;
    packet[30] = MATRIX_COLS;
    packet[31] = key_stats_checksum(packet);

    raw_hid_send(packet, KEY_STATS_REPORT_SIZE);
}

void key_stats_telemetry_record(uint16_t keycode, keyrecord_t *record) {
    if (!key_stats_host_active()) {
        return;
    }
    uint8_t flags = 0;
    if (record->event.pressed) {
        flags |= KEY_STATS_FLAG_PRESSED;
    }
    if (record->tap.count > 0) {
        flags |= KEY_STATS_FLAG_TAP_RESOLVED;
    }
    if (record->tap.interrupted) {
        flags |= KEY_STATS_FLAG_INTERRUPTED;
    }
    if (IS_QK_LAYER_TAP(keycode)) {
        flags |= KEY_STATS_FLAG_DUAL_ROLE | KEY_STATS_FLAG_LAYER_TAP;
    } else if (IS_QK_MOD_TAP(keycode)) {
        flags |= KEY_STATS_FLAG_DUAL_ROLE | KEY_STATS_FLAG_MOD_TAP;
    }

    uint8_t source_layer = record->event.pressed
                               ? layer_switch_get_layer(record->event.key)
                               : read_source_layers_cache(record->event.key);
    layer_state_t state = layer_state | default_layer_state;
    key_stats_send(KEY_STATS_MESSAGE_KEY_EVENT, flags, source_layer, get_highest_layer(state), record->event.key.row, record->event.key.col, keycode, record->tap.count, key_stats_event_time(record), state);
}

void key_stats_telemetry_layer_state(layer_state_t state) {
    if (!key_stats_host_active()) {
        return;
    }
    layer_state_t effective_state = state | default_layer_state;
    key_stats_send(KEY_STATS_MESSAGE_LAYER_STATE, 0, 0xFF, get_highest_layer(effective_state), 0xFF, 0xFF, 0, 0, timer_read32(), effective_state);
}

void key_stats_telemetry_task(void) {
    if (!key_stats_host_active()) {
        return;
    }
    uint32_t now = timer_read32();
    if (TIMER_DIFF_32(now, key_stats_last_heartbeat) < KEY_STATS_HEARTBEAT_INTERVAL_MS) {
        return;
    }
    key_stats_last_heartbeat = now;

    layer_state_t state = layer_state | default_layer_state;
    key_stats_send(KEY_STATS_MESSAGE_HEARTBEAT, 0, 0xFF, get_highest_layer(state), 0xFF, 0xFF, 0, 0, now, state);
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    if (length != KEY_STATS_REPORT_SIZE || data[0] != KEY_STATS_MAGIC_0 || data[1] != KEY_STATS_MAGIC_1 || data[2] != KEY_STATS_MAGIC_2 || data[3] != KEY_STATS_PROTOCOL_VERSION || data[4] != KEY_STATS_MESSAGE_HOST_CONTROL || data[31] != key_stats_checksum(data)) {
        return false;
    }

    key_stats_host_enabled      = data[5] != 0;
    key_stats_last_host_control = timer_read32();
    return true; // Fully handled. No response is required for the keep-alive command.
}
