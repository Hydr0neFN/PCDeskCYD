#pragma once

#include <stddef.h>

#include "ha_entities.h"

// Starts WiFi/HA WebSocket handling on its own FreeRTOS task pinned to
// core 0, so it never blocks the LVGL/display loop on core 1. All webSocket.*,
// WiFi.*, and HTTPClient calls live inside ha_client.cpp / this task —
// main.cpp must never call lv_* from anywhere other than the core-1 loop().
void ha_client_start();

// Short human-readable connection status for the on-screen label.
// Safe to read from core 1: always points to an immortal string literal,
// pointer-sized reads/writes are atomic on Xtensa, no lock needed.
const char *ha_client_status();

// Fixed-size, fixed-order snapshot of the 7 tracked entities. The array and
// its entity_id/kind fields never change after ha_client_start(); only the
// pointed-to Climate/Light/FanState contents are updated by the net task.
size_t ha_client_entity_count();
const EntityBinding *ha_client_entities();

// Guards every EntityBinding.state's contents (written by the net task on
// core 0, read by the LVGL loop on core 1). Take before reading/copying a
// state struct's fields, give immediately after -- keep the critical
// section tiny and never call lv_* while holding it.
void ha_client_lock();
void ha_client_unlock();

// Enqueues a call_service request; the net task (core 0) sends it on its
// next loop tick. Safe to call from core 1 (touch handlers) -- WebSocket
// I/O itself never happens outside ha_client.cpp's own task.
void ha_call_service(const char *domain, const char *service, const char *entity_id);

// Variants carrying one extra service_data key/value, for the M7 detail-page
// controls (set_temperature, set_hvac_mode, set_fan_mode, set_percentage,
// set_preset_mode, oscillate).
void ha_call_service_str(const char *domain, const char *service, const char *entity_id,
                          const char *key, const char *value);
void ha_call_service_num(const char *domain, const char *service, const char *entity_id,
                          const char *key, float value);
void ha_call_service_bool(const char *domain, const char *service, const char *entity_id,
                           const char *key, bool value);
