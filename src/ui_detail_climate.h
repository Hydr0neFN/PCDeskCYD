#pragma once

// Creates the reusable climate detail overlay (one screen, shared by all 3
// ACs). Call once during ui_manager_init(), before any ui_detail_climate_show().
void ui_detail_climate_init();

// Populates the overlay from entity_id's current state and loads it,
// remembering the screen that was active so the back button can return.
void ui_detail_climate_show(const char *entity_id);

// Call periodically (see main.cpp) alongside ui_manager_refresh(). No-op
// unless the climate detail screen is the one currently on screen --
// keeps the hvac/fan mode buttonmatrix in sync with HA (e.g. the power
// button's climate.toggle changes hvac_mode server-side, which this page
// wouldn't otherwise see until the next time it's reopened).
void ui_detail_climate_refresh();
