#pragma once

#include <lvgl.h>

// One tracked icon: the touchable color-block, the curated CJK name label
// under it (fixed text, set once at creation -- never HA's live
// friendly_name, which can contain characters outside our font subset, see
// HARDWARE_NOTES.md), and a status label below that for live ASCII-only
// state text (temp/mode/percentage -- always in-subset, no drift risk).
struct IconWidgets {
  lv_obj_t *icon;
  lv_obj_t *glyph;  // LV_SYMBOL_* centered inside icon; blank until an
                     // update_*_ui() sets it (climate leaves it blank --
                     // no built-in AC/thermometer glyph exists in LVGL)
  lv_obj_t *label;
  lv_obj_t *status_label;
};

// Creates all 3 room screens + nav bars, registers each icon against its
// entity_id for ui_manager_refresh(), and loads the 書房 screen.
void ui_manager_init();

// Call periodically (not every LVGL tick -- see main.cpp) to pull the
// latest entity state from ha_client and update icon color/label text.
// Only touches LVGL objects that actually changed value.
void ui_manager_refresh();

// Shared by each page's *_create(): a touchable placeholder icon (plain
// color block -- ui_manager_refresh() recolors it per entity state) with a
// CJK label below, wired to log "icon <entity_id> tapped" on press.
IconWidgets ui_manager_create_icon(lv_obj_t *screen, int x, int y, const char *label_text,
                                    const char *entity_id);

// Centered page title at the top of a screen.
void ui_manager_create_header(lv_obj_t *screen, const char *title);

// Shared by detail-page back/power/up/down buttons: a button showing an
// LV_SYMBOL_* icon (Montserrat only -- these glyphs aren't in the Noto
// subset, see HARDWARE_NOTES.md).
lv_obj_t *ui_manager_create_symbol_button(lv_obj_t *screen, int x, int y, int w, int h,
                                           const char *symbol, lv_event_cb_t cb);
