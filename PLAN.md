# ESP32-CYD Home Assistant Control Panel — Implementation Plan

## Target
ESP32-2432S028 (Cheap Yellow Display) -> 3-page touch UI -> HA WebSocket API -> control 7 entities.

## Hardware: ESP32-2432S028 (CYD)
- MCU: ESP32-WROOM-32 (no PSRAM — buffer sizing critical)
- Display: 2.8" 320x240 ILI9341 TFT (HSPI bus)
- Touch: XPT2046 resistive (VSPI bus — SEPARATE SPI from display, classic trap)
- Backlight: GPIO 21 (PWM)
- **Canonical pin source**: github.com/witnessmenow/ESP32-Cheap-Yellow-Display (the de-facto CYD reference repo). Sonnet MUST cross-check pins from that repo's config, not this table alone.
- Known CYD pin map (2432S028, verify against witnessmenow repo + your board variant):

| Function | Pin | Notes |
|----------|-----|-------|
| TFT MOSI | 13 | HSPI bus |
| TFT MISO | 12 | HSPI bus (some docs call this RST — it's NOT) |
| TFT SCLK | 14 | HSPI bus |
| TFT CS | 15 | |
| TFT DC | 2 | |
| TFT RST | -1 | Most 2432S028 boards have no RST pin exposed — use -1 |
| TFT BL | 21 | PWM for backlight brightness |
| Touch CLK | 25 | VSPI / separate SPI — NOT shared with display |
| Touch MOSI | 32 | |
| Touch MISO | 39 | |
| Touch CS | 33 | |
| Touch IRQ | 36 | |
| RGB LED | R=4 G=16 B=17 | |

**IMPORTANT**: `LGFX_AUTODETECT` does NOT cover CYD (panel ID can't be read). Must use manual LGFX class config. Many 2432S028 panels need `invertColors(true)` and/or BGR color order — first thing to try if colors look wrong.

## Tech Stack

| Layer | Library | Why |
|-------|---------|-----|
| Display driver | **TFT_eSPI** (`ILI9341_2_DRIVER`) | Switched from originally-planned LovyanGFX after M1 bring-up — see [HARDWARE_NOTES.md](HARDWARE_NOTES.md). This board's ILI9341 clone needs the `_2` register variant; LovyanGFX's `Panel_ILI9341_2` also exists but its rotation/MADCTL combo was never fully resolved, while TFT_eSPI matched the proven vendor reference examples directly. |
| Touch driver | XPT2046_Touchscreen (PaulStoffregen) | Separate VSPI bus from display, matches vendor reference |
| UI framework | LVGL 9.x | Multi-page, touch events, arc/slider widgets for climate. Wired to TFT_eSPI via a hand-written flush_cb, not `lv_tft_espi_create()` (unverified on this board) |
| WebSocket | ArduinoWebSockets (by Links2004) | Stable ESP32 WS client |
| JSON | ArduinoJson 7 | Filter feature = parse only needed entities, saves RAM |
| Framework | Arduino (PlatformIO) | As specified |

**Fallback note**: If CYD display/touch bring-up becomes a quagmire, ESPHome + LVGL YAML is the escape hatch — it eliminates connection/auth/state-sync/reconnect complexity entirely. Only consider if Milestone 1 fails after reasonable effort. (M1 did hit exactly this quagmire on the display layer — resolved by switching drivers rather than escaping to ESPHome; see HARDWARE_NOTES.md.)

## Entity Map (from HA, verified 2026-07-20)

| Entity ID | HA Name | Type | Page |
|-----------|---------|------|------|
| `climate.ting` | 廳 | Hitachi AC | 書房 |
| `light.aqara_smart_wall_switch_z1_pro_4` | 書房吊燈 | on/off light | 書房 |
| `light.aqara_smart_wall_switch_z1_pro_3` | 書房坎燈 | on/off light | 書房 |
| `climate.ci_wo` | 次臥 | Hitachi AC | 次臥 |
| `light.aqara_smart_wall_switch_z1_pro_16` | 次臥燈 | on/off light | 次臥 |
| `climate.chu` | 廚 | Hitachi AC | 廚房 |
| `fan.xiaomi_p85_8f39_fan` | 電風扇 (display name, HA name is 米家電風扇 风扇) | Xiaomi fan | 廚房 |

### Climate capabilities (all 3 identical — Hitachi via jcihitachi_tw)
- hvac_modes: `off`, `cool`, `dry`, `fan_only`, `auto`, `heat`
- temp range: 16-32 C, step 1
- fan_modes: `auto`, `silent`, `low`, `medium`, `high`
- preset_modes: `none`, `eco`, `Mold Prev`, `Eco & Mold Prev`, `boost`
- swing_modes: 12 options (vertical, horizontal, both, directional combos)

### Fan capabilities (Xiaomi P85 via xiaomi_miot)
- percentage: 0-100, step 25 (4 levels)
- preset_modes: `Straight Wind`, `Natural Wind`
- oscillation: bool
- speed_list: Level1-4

## Page Structure (320x240, landscape)

### Navigation
- Bottom nav bar (30px height): 3 dots/tabs for page switching, always visible
- Active page highlighted
- Touch targets >= 48x48px throughout (resistive touch = imprecise)

### Page 1: 書房 (home page)
```
+----------------------------------+
|          書  房          (header) |
|  [AC icon]  [吊燈 icon] [坎燈 icon] |
|   廳 27C     書房吊燈    書房坎燈   |
|   (cool)     (on/off)   (on/off)  |
|                                    |
|         [ . ]  .    .   (nav bar) |
+----------------------------------+
```
- AC icon: shows current_temp + state (cool/off/etc). Color = blue(cool)/red(heat)/gray(off)
- Light icons: bulb icon, filled=on / outline=off. Tap = toggle
- Tap AC icon -> Climate Detail page

### Page 2: 次臥
```
+----------------------------------+
|          次  臥          (header) |
|     [AC icon]    [燈 icon]       |
|      次臥 29C     次臥燈          |
|       (off)      (on/off)        |
|                                    |
|           .   [ . ]  .   (nav bar)|
+----------------------------------+
```

### Page 3: 廚房
```
+----------------------------------+
|          廚  房          (header) |
|     [AC icon]    [Fan icon]      |
|       廚 30C      電風扇          |
|       (off)       (Lv2)          |
|                                    |
|           .    .  [ . ]   (nav bar)|
+----------------------------------+
```

### Climate Detail Page (overlay, any of 3 ACs)
```
+----------------------------------+
|  [<back]    廳              [pwr] |
|                                    |
|         current: 27 C             |
|           [ ^ ]                    |
|      target: [ 27 ] C             |
|           [ v ]                    |
|                                    |
|  mode: [cool] dry fan auto heat   |
|  fan:  [auto] silent low med high |
|                                    |
+----------------------------------+
```
- `[<back]` returns to previous page
- `[pwr]` = power toggle (climate.toggle service). Icon color reflects state
- Up/down arrows adjust target temp (1C step, range 16-32)
- Mode row: horizontal scrollable chips, active one highlighted
- Fan row: same chip style
- Preset/swing: OMIT from v1 (too many swing options, screen too small). Can add later

### Fan Detail Page (overlay)
```
+----------------------------------+
|  [<back]   電風扇           [pwr] |
|                                    |
|         speed:                     |
|    [1]  [2]  [3]  [4]             |
|                                    |
|    mode: [Straight Wind] [Natural Wind] |
|                                    |
|    oscillation: [  ON  ]           |
|                                    |
+----------------------------------+
```
- 4 speed buttons (percentage: 25/50/75/100)
- 2 preset mode chips
- Oscillation toggle

## File Structure

**Note (post-M1)**: display/touch init ended up inline in `main.cpp` via
TFT_eSPI, not a separate `display_setup.cpp/.h` LovyanGFX module as planned
below — see [HARDWARE_NOTES.md](HARDWARE_NOTES.md). Split it back out once
M2+ starts adding enough to `main.cpp` to warrant it.

```
ESP32CYD/
  platformio.ini          -- board config, lib_deps, build_flags
  include/
    secrets.h             -- WiFi SSID/pass, HA token (GITIGNORED)
    config.h              -- entity IDs, pin defs, HA host/port
    ha_entities.h         -- entity state structs
  src/
    main.cpp              -- setup/loop, WiFi connect, orchestration
    display_setup.cpp/.h  -- LovyanGFX init, LVGL display/touch driver
    ha_client.cpp/.h      -- WS connect, auth, subscribe, call_service, reconnect
    ui_manager.cpp/.h     -- LVGL screen management, page creation, navigation
    ui_page_study.cpp/.h  -- 書房 page widgets
    ui_page_bedroom.cpp/.h -- 次臥 page widgets
    ui_page_kitchen.cpp/.h -- 廚房 page widgets
    ui_detail_climate.cpp/.h -- climate detail overlay (reusable for all 3 ACs)
    ui_detail_fan.cpp/.h  -- fan detail overlay
    icons.c               -- LVGL image data (simple built-in symbols preferred over custom)
```

## HA WebSocket Protocol (verified from HA developer docs)

### Auth handshake
```
1. Connect ws://192.168.1.10:8123/api/websocket
2. Server sends: {"type":"auth_required","ha_version":"..."}
3. Client sends: {"type":"auth","access_token":"<LONG_LIVED_TOKEN>"}
4. Server sends: {"type":"auth_ok",...} or {"type":"auth_invalid",...}
```

### Subscribe to state changes
```json
{"id":1,"type":"subscribe_events","event_type":"state_changed"}
```
Server responds with `{"type":"result","success":true}`, then streams events.
Filter client-side: only process events where `entity_id` matches our 7 entities.

### Call service (climate example)
```json
{"id":2,"type":"call_service","domain":"climate","service":"set_temperature",
 "service_data":{"entity_id":"climate.ting","temperature":25}}
```
```json
{"id":3,"type":"call_service","domain":"climate","service":"set_fan_mode",
 "service_data":{"entity_id":"climate.ting","fan_mode":"low"}}
```
```json
{"id":4,"type":"call_service","domain":"climate","service":"toggle",
 "service_data":{"entity_id":"climate.ting"}}
```
```json
{"id":5,"type":"call_service","domain":"climate","service":"set_hvac_mode",
 "service_data":{"entity_id":"climate.ting","hvac_mode":"cool"}}
```

### Call service (light toggle)
```json
{"id":6,"type":"call_service","domain":"light","service":"toggle",
 "service_data":{"entity_id":"light.aqara_smart_wall_switch_z1_pro_4"}}
```

### Call service (fan)
```json
{"id":7,"type":"call_service","domain":"fan","service":"set_percentage",
 "service_data":{"entity_id":"fan.xiaomi_p85_8f39_fan","percentage":50}}
```
```json
{"id":8,"type":"call_service","domain":"fan","service":"set_preset_mode",
 "service_data":{"entity_id":"fan.xiaomi_p85_8f39_fan","preset_mode":"Natural Wind"}}
```
```json
{"id":9,"type":"call_service","domain":"fan","service":"oscillate",
 "service_data":{"entity_id":"fan.xiaomi_p85_8f39_fan","oscillating":true}}
```

### Fetch initial states (on connect, after WiFi up) — REVISED, verified on hardware

**`get_states` over the WS connection does not work on this stack — do not
use it.** `links2004/WebSockets` hard-caps incoming frames at 15KB
(`WEBSOCKETS_MAX_DATA_SIZE`, not overridable without patching the vendored
library — see HARDWARE_NOTES.md). A real HA instance's `get_states` dump is
100KB+, so every request triggers an instant `clientDisconnect(...,1009)`
before any JSON ever reaches ArduinoJson — the filter mitigation below
doesn't help because there are no bytes to filter.

**Actual approach (implemented + confirmed in M3)**:
- Initial fetch: 7 individual REST `GET /api/states/<entity_id>` calls via
  `HTTPClient` (plain HTTP, no WS frame-size limit; `Authorization: Bearer
  <HA_Token>` header).
- Live updates: WS `subscribe_trigger` with a `state` platform trigger and
  an `entity_id` array listing our 7 IDs (**not** `subscribe_events` with
  `event_type: state_changed` — that's a firehose over every HA entity, and
  a single untracked large entity, e.g. a `weather.*` forecast, can still
  exceed 15KB and trip the same disconnect). `subscribe_trigger` is
  server-scoped, so payload size is bounded regardless of what else exists
  on the HA instance.

## Implementation Milestones (serial — each must pass before next)

### Milestone 1: Display + Touch bring-up (NO WiFi, NO HA)

**Goal**: Prove hardware works. Draw something, tap something, see serial log.

1. Configure `platformio.ini`:
   ```ini
   [env:esp32doit-devkit-v1]
   platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
   board = esp32doit-devkit-v1
   framework = arduino
   monitor_speed = 115200
   board_build.f_cpu = 240000000L
   board_build.flash_mode = dio
   lib_deps =
     lovyan03/LovyanGFX@^1.2.0
     lvgl/lvgl@^9.1.0
     links2004/WebSockets@^2.4.0
     bblanchon/ArduinoJson@^7
   build_flags =
     -D LV_CONF_INCLUDE_SIMPLE
     -I include
   ```
   **IMPORTANT**: Use a proper `lv_conf.h` file in `include/` — NOT build_flags-only. LVGL 9 changed tick mechanism to `lv_tick_set_cb()` (the old `LV_TICK_CUSTOM` is LVGL 8 era). Sonnet should verify the exact LVGL 9.1 conf mechanism via context7 before writing lv_conf.h. Start from a known-good LVGL 9 + ESP32 template, enable only needed widgets.
   
   Key lv_conf.h settings:
   - `LV_USE_LOG 1` + `LV_LOG_LEVEL LV_LOG_LEVEL_INFO`
   - Enable: LV_USE_BUTTON, LV_USE_LABEL, LV_USE_BUTTONMATRIX, LV_USE_IMAGE
   - Disable unused: LV_USE_LIST, LV_USE_CHART, LV_USE_TABLE, etc.
   - Font: LV_FONT_MONTSERRAT_14/20 (for ASCII). CJK = custom font files (see CJK section)
   - Color: LV_COLOR_DEPTH 16

2. Write `display_setup.cpp`: LovyanGFX config for CYD
   - HSPI bus for ILI9341 (pins per table above)
   - Separate VSPI/software SPI for XPT2046 touch
   - LVGL display driver: `lv_display_create(320, 240)` + flush callback
   - LVGL touch driver: `lv_indev_create()` + read callback
   - Draw buffer: **partial buffer** ~320x24 (15,360 bytes) — no PSRAM, don't allocate full-frame
   - Touch calibration: XPT2046 often needs axis swap + invert on CYD. Log raw touch coords first, calibrate

3. Write minimal `main.cpp`:
   - Init serial 115200
   - Init LovyanGFX display
   - Init LVGL
   - Create one button centered on screen with label "TOUCH ME"
   - On tap: log "Button pressed at (x,y)" to serial + change button color

4. Flash + open serial monitor

**API NOTE**: LVGL 9 API differs significantly from v8. DO NOT use `lv_disp_draw_buf_init` (v8) — v9 uses `lv_display_create()` + `lv_display_set_flush_cb()`. Button events use `lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL)` not the old `lv_button_set_action`. Verify every LVGL call against v9.1 docs via context7 (`/websites/lvgl_io_open_9_1`). Touch input: `lv_indev_create()` + `lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER)` + `lv_indev_set_read_cb(indev, touch_read_cb)`.

**GATE**: Screen shows button. Touch logs coordinates. Proceed only when this works.
**Common failures**: White screen = wrong SPI pins or driver. Garbled = wrong rotation/color order. Touch inverted = axis swap needed. Touch unresponsive = wrong SPI bus config (must be separate from display SPI).

---

### Milestone 2: WiFi + WebSocket connect + HA auth

**Goal**: Serial prints "HA authenticated" after connecting.

1. Create `include/secrets.h`:
   ```cpp
   #pragma once
   #define WIFI_SSID "YOUR_SSID"
   #define WIFI_PASS "YOUR_PASS"
   #define HA_TOKEN "YOUR_LONG_LIVED_ACCESS_TOKEN"
   ```
   Add `include/secrets.h` to `.gitignore`.

2. Create `include/config.h`:
   ```cpp
   #pragma once
   #define HA_HOST "192.168.1.10"
   #define HA_PORT 8123
   #define HA_WS_URL "ws://192.168.1.10:8123/api/websocket"
   ```

3. Write `ha_client.cpp/.h`:
   - WiFi connect with retry (show "Connecting..." on display via LVGL label)
   - WebSocket connect to `HA_WS_URL`
   - Auth handshake: wait for `auth_required` -> send `auth` with token -> check `auth_ok`
   - Log result to serial
   - **Reconnection logic**: on WS disconnect or WiFi drop -> auto-reconnect with backoff (1s, 2s, 4s, max 30s). Display shows "Reconnecting..." overlay

4. Update `main.cpp`: init WiFi + HA client after display init

**GATE**: Serial shows "WiFi connected: <IP>" then "HA WebSocket authenticated". Screen shows connection status.

---

### Milestone 3: Fetch + parse entity states

**Goal**: All 7 entity states logged to serial after auth.

1. On WiFi connect (before/independent of WS auth), fetch each of the 7
   entities via REST `GET /api/states/<entity_id>` (`HTTPClient`) — **not**
   WS `get_states`, which hard-disconnects on this stack (see
   HARDWARE_NOTES.md and the revised "Fetch initial states" section above)
2. Parse each REST response with ArduinoJson:
   - Add `bblanchon/ArduinoJson@^7` to lib_deps
   - Populate entity state structs defined in `ha_entities.h`:
     ```cpp
     struct ClimateState {
       char entity_id[64];
       char friendly_name[32];
       char state[16];        // "off", "cool", "heat", etc.
       float current_temp;
       float target_temp;
       char fan_mode[16];
       char hvac_mode[16];
     };
     struct LightState {
       char entity_id[64];
       char friendly_name[32];
       bool is_on;
     };
     struct FanState {
       char entity_id[64];
       char friendly_name[32];
       bool is_on;
       int percentage;        // 0-100
       char preset_mode[32];
       bool oscillating;
     };
     ```
3. Send `subscribe_trigger` with a `state` platform trigger scoped to our 7
   entity_ids (`{"trigger":{"platform":"state","entity_id":[...7 ids...]}}`)
4. On incoming trigger event, read `event.variables.trigger.{entity_id,to_state}`
   (not `event.data.new_state` — that's `subscribe_events`' shape), match
   entity_id against our 7, update struct, log to serial

**GATE**: Serial shows all 7 entities with correct friendly names, states, and attributes. Change a light in HA app -> serial logs the state_changed event within 1-2s.

---

### Milestone 4: Static UI — 3 pages + navigation

**Goal**: 3 swipeable/tabbable pages with correct layout, no live data yet.

1. Write `ui_manager.cpp/.h`:
   - Create 3 LVGL screens (one per page)
   - Bottom nav bar: 3 tab buttons (書房/次臥/廚房) - `lv_button` row at y=210, height 30px
   - Page switch: `lv_screen_load_anim()` with slide animation
   - Status bar (optional, top 20px): WiFi icon + "Connected" / "Disconnected"

2. Write each page file with placeholder icons/labels:
   - `ui_page_study.cpp`: 3 icons in a row (AC + 2 lights), labels below each
   - `ui_page_bedroom.cpp`: 2 icons centered (AC + light)
   - `ui_page_kitchen.cpp`: 2 icons centered (AC + fan)

3. Use LVGL built-in symbols where possible:
   - AC: `LV_SYMBOL_CHARGE` or custom thermometer (can use simple colored circle for v1)
   - Light: `LV_SYMBOL_EYE_OPEN` (on) / `LV_SYMBOL_EYE_CLOSE` (off) — or custom
   - Fan: `LV_SYMBOL_REFRESH` (spinning feel)
   - Consider: Montserrat 20 for labels, 28 for temps, 14 for status text

4. Icon touch areas: 80x80px minimum, even if icon itself is smaller

**GATE**: All 3 pages render correctly. Tab navigation works. Touch on icons logs "icon X tapped" to serial.

---

### Milestone 5: Live state -> UI binding

**Goal**: Entity states from HA reflected in real-time on screen.

1. Create update functions per entity type:
   - `update_climate_ui(page, ClimateState*)`: set temp label, state color, mode text
   - `update_light_ui(page, LightState*)`: toggle icon fill/outline + color
   - `update_fan_ui(page, FanState*)`: set speed label, state color

2. On initial `get_states` response: populate all UI elements
3. On `state_changed` event: find matching entity -> update struct -> call UI update fn
4. Color scheme:
   - AC cool = blue (#2196F3), heat = red (#F44336), off = gray (#757575)
   - Light on = yellow (#FFC107), off = gray
   - Fan on = green (#4CAF50), off = gray

**GATE**: Toggle a light in HA app -> CYD screen updates within 1-2s. Change AC temp in HA -> CYD shows new temp.

---

### Milestone 6: Controls — call_service from touch

**Goal**: Tapping icons on CYD actually controls HA entities.

1. Light icons: tap = `call_service` light.toggle
2. AC/fan icons: tap = open detail page (Milestone 7)
3. `ha_call_service(domain, service, entity_id)` in `ha_client` -- touch
   handlers run on core 1 (LVGL), WebSocket I/O must stay on core 0, so
   this enqueues a request onto a `QueueHandle_t` instead of calling
   `webSocket.sendTXT()` directly; the net task drains it each loop tick
4. Maintain incrementing message `id` for WS requests
5. **Simplified from the original optimistic-update-with-3s-revert
   design**: `ui_manager_refresh()` already re-applies the authoritative
   struct value from `ha_client` every 250ms regardless of what triggered
   it, so a failed/no-op service call naturally "reverts" on its own next
   tick with no separate timer/state machine needed. Trade-off: up to
   ~250ms between tap and visual confirmation (vs. instant optimistic
   paint), acceptable given observed WS round-trip is well under 1s on
   this LAN. Revisit with real optimistic-paint-then-verify if perceived
   latency becomes an issue.

**GATE**: Tap light icon on CYD -> light toggles in HA (verify in HA app). Response < 1s.
Confirmed on hardware: 書房吊燈 toggled via touch, verified in HA app.

---

### Milestone 7: Climate + Fan detail pages

**Goal**: Full climate and fan control overlays.

1. `ui_detail_climate.cpp/.h` — reusable for all 3 ACs:
   - Takes `ClimateState*` pointer
   - Creates LVGL screen overlay (full 320x240)
   - Back button (top-left, `LV_SYMBOL_LEFT`) -> `lv_screen_load()` previous page
   - Power button (top-right, `LV_SYMBOL_POWER`) -> `climate.toggle` service
   - Current temp display (read-only, large font 28)
   - Target temp: value label (font 28) + up arrow btn + down arrow btn
     - Up: `set_temperature` with target+1 (clamped to 32)
     - Down: `set_temperature` with target-1 (clamped to 16)
   - HVAC mode row: `lv_buttonmatrix` with ["off","cool","dry","fan","auto","heat"]
     - Active mode = checked state
     - Tap -> `set_hvac_mode` service
   - Fan mode row: `lv_buttonmatrix` with ["auto","silent","low","med","high"]
     - Active = checked
     - Tap -> `set_fan_mode` service
   - NO preset/swing in v1 (12 swing options = unusable on 320x240)

2. `ui_detail_fan.cpp/.h`:
   - Back + power buttons (same layout as climate)
   - Speed: 4 buttons [1][2][3][4] -> `fan.set_percentage` (25/50/75/100)
   - Preset: 2 buttons [直吹][自然] -> `fan.set_preset_mode`
   - Oscillation: toggle button -> `fan.oscillate`
   - All buttons show active state from FanState struct

3. Wire page icons: tap AC icon on any page -> `show_climate_detail(&climate_states[index])`

**GATE**: Open climate detail from 書房 -> change temp via arrows -> HA reports new temp. Change fan speed -> fan responds. Back button returns to correct page.

---

## Prerequisites (user must provide before flashing)

1. **WiFi SSID + password** -> put in `include/secrets.h`
2. **HA Long-Lived Access Token** -> Profile -> Security -> Create Token -> put in `secrets.h`
3. **USB cable** connected to CYD, correct COM port in PlatformIO
4. **Verify board variant**: check if your CYD is USB-C or micro-USB version (pin map may differ slightly for RST pin). If display shows white after Milestone 1 flash, try `TFT_RST = -1` instead of 12.

## CYD Chinese Font Support

The friendly names contain CJK characters (廳, 書房吊燈, 次臥, etc.). LVGL's built-in Montserrat fonts don't include CJK.

**Approach**: Use LVGL's font converter to generate a subset font containing only the specific characters needed:
- CJK characters needed: `書房吊燈坎次臥廚電風扇廳` (11 unique CJK chars)
- Also include: full ASCII range (digits, letters for mode names like "cool", "auto", "Straight Wind", "Natural Wind", temp values), degree symbol
- Source: Noto Sans TC (free, Traditional Chinese) — merge with Montserrat for Latin range
- Generate at sizes 20 (labels), 28 (temp display) — two sizes enough
- Output: `font_noto_20.c`, `font_noto_28.c` — include in `src/`
- Tool: `lv_font_conv` CLI (npm package) or LVGL online font converter
- **The font subset must contain EVERY character rendered on screen**. If you add new labels, update the subset.

**Alternative simpler approach**: Use LovyanGFX's built-in `setFont(&fonts::efontCN_14)` for Chinese text, and LVGL only for non-CJK widgets. But mixing rendering layers = messy. Prefer LVGL-native font.

## Memory Budget (ESP32, no PSRAM)

| Item | Estimate |
|------|----------|
| LVGL draw buffer (320x24x2) | ~15 KB |
| LVGL widgets (3 pages + 2 detail) | ~20 KB |
| ArduinoJson doc (filtered) | ~4 KB |
| WebSocket buffer | ~4 KB |
| CJK font subsets (3 sizes) | ~30 KB |
| Entity state structs | ~1 KB |
| Stack + overhead | ~40 KB |
| **Total estimated** | **~114 KB** |
| **Available heap** | **~300 KB** |

Should fit comfortably. Monitor with `ESP.getFreeHeap()` in serial log.

## Reconnection Strategy

- WiFi lost -> retry every 5s, show "WiFi disconnected" overlay on screen
- WS disconnected (WiFi OK) -> reconnect with exponential backoff (1s, 2s, 4s, ..., max 30s)
- HA restart -> WS will drop, auto-reconnect catches it
- After reconnect: re-auth + re-subscribe + `get_states` to resync all entities
- Never block LVGL loop during reconnect (use non-blocking WS library)

## Scope Boundaries

**In scope (v1):**
- 3 room pages with nav
- Light toggle (tap icon)
- Climate detail: power, temp up/down, hvac mode, fan mode
- Fan detail: power, 4-speed, preset, oscillation
- Real-time state sync via WS
- Auto-reconnect WiFi + WS
- CJK font for entity names

**Out of scope (v1):**
- Swing mode control (12 options, unmanageable on 320x240)
- Climate preset modes (eco/boost — low priority)
- OTA updates (add later)
- Brightness sensor (LDR on GPIO 34 — could auto-dim backlight, add later)
- Screen sleep / screensaver
- Multiple HA instances
- mDNS discovery (hardcoded IP is fine for home use)
