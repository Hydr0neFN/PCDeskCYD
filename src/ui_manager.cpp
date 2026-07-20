#include "ui_manager.h"

#include <Arduino.h>

#include "ha_client.h"
#include "ui_detail_climate.h"
#include "ui_detail_fan.h"
#include "ui_page_bedroom.h"
#include "ui_page_kitchen.h"
#include "ui_page_study.h"

// Forward declarations -- defined further down, used by icon_tap_cb's
// optimistic light repaint below.
static void update_light_ui(IconWidgets w, const LightState *s);
static IconWidgets *find_widget(const char *entity_id);
static void suppress_refresh_for(const char *entity_id, unsigned long duration_ms);

static void icon_tap_cb(lv_event_t *e) {
  const char *entity_id = static_cast<const char *>(lv_event_get_user_data(e));
  Serial.printf("icon %s tapped\n", entity_id);

  size_t n = ha_client_entity_count();
  const EntityBinding *entities = ha_client_entities();
  for (size_t i = 0; i < n; i++) {
    if (strcmp(entities[i].entity_id, entity_id) != 0) continue;
    switch (entities[i].kind) {
      case EntityKind::LIGHT: {
        ha_call_service("light", "toggle", entity_id);

        // Optimistic paint: flip locally and repaint now instead of
        // waiting for the next 250ms ui_manager_refresh() tick. The real
        // subscribe_trigger event confirms (or corrects) this on arrival.
        LightState snap;
        ha_client_lock();
        snap = *static_cast<LightState *>(entities[i].state);
        ha_client_unlock();
        snap.is_on = !snap.is_on;
        IconWidgets *w = find_widget(entity_id);
        if (w) update_light_ui(*w, &snap);
        suppress_refresh_for(entity_id, 1000);
        break;
      }
      case EntityKind::CLIMATE:
        ui_detail_climate_show(entity_id);
        break;
      case EntityKind::FAN:
        ui_detail_fan_show(entity_id);
        break;
    }
    break;
  }
}

IconWidgets ui_manager_create_icon(lv_obj_t *screen, int x, int y, const char *label_text,
                                    const char *entity_id) {
  IconWidgets w;

  w.icon = lv_obj_create(screen);
  lv_obj_set_size(w.icon, 80, 80);
  lv_obj_set_pos(w.icon, x, y);
  lv_obj_set_style_radius(w.icon, 12, 0);
  lv_obj_set_style_bg_color(w.icon, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_style_border_width(w.icon, 0, 0);
  lv_obj_remove_flag(w.icon, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(w.icon, icon_tap_cb, LV_EVENT_CLICKED, (void *)entity_id);

  // Populated per entity type by update_light_ui()/update_fan_ui() -- no
  // built-in LV_SYMBOL exists for climate/AC, so update_climate_ui()
  // leaves this blank (see HARDWARE_NOTES.md).
  w.glyph = lv_label_create(w.icon);
  lv_obj_set_style_text_font(w.glyph, &lv_font_montserrat_28, 0);
  lv_obj_center(w.glyph);
  lv_label_set_text(w.glyph, "");

  w.label = lv_label_create(screen);
  lv_obj_set_style_text_font(w.label, &font_noto_20, 0);
  lv_obj_set_style_text_align(w.label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(w.label, 80);
  lv_obj_set_pos(w.label, x, y + 84);
  lv_label_set_text(w.label, label_text);

  // ASCII/degree only (temp, mode, on/off, percentage) -- Montserrat
  // covers it, no CJK font or subset risk here.
  w.status_label = lv_label_create(screen);
  lv_obj_set_style_text_font(w.status_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(w.status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(w.status_label, 80);
  lv_obj_set_pos(w.status_label, x, y + 106);
  lv_label_set_text(w.status_label, "");

  return w;
}

void ui_manager_create_header(lv_obj_t *screen, const char *title) {
  // Bordered box sized to hug the (bigger) title text top-to-bottom
  // (LV_SIZE_CONTENT + small padding) instead of a bare floating label.
  lv_obj_t *box = lv_obj_create(screen);
  lv_obj_set_width(box, 300);
  lv_obj_set_height(box, LV_SIZE_CONTENT);
  lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 4);
  lv_obj_set_style_border_width(box, 2, 0);
  lv_obj_set_style_radius(box, 8, 0);
  lv_obj_set_style_pad_all(box, 4, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *header = lv_label_create(box);
  lv_obj_set_style_text_font(header, &font_noto_28_bold, 0);
  lv_label_set_text(header, title);
  lv_obj_center(header);
}

lv_obj_t *ui_manager_create_symbol_button(lv_obj_t *screen, int x, int y, int w, int h,
                                           const char *symbol, lv_event_cb_t cb) {
  lv_obj_t *btn = lv_button_create(screen);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_label_set_text(lbl, symbol);
  lv_obj_center(lbl);
  return btn;
}

static void navbar_tab_cb(lv_event_t *e) {
  lv_obj_t *target_screen = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
  if (lv_screen_active() == target_screen) return;
  // No slide anim: this board has no TE (tearing-effect) pin wired (see
  // HARDWARE_NOTES.md), so a directional sweep always shows a visible
  // top-to-bottom tear. An instant switch has nothing for the tear to be
  // visible against.
  lv_screen_load(target_screen);
}

static void add_navbar(lv_obj_t *screen, lv_obj_t *study, lv_obj_t *bedroom, lv_obj_t *kitchen,
                        int active_index) {
  const char *labels[3] = {"書房", "次臥", "廚房"};
  lv_obj_t *targets[3] = {study, bedroom, kitchen};
  const int tab_w = 320 / 3;

  for (int i = 0; i < 3; i++) {
    lv_obj_t *btn = lv_button_create(screen);
    lv_obj_set_size(btn, tab_w, 30);
    lv_obj_set_pos(btn, i * tab_w, 210);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_bg_color(
        btn, lv_palette_main(i == active_index ? LV_PALETTE_BLUE : LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(btn, navbar_tab_cb, LV_EVENT_CLICKED, targets[i]);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &font_noto_20, 0);
    lv_label_set_text(lbl, labels[i]);
    lv_obj_center(lbl);
  }
}

// --- Live state binding (Milestone 5) ----------------------------------
// Entity registry: entity_id -> the IconWidgets pair created for it,
// populated once in ui_manager_init(). Matched by string against
// ha_client_entities() so it stays correct even if either list's order
// ever drifts independently.
struct EntityWidget {
  const char *entity_id;
  IconWidgets widgets;
};
static EntityWidget s_entity_widgets[7];
static int s_entity_widget_count = 0;

static void register_widget(const char *entity_id, IconWidgets w) {
  s_entity_widgets[s_entity_widget_count++] = {entity_id, w};
}

static IconWidgets *find_widget(const char *entity_id) {
  for (int j = 0; j < s_entity_widget_count; j++) {
    if (strcmp(s_entity_widgets[j].entity_id, entity_id) == 0) return &s_entity_widgets[j].widgets;
  }
  return nullptr;
}

// Optimistic-paint suppression: ui_manager_refresh() runs every 250ms from
// ha_client's struct, which may still hold the pre-tap value for a few
// hundred ms until the real subscribe_trigger event lands -- without this,
// refresh() stomps the just-painted optimistic color back to the stale
// one, then forward again once the real event arrives (visible flicker).
// Suppressing refresh writes for this entity for a short window lets the
// real event catch up silently. If it never arrives (failed call call),
// the window expires and refresh() reverts to the true state -- the
// "revert if HA disagrees" safety net from the original M6 design.
struct OptimisticSuppress {
  const char *entity_id;
  unsigned long until_ms;
};
static OptimisticSuppress s_suppress[7];

static void suppress_refresh_for(const char *entity_id, unsigned long duration_ms) {
  for (auto &s : s_suppress) {
    if (s.entity_id == nullptr || strcmp(s.entity_id, entity_id) == 0) {
      s.entity_id = entity_id;
      s.until_ms = millis() + duration_ms;
      return;
    }
  }
}

static bool is_refresh_suppressed(const char *entity_id) {
  for (auto &s : s_suppress) {
    if (s.entity_id && strcmp(s.entity_id, entity_id) == 0) {
      return millis() < s.until_ms;
    }
  }
  return false;
}

// Diff against LVGL's own current value instead of a separate cache --
// avoids redundant style/text writes (and the SPI flush + redraw they
// trigger) on every refresh tick when nothing actually changed.
static void set_label_text_if_changed(lv_obj_t *label, const char *new_text) {
  const char *cur = lv_label_get_text(label);
  if (cur && strcmp(cur, new_text) == 0) return;
  lv_label_set_text(label, new_text);
}

static void set_bg_color_if_changed(lv_obj_t *obj, lv_color_t color) {
  lv_color_t cur = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
  if (lv_color_eq(cur, color)) return;
  lv_obj_set_style_bg_color(obj, color, 0);
}

// None of these touch w.label (the curated CJK name, fixed at creation) --
// only w.icon's color and w.status_label's ASCII/degree text, both of which
// are safe against the font subset regardless of what HA's live
// friendly_name contains. See HARDWARE_NOTES.md.

// U+E9CA "Frigid" from Segoe Fluent Icons (C:\Windows\Fonts\SegoeIcons.ttf)
// -- closest built-in match for a cooling/AC glyph; LVGL's own LV_SYMBOL_*
// set has nothing thermostat/AC-shaped. Confirmed present in the font via
// fontTools before generating font_ac_28.c (see HARDWARE_NOTES.md).
#define AC_ICON "\xEE\xA7\x8A"

static void update_climate_ui(IconWidgets w, const ClimateState *s) {
  lv_color_t color;
  if (strcmp(s->state, "cool") == 0) {
    color = lv_color_hex(0x2196F3);
  } else if (strcmp(s->state, "heat") == 0) {
    color = lv_color_hex(0xF44336);
  } else {
    color = lv_color_hex(0x757575);
  }
  set_bg_color_if_changed(w.icon, color);
  lv_obj_set_style_text_font(w.glyph, &font_ac_28, 0);
  set_label_text_if_changed(w.glyph, AC_ICON);

  char buf[32];
  snprintf(buf, sizeof(buf), "%.0f°C %s", s->current_temp, s->state);
  set_label_text_if_changed(w.status_label, buf);
}

// U+EA80 "Lightbulb" from Segoe Fluent Icons (same source/verification as
// AC_ICON above). Only one glyph confirmed available (no separate on/off
// variant found in Microsoft's docs) -- color carries on/off state instead,
// same pattern as the fan icon's static LV_SYMBOL_LOOP.
#define LIGHT_ICON "\xEE\xAA\x80"

static void update_light_ui(IconWidgets w, const LightState *s) {
  lv_color_t color = lv_color_hex(s->is_on ? 0xFFC107 : 0x757575);
  set_bg_color_if_changed(w.icon, color);
  lv_obj_set_style_text_font(w.glyph, &font_light_28, 0);
  set_label_text_if_changed(w.glyph, LIGHT_ICON);
  set_label_text_if_changed(w.status_label, s->is_on ? "on" : "off");
}

static void update_fan_ui(IconWidgets w, const FanState *s) {
  lv_color_t color = lv_color_hex(s->is_on ? 0xFFC107 : 0x757575);
  set_bg_color_if_changed(w.icon, color);
  // Static for now -- no spin. This board has no TE pin (see
  // HARDWARE_NOTES.md); a continuously-animating icon risks the same
  // tearing the nav-slide animation had, running constantly instead of
  // just during a transition. Needs a single-icon hardware spike before
  // wiring into every fan/AC box.
  set_label_text_if_changed(w.glyph, LV_SYMBOL_LOOP);

  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", s->percentage);
  set_label_text_if_changed(w.status_label, buf);
}

void ui_manager_refresh() {
  size_t n = ha_client_entity_count();
  const EntityBinding *entities = ha_client_entities();

  for (size_t i = 0; i < n; i++) {
    IconWidgets *w = find_widget(entities[i].entity_id);
    if (!w) continue;

    switch (entities[i].kind) {
      case EntityKind::CLIMATE: {
        ClimateState snap;
        ha_client_lock();
        snap = *static_cast<ClimateState *>(entities[i].state);
        ha_client_unlock();
        update_climate_ui(*w, &snap);
        break;
      }
      case EntityKind::LIGHT: {
        if (is_refresh_suppressed(entities[i].entity_id)) break;
        LightState snap;
        ha_client_lock();
        snap = *static_cast<LightState *>(entities[i].state);
        ha_client_unlock();
        update_light_ui(*w, &snap);
        break;
      }
      case EntityKind::FAN: {
        FanState snap;
        ha_client_lock();
        snap = *static_cast<FanState *>(entities[i].state);
        ha_client_unlock();
        update_fan_ui(*w, &snap);
        break;
      }
    }
  }
}

void ui_manager_init() {
  ui_detail_climate_init();
  ui_detail_fan_init();

  StudyPageWidgets study = ui_page_study_create();
  BedroomPageWidgets bedroom = ui_page_bedroom_create();
  KitchenPageWidgets kitchen = ui_page_kitchen_create();

  register_widget("climate.ting", study.ac_icon);
  register_widget("light.aqara_smart_wall_switch_z1_pro_4", study.light1_icon);
  register_widget("light.aqara_smart_wall_switch_z1_pro_3", study.light2_icon);
  register_widget("climate.ci_wo", bedroom.ac_icon);
  register_widget("light.aqara_smart_wall_switch_z1_pro_16", bedroom.light_icon);
  register_widget("climate.chu", kitchen.ac_icon);
  register_widget("fan.xiaomi_p85_8f39_fan", kitchen.fan_icon);

  add_navbar(study.screen, study.screen, bedroom.screen, kitchen.screen, 0);
  add_navbar(bedroom.screen, study.screen, bedroom.screen, kitchen.screen, 1);
  add_navbar(kitchen.screen, study.screen, bedroom.screen, kitchen.screen, 2);

  lv_screen_load(study.screen);
}
