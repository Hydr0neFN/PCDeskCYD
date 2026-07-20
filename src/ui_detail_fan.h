#pragma once

// Creates the fan detail overlay. Call once during ui_manager_init(),
// before any ui_detail_fan_show().
void ui_detail_fan_init();

// Populates the overlay from entity_id's current state and loads it,
// remembering the screen that was active so the back button can return.
void ui_detail_fan_show(const char *entity_id);

// Call periodically (see main.cpp) alongside ui_manager_refresh(). No-op
// unless the fan detail screen is the one currently on screen.
void ui_detail_fan_refresh();
