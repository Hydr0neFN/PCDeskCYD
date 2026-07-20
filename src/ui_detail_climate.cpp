#include "ui_detail_climate.h"

#include <Arduino.h>
#include <lvgl.h>

#include "ha_client.h"
#include "ui_manager.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_return_screen = NULL;
static char s_entity_id[64] = "";
static float s_target_temp = 24;

static lv_obj_t *s_power_btn;
static lv_obj_t *s_current_temp_label;
static lv_obj_t *s_target_temp_label;
static lv_obj_t *s_hvac_matrix;
static lv_obj_t *s_fan_matrix;

// Suppresses periodic refresh's overwrite for a short window after any tap
// on this screen -- otherwise ui_detail_climate_refresh() (running every
// 250ms while this screen is active) re-populates from ha_client's struct
// before the real subscribe_trigger confirmation lands, visibly flickering
// the just-tapped control back to its pre-tap value and then forward again.
static unsigned long s_last_interaction_ms = 0;
static constexpr unsigned long INTERACTION_SUPPRESS_MS = 1000;

// Buttonmatrix label text (what's tapped) vs the HA service value it maps
// to -- these differ for a few entries (climate.ting's fan_mode/hvac_mode
// values confirmed via serial in M3/M5), so the label can never be passed
// straight to call_service. See HARDWARE_NOTES.md.
static const char *HVAC_LABELS[] = {"off", "cool", "dry", "fan", "auto", "heat", ""};
static const char *HVAC_VALUES[] = {"off", "cool", "dry", "fan_only", "auto", "heat"};
static constexpr int HVAC_COUNT = 6;

static const char *FAN_LABELS[] = {"auto", "silent", "low", "med", "high", ""};
static const char *FAN_VALUES[] = {"auto", "silent", "low", "medium", "high"};
static constexpr int FAN_COUNT = 5;

static int index_of(const char *const *values, int count, const char *value) {
  for (int i = 0; i < count; i++) {
    if (strcmp(values[i], value) == 0) return i;
  }
  return -1;
}

// Clears every button's checked flag, then checks exactly `idx` (if valid).
// lv_buttonmatrix's one-checked mode only enforces exclusivity on user
// clicks, not on programmatic lv_buttonmatrix_set_button_ctrl(), so this
// runs explicitly whenever the detail page is (re)populated from HA state.
static void set_checked_exclusive(lv_obj_t *matrix, int count, int idx) {
  for (int i = 0; i < count; i++) {
    lv_buttonmatrix_clear_button_ctrl(matrix, i, LV_BUTTONMATRIX_CTRL_CHECKED);
  }
  if (idx >= 0) {
    lv_buttonmatrix_set_button_ctrl(matrix, idx, LV_BUTTONMATRIX_CTRL_CHECKED);
  }
}

static void back_cb(lv_event_t *e) {
  if (s_return_screen) lv_screen_load(s_return_screen);
}

static void power_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  ha_call_service("climate", "toggle", s_entity_id);
}

static void set_target_temp(float t) {
  s_target_temp = t;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f°C", s_target_temp);
  lv_label_set_text(s_target_temp_label, buf);
}

static void temp_up_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  set_target_temp(s_target_temp < 32 ? s_target_temp + 1 : 32);
  ha_call_service_num("climate", "set_temperature", s_entity_id, "temperature", s_target_temp);
}

static void temp_down_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  set_target_temp(s_target_temp > 16 ? s_target_temp - 1 : 16);
  ha_call_service_num("climate", "set_temperature", s_entity_id, "temperature", s_target_temp);
}

static void hvac_matrix_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  uint32_t id = lv_buttonmatrix_get_selected_button(s_hvac_matrix);
  if (id >= (uint32_t)HVAC_COUNT) return;
  ha_call_service_str("climate", "set_hvac_mode", s_entity_id, "hvac_mode", HVAC_VALUES[id]);
}

static void fan_matrix_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  uint32_t id = lv_buttonmatrix_get_selected_button(s_fan_matrix);
  if (id >= (uint32_t)FAN_COUNT) return;
  ha_call_service_str("climate", "set_fan_mode", s_entity_id, "fan_mode", FAN_VALUES[id]);
}

void ui_detail_climate_init() {
  s_screen = lv_obj_create(NULL);

  ui_manager_create_symbol_button(s_screen, 4, 4, 48, 36, LV_SYMBOL_LEFT, back_cb);
  s_power_btn = ui_manager_create_symbol_button(s_screen, 320 - 52, 4, 48, 36, LV_SYMBOL_POWER, power_cb);

  s_current_temp_label = lv_label_create(s_screen);
  lv_obj_set_style_text_font(s_current_temp_label, &font_noto_28, 0);
  lv_obj_align(s_current_temp_label, LV_ALIGN_TOP_MID, 0, 42);

  ui_manager_create_symbol_button(s_screen, 40, 80, 44, 44, LV_SYMBOL_DOWN, temp_down_cb);
  ui_manager_create_symbol_button(s_screen, 320 - 84, 80, 44, 44, LV_SYMBOL_UP, temp_up_cb);

  s_target_temp_label = lv_label_create(s_screen);
  lv_obj_set_style_text_font(s_target_temp_label, &font_noto_28, 0);
  lv_obj_align(s_target_temp_label, LV_ALIGN_TOP_MID, 0, 86);

  // 44px tall (was 36 -- clipped the font-14 label text top/bottom).
  s_hvac_matrix = lv_buttonmatrix_create(s_screen);
  lv_buttonmatrix_set_map(s_hvac_matrix, HVAC_LABELS);
  lv_obj_set_size(s_hvac_matrix, 300, 44);
  lv_obj_set_pos(s_hvac_matrix, 10, 128);
  lv_obj_set_style_text_font(s_hvac_matrix, &lv_font_montserrat_14, 0);
  lv_buttonmatrix_set_button_ctrl_all(s_hvac_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(s_hvac_matrix, true);
  // Inline with the app's yellow-on/gray-off convention instead of the
  // theme's default checked-button accent color.
  lv_obj_set_style_bg_color(s_hvac_matrix, lv_color_hex(0xFFC107), ((lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED));
  lv_obj_add_event_cb(s_hvac_matrix, hvac_matrix_cb, LV_EVENT_VALUE_CHANGED, NULL);

  s_fan_matrix = lv_buttonmatrix_create(s_screen);
  lv_buttonmatrix_set_map(s_fan_matrix, FAN_LABELS);
  lv_obj_set_size(s_fan_matrix, 300, 44);
  lv_obj_set_pos(s_fan_matrix, 10, 176);
  lv_obj_set_style_text_font(s_fan_matrix, &lv_font_montserrat_14, 0);
  lv_buttonmatrix_set_button_ctrl_all(s_fan_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(s_fan_matrix, true);
  lv_obj_set_style_bg_color(s_fan_matrix, lv_color_hex(0xFFC107), ((lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED));
  lv_obj_add_event_cb(s_fan_matrix, fan_matrix_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void populate_from_snapshot(const ClimateState &snap) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f°C", snap.current_temp);
  lv_label_set_text(s_current_temp_label, buf);
  set_target_temp(snap.target_temp);

  set_checked_exclusive(s_hvac_matrix, HVAC_COUNT, index_of(HVAC_VALUES, HVAC_COUNT, snap.hvac_mode));
  set_checked_exclusive(s_fan_matrix, FAN_COUNT, index_of(FAN_VALUES, FAN_COUNT, snap.fan_mode));

  bool is_on = strcmp(snap.state, "off") != 0;
  lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(is_on ? 0xFFC107 : 0x757575), 0);
}

static bool fetch_snapshot(const char *entity_id, ClimateState *out) {
  size_t n = ha_client_entity_count();
  const EntityBinding *entities = ha_client_entities();
  for (size_t i = 0; i < n; i++) {
    if (strcmp(entities[i].entity_id, entity_id) != 0) continue;
    ha_client_lock();
    *out = *static_cast<ClimateState *>(entities[i].state);
    ha_client_unlock();
    return true;
  }
  return false;
}

void ui_detail_climate_show(const char *entity_id) {
  strlcpy(s_entity_id, entity_id, sizeof(s_entity_id));
  s_return_screen = lv_screen_active();

  ClimateState snap = {};
  if (fetch_snapshot(entity_id, &snap)) {
    populate_from_snapshot(snap);
  }

  lv_screen_load(s_screen);
}

void ui_detail_climate_refresh() {
  if (lv_screen_active() != s_screen) return;
  if (millis() - s_last_interaction_ms < INTERACTION_SUPPRESS_MS) return;
  ClimateState snap = {};
  if (fetch_snapshot(s_entity_id, &snap)) {
    populate_from_snapshot(snap);
  }
}
