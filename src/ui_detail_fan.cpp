#include "ui_detail_fan.h"

#include <Arduino.h>
#include <lvgl.h>

#include "ha_client.h"
#include "ui_manager.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_return_screen = NULL;
static char s_entity_id[64] = "";

static lv_obj_t *s_power_btn;
static lv_obj_t *s_speed_matrix;
static lv_obj_t *s_preset_matrix;
static lv_obj_t *s_osc_btn;
static lv_obj_t *s_osc_label;
static bool s_oscillating = false;

// Same purpose as ui_detail_climate.cpp's copy: suppresses periodic
// refresh's overwrite for a short window after any tap on this screen, so
// a real subscribe_trigger confirmation has time to catch up before the
// next 250ms refresh tick would otherwise flicker the tapped control back
// to its stale pre-tap value and then forward again.
static unsigned long s_last_interaction_ms = 0;
static constexpr unsigned long INTERACTION_SUPPRESS_MS = 1000;

// Buttonmatrix label text vs the HA service value it maps to -- 直吹/自然
// are our own short labels, HA's actual preset_mode strings are English
// ("Straight Wind"/"Natural Wind", confirmed via serial in M3/M5).
static const char *SPEED_LABELS[] = {"1", "2", "3", "4", ""};
static const int SPEED_VALUES[] = {25, 50, 75, 100};
static constexpr int SPEED_COUNT = 4;

static const char *PRESET_LABELS[] = {"直吹", "自然", ""};
static const char *PRESET_VALUES[] = {"Straight Wind", "Natural Wind"};
static constexpr int PRESET_COUNT = 2;

static int nearest_speed_index(int percentage) {
  int best = 0;
  int best_diff = 1000;
  for (int i = 0; i < SPEED_COUNT; i++) {
    int diff = abs(percentage - SPEED_VALUES[i]);
    if (diff < best_diff) {
      best_diff = diff;
      best = i;
    }
  }
  return best;
}

static int preset_index_of(const char *value) {
  for (int i = 0; i < PRESET_COUNT; i++) {
    if (strcmp(PRESET_VALUES[i], value) == 0) return i;
  }
  return -1;
}

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
  ha_call_service("fan", "toggle", s_entity_id);
}

static void speed_matrix_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  uint32_t id = lv_buttonmatrix_get_selected_button(s_speed_matrix);
  if (id >= (uint32_t)SPEED_COUNT) return;
  ha_call_service_num("fan", "set_percentage", s_entity_id, "percentage", SPEED_VALUES[id]);
}

static void preset_matrix_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  uint32_t id = lv_buttonmatrix_get_selected_button(s_preset_matrix);
  if (id >= (uint32_t)PRESET_COUNT) return;
  ha_call_service_str("fan", "set_preset_mode", s_entity_id, "preset_mode", PRESET_VALUES[id]);
}

static void set_osc_ui(bool on) {
  s_oscillating = on;
  lv_obj_set_style_bg_color(s_osc_btn, lv_color_hex(on ? 0xFFC107 : 0x757575), 0);
  lv_label_set_text(s_osc_label, on ? LV_SYMBOL_LOOP : LV_SYMBOL_STOP);
}

static void osc_cb(lv_event_t *e) {
  s_last_interaction_ms = millis();
  set_osc_ui(!s_oscillating);
  ha_call_service_bool("fan", "oscillate", s_entity_id, "oscillating", s_oscillating);
}

void ui_detail_fan_init() {
  s_screen = lv_obj_create(NULL);

  ui_manager_create_symbol_button(s_screen, 4, 4, 48, 36, LV_SYMBOL_LEFT, back_cb);
  s_power_btn = ui_manager_create_symbol_button(s_screen, 320 - 52, 4, 48, 36, LV_SYMBOL_POWER, power_cb);

  // 50px tall (was 40 -- clipped the font-20 button labels top/bottom).
  s_speed_matrix = lv_buttonmatrix_create(s_screen);
  lv_buttonmatrix_set_map(s_speed_matrix, SPEED_LABELS);
  lv_obj_set_size(s_speed_matrix, 240, 50);
  lv_obj_align(s_speed_matrix, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_set_style_text_font(s_speed_matrix, &lv_font_montserrat_20, 0);
  lv_buttonmatrix_set_button_ctrl_all(s_speed_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(s_speed_matrix, true);
  lv_obj_set_style_bg_color(s_speed_matrix, lv_color_hex(0xFFC107), ((lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED));
  lv_obj_add_event_cb(s_speed_matrix, speed_matrix_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // CJK glyphs in font_noto_20 run closer to the full em box than
  // Montserrat digits at the same size -- needs more height than the
  // speed matrix above or the text touches the button's top/bottom edge.
  s_preset_matrix = lv_buttonmatrix_create(s_screen);
  lv_buttonmatrix_set_map(s_preset_matrix, PRESET_LABELS);
  lv_obj_set_size(s_preset_matrix, 240, 60);
  lv_obj_align(s_preset_matrix, LV_ALIGN_TOP_MID, 0, 114);
  lv_obj_set_style_text_font(s_preset_matrix, &font_noto_20, 0);
  lv_buttonmatrix_set_button_ctrl_all(s_preset_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(s_preset_matrix, true);
  lv_obj_set_style_bg_color(s_preset_matrix, lv_color_hex(0xFFC107), ((lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED));
  lv_obj_add_event_cb(s_preset_matrix, preset_matrix_cb, LV_EVENT_VALUE_CHANGED, NULL);

  s_osc_btn = lv_button_create(s_screen);
  lv_obj_set_size(s_osc_btn, 160, 50);
  lv_obj_align(s_osc_btn, LV_ALIGN_TOP_MID, 0, 182);
  lv_obj_add_event_cb(s_osc_btn, osc_cb, LV_EVENT_CLICKED, NULL);
  s_osc_label = lv_label_create(s_osc_btn);
  lv_obj_set_style_text_font(s_osc_label, &lv_font_montserrat_20, 0);
  lv_obj_center(s_osc_label);
}

static void populate_from_snapshot(const FanState &snap) {
  set_checked_exclusive(s_speed_matrix, SPEED_COUNT, nearest_speed_index(snap.percentage));
  set_checked_exclusive(s_preset_matrix, PRESET_COUNT, preset_index_of(snap.preset_mode));
  set_osc_ui(snap.oscillating);
  lv_obj_set_style_bg_color(s_power_btn, lv_color_hex(snap.is_on ? 0xFFC107 : 0x757575), 0);
}

static bool fetch_snapshot(const char *entity_id, FanState *out) {
  size_t n = ha_client_entity_count();
  const EntityBinding *entities = ha_client_entities();
  for (size_t i = 0; i < n; i++) {
    if (strcmp(entities[i].entity_id, entity_id) != 0) continue;
    ha_client_lock();
    *out = *static_cast<FanState *>(entities[i].state);
    ha_client_unlock();
    return true;
  }
  return false;
}

void ui_detail_fan_show(const char *entity_id) {
  strlcpy(s_entity_id, entity_id, sizeof(s_entity_id));
  s_return_screen = lv_screen_active();

  FanState snap = {};
  if (fetch_snapshot(entity_id, &snap)) {
    populate_from_snapshot(snap);
  }

  lv_screen_load(s_screen);
}

void ui_detail_fan_refresh() {
  if (lv_screen_active() != s_screen) return;
  if (millis() - s_last_interaction_ms < INTERACTION_SUPPRESS_MS) return;
  FanState snap = {};
  if (fetch_snapshot(s_entity_id, &snap)) {
    populate_from_snapshot(snap);
  }
}
