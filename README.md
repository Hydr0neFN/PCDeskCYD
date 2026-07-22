# PCDeskCYD
<img width="3024" height="2081" alt="image" src="https://github.com/user-attachments/assets/47d75915-9cf5-43b6-af75-a281262daafc" />

A 3-page touchscreen Home Assistant control panel running on a Cheap
Yellow Display (ESP32-2432S028), talking to HA over its native WebSocket
API with a REST fallback for initial state. Built with PlatformIO +
Arduino + LVGL 9 + TFT_eSPI.

## Features

- **3 room pages** (書房/次臥/廚房) with touch navigation, each showing
  climate, light, and fan entities as live-updating icon tiles
- **Real-time sync**: entity state changes in HA reflect on-screen within
  ~250ms via a scoped `subscribe_trigger` WebSocket subscription
- **Touch control**: toggle lights directly from the room page; tap a
  climate or fan icon to open a full detail page (temperature, HVAC mode,
  fan speed, oscillation, presets)
- **Optimistic UI**: taps repaint immediately, self-correcting from the
  next confirmed HA state if a call fails
- **CJK label support** via a generated Noto Sans TC font subset, mixed
  with Segoe Fluent Icons glyphs for light/AC iconography
- **Dual-core**: all WiFi/WebSocket/HTTP I/O runs on a dedicated FreeRTOS
  task pinned to core 0; LVGL/display stays exclusively on core 1

## Hardware

ESP32-2432S028 ("Cheap Yellow Display", CYD) -- 2.8" 320x240 ILI9341 TFT,
XPT2046 resistive touch, no PSRAM. See [HARDWARE_NOTES.md](HARDWARE_NOTES.md)
for the board-specific driver quirks, WebSocket frame-size limits, and
font pipeline gotchas discovered while bringing this board up -- worth
reading before touching display/touch/font code.

## Setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and fill in
   your WiFi credentials and a Home Assistant long-lived access token
   (**Profile -> Security -> Long-Lived Access Tokens** in HA).
2. Edit `include/config.h` with your HA host/port.
3. Edit the entity list in `src/ha_client.cpp` (`g_entities[]`) and the
   room page files (`src/ui_page_*.cpp`) to match your own HA entities --
   this repo's defaults are wired to one specific home's climate/light/fan
   setup and won't mean anything on a different HA instance.
4. Build and flash with [PlatformIO](https://platformio.org/):
   ```
   pio run -t upload
   ```

## Project structure

- [`PLAN.md`](PLAN.md) -- the original implementation plan, milestone by
  milestone (M1 display bring-up through M7 detail pages), kept updated
  as the actual implementation diverged from the initial design
- [`HARDWARE_NOTES.md`](HARDWARE_NOTES.md) -- board-specific findings:
  display driver selection, the WebSocket library's 15KB frame cap and
  why `get_states` doesn't work on this stack, CJK/icon font pipeline
  gotchas, screen-tearing constraints
- `src/ha_client.cpp` -- WiFi/WebSocket/REST, entity state, `call_service`
- `src/ui_manager.cpp` + `src/ui_page_*.cpp` -- room pages, nav, live
  state -> icon binding
- `src/ui_detail_climate.cpp` / `src/ui_detail_fan.cpp` -- full-screen
  control overlays

## License

MIT -- see [LICENSE](LICENSE).
