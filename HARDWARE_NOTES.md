# CYD Hardware Bring-Up Findings (Milestone 1)

Board: ESP32-2432S028, 2.8" 320x240, marked `TPM408-2.8` on the touch flex.
Confirmed working stack and gotchas found during M1 bring-up — read this
before touching display/touch code again.

## Correction (2026-09-27): the controller is an ST7789

The two sections below (`ILI9341_2_DRIVER`, `TFT_INVERSION_ON`) are
**superseded**. They describe what made the panel usable, not what the
controller is. Found while building the sibling project
[NLDeskCYD](https://github.com/Hydr0neFN/NLDeskCYD) on this same board, whose
dark theme exposed what this project's light theme hid:

- With `ILI9341_2_DRIVER` + `TFT_INVERSION_ON`, every non-black colour had a
  strong blue/green lift: `#0B0E14` showed as navy, amber `#D29922` as pale
  mint, white as light cyan. Pure black stayed black, so it is a gamma
  problem, not inversion or channel order.
- Cause: the panel is an **ST7789**. `ILI9341_2_DRIVER` writes ILI9341-format
  gamma tables into registers `E0`/`E1`, which the ST7789 interprets with a
  different layout. The "ILI9341 clone with a different register set" reading
  below was this: a different controller.
- Fix, confirmed on hardware with a colour-swatch page: `-D ST7789_DRIVER`,
  `TFT_WIDTH=240`, `TFT_HEIGHT=320`, `TFT_RGB_ORDER=TFT_BGR`, inversion
  **off**. Geometry, rotation 1 and the touch mapping are unchanged.
- Why the plain `ILI9341_DRIVER` left part of the panel unwritten while
  `ILI9341_2_DRIVER` did not is inferred, not measured: the `_2` init writes a
  full-screen `CASET`/`RASET` window and orders `MADCTL` differently. A logic
  analyser trace would settle it.

## Board variant: needs `ILI9341_2_DRIVER`, not plain `ILI9341_DRIVER` *(superseded, kept for history)*

This board's ILI9341 clone controller uses a different init/register sequence
than a standard ILI9341 (different `PWCTR1/2`, `VMCTR1/2`, gamma tables,
`MADCTL`). Confirmed by cross-referencing witnessmenow/ESP32-Cheap-Yellow-Display
(`DisplayConfig/User_Setup.h`, `Examples/Basics/1-HelloWorld`, `2-TouchTest`),
all of which build with `-D ILI9341_2_DRIVER` for TFT_eSPI.

**Symptom if you use the wrong variant**: LovyanGFX's plain `Panel_ILI9341`
class (register set for stock ILI9341) partially addresses the panel —
roughly the left ~65-75% of columns write real (but wrong-looking) data, the
remaining columns show static noise (never-written GRAM). Screen looks like
"leftover garbage from a previous program." LovyanGFX does ship a
`Panel_ILI9341_2` class with matching registers
(`.pio/libdeps/*/LovyanGFX/src/lgfx/v1/panel/Panel_ILI9341.hpp`) — using it
fixed the column corruption but a *different* MADCTL base (`0x36=0x08` vs
default) meant `lcd.setRotation(1)` came out mirrored (graphics fine, text
backwards — a centered symmetric button hides the mirror, text reveals it).
Chasing the right rotation index for `Panel_ILI9341_2` was not resolved
before switching stacks (see below).

## Stack decision: TFT_eSPI, not LovyanGFX

PLAN.md originally specified LovyanGFX. After two failed LovyanGFX rotation/
MADCTL attempts, switched to **TFT_eSPI + XPT2046_Touchscreen**, matching the
proven witnessmenow reference examples byte-for-byte on pins/driver/rotation.
LVGL is wired on top via a hand-written `flush_cb`/touch `read_cb` in
`src/main.cpp` (not `lv_tft_espi_create()` — that LVGL-bundled TFT_eSPI
driver is untested for this board; safer to reuse the proven raw-driver
config and only bolt LVGL's buffer-flush semantics on top).

**Do not readopt LovyanGFX for this board without first getting a clean,
unmirrored `Panel_ILI9341_2` + rotation combination confirmed on hardware.**

## Confirmed working config

```
platformio.ini build_flags (env:cyd):
  USER_SETUP_LOADED, USE_HSPI_PORT
  TFT_MISO=12  TFT_MOSI=13  TFT_SCLK=14  TFT_CS=15  TFT_DC=2  TFT_RST=-1  TFT_BL=21
  TFT_BACKLIGHT_ON=HIGH
  SPI_FREQUENCY=55000000  SPI_READ_FREQUENCY=20000000  SPI_TOUCH_FREQUENCY=2500000
  ILI9341_2_DRIVER
  LOAD_GLCD/FONT2/FONT4/FONT6/FONT7/FONT8/GFXFF

Touch (separate VSPI bus, XPT2046_Touchscreen library):
  CLK=25  MOSI=32  MISO=39  CS=33  IRQ=36
  touchscreen.setRotation(1)   -- matches tft.setRotation(1)
  raw ADC calibration: x 200-3700, y 240-3800 (typical starting values)

tft.setRotation(1)  -- confirmed correct, unmirrored, matches touch mapping
```

Full pin map (TFT + touch + RGB LED + SD + LDR + backlight) cross-checked
against the vendor's own pinout table — matches PLAN.md's table exactly, no
changes needed there.

## LVGL 9.5 draw buffer alignment

`lv_display_set_buffers()` asserts if the draw buffer isn't aligned to
`LV_DRAW_BUF_ALIGN` (default drifted stricter between LVGL 9.1 and 9.5 — our
`lib_deps` pins `^9.1.0`, which resolved to 9.5.0 at build time). A plain
`static uint16_t draw_buf[...]` can land on a 2-byte boundary and fail the
assert (`buf1 not aligned`, halts in `setup()`, screen shows whatever was on
it before — easy to misread as a display bug).

Fix (both, belt and suspenders):
- `static uint16_t draw_buf[...] __attribute__((aligned(4)));`
- `#define LV_DRAW_BUF_ALIGN 4` in `lv_conf.h`

## Color inversion — needs `TFT_INVERSION_ON` *(superseded: correct for `ILI9341_2_DRIVER` only; ST7789 wants inversion off)*

Without an explicit `TFT_INVERSION_ON`/`_OFF` build flag, `ILI9341_2_DRIVER`'s
default inversion state is wrong for this board: a known-green LVGL label
(`lv_palette_main(LV_PALETTE_GREEN)`) rendered purple, and the light theme's
white background rendered as black/navy. Added `-D TFT_INVERSION_ON` to
`[env:cyd]` build_flags — confirmed on hardware: label renders green, theme
background renders its true white. If you ever see globally-inverted-looking
colors (dark theme where you expect light, wrong hue on known colors), this
flag is the first thing to check.

## Touch coordinate mapping — confirmed correct

Logged `Button pressed at (168, 118)` when tapping the centered 160x60
button (spans x:80-240, y:90-150 in the 320x240 logical canvas) — inside
bounds, correct orientation, no axis swap/invert needed with
`touchscreen.setRotation(1)` + the calibration constants above.

**Gate design note**: a centered, symmetric button can't detect a 180°
touch inversion (every point still maps onto the button under
`x'=320-x, y'=240-y`). If touch ever seems oddly reliable but wrong further
into the UI, tap an off-center point and check the logged coordinates
against where you actually touched.

## WebSockets library: 15KB hard frame cap (breaks `get_states`)

`links2004/WebSockets` (`WebSocketsClient`) hard-caps incoming frame size at
`WEBSOCKETS_MAX_DATA_SIZE` = `15 * 1024` for ESP32
(`.pio/libdeps/*/WebSockets/src/WebSockets.h:66`). Any frame bigger than that
gets `clientDisconnect(client, 1009)` in `WebSockets.cpp:438` **before** your
`onEvent` callback ever sees it -- no `WStype_TEXT`, no error text, just an
immediate `WStype_DISCONNECTED`.

**This kills HA's `get_states` command** (PLAN.md's original M3 approach):
a real HA instance's full state dump is 100KB+, so every `get_states`
request triggers an instant disconnect/reconnect loop. Symptom: serial log
shows `sent get_states, id=N` immediately followed by
`WebSocket disconnected`, repeating forever, `freeHeap` looking fine the
whole time (this is not a heap problem, don't chase one).

The `#define` isn't `#ifndef`-guarded, so overriding it via
`-D WEBSOCKETS_MAX_DATA_SIZE=...` in `platformio.ini` build_flags **does
not work** (header's own `#define` wins, just a redefinition warning). Even
if it did work, raising it is heap-fatal anyway: the library `malloc()`s the
full frame (`WebSockets.cpp:245`), then ArduinoJson copies it again --
200KB+ against ~198KB free heap on this board.

**PLAN.md's suggested mitigation (ArduinoJson filter on the receiving end)
does not help** -- the disconnect happens at frame-receipt, before any
JSON parsing code runs. There are no bytes to filter.

**Fix**: never let a single WS frame exceed ~15KB. Concretely:
- Initial state fetch: 7 individual REST `GET /api/states/<entity_id>`
  calls (`HTTPClient`, plain HTTP, no WS frame-size limit) instead of one
  WS `get_states`.
- Live updates: HA WS `subscribe_trigger` with a `state` platform trigger
  and an `entity_id` array (our 7 IDs), **not** `subscribe_events` with
  `event_type: state_changed`. `subscribe_events(state_changed)` is a
  firehose over *every* entity in HA -- a single untracked entity with a
  large state object (a `weather.*` entity's forecast attributes are the
  classic case) can still exceed 15KB and trip the same disconnect.
  `subscribe_trigger` with an entity_id list is server-scoped: HA only
  sends frames for those entities, so payload size is bounded regardless
  of what else exists on the HA instance.
- `subscribe_trigger`'s event shape differs from `subscribe_events`: the
  changed state lives at `event.variables.trigger.to_state` (not
  `event.data.new_state`), and `event.variables.trigger.entity_id` says
  which entity fired.

Confirmed on hardware: 7 REST fetches logged correct friendly names/states/
attributes, `subscribe_trigger` result came back `success=1`, live
`to_state` events observed at 1-4KB (comfortably under the cap).

## CJK font pipeline (lv_font_conv) — two silent-failure gotchas

Generated `src/font_noto_20.c`/`font_noto_28.c` via `npx lv_font_conv` from
`C:\Windows\Fonts\NotoSansTC-VF.ttf` (already present on this machine --
variable font, but lv_font_conv/opentype.js handled it fine, no need to
instance it with fonttools). Symbol set: exactly the characters used on
screen (`書房次臥廚廳吊燈坎電風扇°直吹自然` -- the last 4 added in M7 for
the fan detail page's preset buttons, missed in the original M4 subset
and caught as tofu on hardware) + ASCII `0x20-0x7F`. Regenerate both
sizes together if a new label introduces a character not in that set --
missing glyphs render as blank tofu with no error.

Two build/config gotchas hit during bring-up, both silent or near-silent:

1. **`#include "lvgl/lvgl.h"` not found.** lv_font_conv's generated `.c`
   files pick between `#include "lvgl.h"` and `#include "lvgl/lvgl.h"`
   based on `LV_LVGL_H_INCLUDE_SIMPLE` -- a *different* macro from
   `LV_CONF_INCLUDE_SIMPLE` (which only controls how `lvgl.h` finds
   `lv_conf.h`). Needed both as build_flags:
   `-D LV_CONF_INCLUDE_SIMPLE -D LV_LVGL_H_INCLUDE_SIMPLE`.
2. **Glyphs silently fail to render.** lv_font_conv emits RLE-compressed
   bitmaps by default. Without `#define LV_USE_FONT_COMPRESSED 1` in
   `lv_conf.h`, the font loads fine and no build error occurs -- glyphs
   just don't draw, with only a runtime `[Warn] draw_letter_cb: Couldn't
   get bitmap glyph` / `Compressed fonts is used but LV_USE_FONT_COMPRESSED
   not enabled` on serial to hint why.

Custom fonts are made available globally (same as `LV_FONT_MONTSERRAT_*`)
via `#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(font_noto_20) LV_FONT_DECLARE(font_noto_28)`
in `lv_conf.h` -- reference `&font_noto_20`/`&font_noto_28` directly from
any file that includes `lvgl.h`, no extra header needed.

Cost confirmed on hardware: ~8KB flash for both sizes combined (12 CJK
glyphs + ASCII, bpp4), 0 bytes RAM/heap (glyph data lives in flash
`.rodata`, not loaded into RAM).

## No TE pin -- avoid directional screen-transition animations

This board's confirmed pin map (see "Confirmed working config" above) has
no tearing-effect (TE) signal wired, and LVGL/TFT_eSPI's partial-buffer
flush isn't synced to the panel's own refresh either way. Any
`lv_screen_load_anim()` with directional motion (`MOVE_LEFT` etc.) shows a
visible top-to-bottom tear during the sweep, worse the larger the draw
buffer's flush chunk. Confirmed on hardware; switched nav-tab transitions
to `lv_screen_load()` (instant, no animation) since a static swap has no
motion for the tear to be visible against. If a transition effect is
wanted later, a full-screen fade is worth trying before motion (same tear
risk, but stationary blend may hide it better) -- not yet tested.

**Decided against a spinning fan/AC icon for the same reason (M7):** a
continuously-animating glyph would tear *for as long as that detail page
stays open*, not just during a one-shot transition -- strictly worse than
the nav-slide case this section already fixed by removing motion. Fan
icon (`LV_SYMBOL_LOOP`) stays static; color still conveys on/off. Revisit
only if a DMA-driven full-frame flush path is ever added.

## Bold CJK text needs a separate font (VF weight instancing)

`lv_font_conv` renders whatever the source variable font's default
instance is (Regular/400 for `NotoSansTC-VF.ttf`) -- there's no CLI flag
to pick a different weight axis value directly. For a bold variant
(`font_noto_28_bold.c`, used for room-page headers):
```
pip install fonttools
python -m fontTools.varLib.instancer NotoSansTC-VF.ttf wght=700 -o NotoSansTC-Bold.ttf
npx lv_font_conv --font NotoSansTC-Bold.ttf --size 28 --bpp 4 --format lvgl \
  --symbols "書房次臥廚" -o font_noto_28_bold.c
```
Keep the symbol set scoped to just what that specific bold usage needs
(here: only the room headers' 5 chars) rather than reusing the full
12-char app-wide set -- smaller flash footprint, and bold is a distinct
font object from `font_noto_28`, doesn't replace it. Declare via the same
`LV_FONT_CUSTOM_DECLARE` mechanism in `lv_conf.h`.

## AC/climate icon: Segoe Fluent Icons, not LVGL's LV_SYMBOL_*

LVGL's built-in symbol set has no thermostat/AC/snowflake glyph. Used
`C:\Windows\Fonts\SegoeIcons.ttf` (ships with Windows) instead: U+E9CA
"Frigid" (closest built-in match to a cooling concept), confirmed present
in the font's cmap via `fontTools` (`TTFont(path).getBestCmap()`) *before*
generating the icon font -- these PUA icon fonts don't have greppable
descriptive glyph names in the file itself, only Microsoft's own docs
(learn.microsoft.com Segoe Fluent Icons page) name them, so cross-check
both the doc's codepoint claim and the actual local font file rather than
trusting either alone:
```
npx lv_font_conv --font C:\Windows\Fonts\SegoeIcons.ttf --size 28 --bpp 4 \
  --format lvgl --range 0xE9CA-0xE9CA -o font_ac_28.c
```
Referenced in code as an explicit UTF-8 byte-literal macro (`"\xEE\xA7\x8A"`
for U+E9CA), not a plain source-file `°`-style literal -- the codepoint
is outside what any editor/keyboard types directly. This is a *separate*
font object from `font_noto_*`/Montserrat; the climate room-icon's glyph
label switches its font to `&font_ac_28` specifically (light/fan icons on
the same struct type stay on Montserrat's `LV_SYMBOL_*`).

## Light icon: same Segoe Fluent Icons source, single glyph + color

U+EA80 "Lightbulb" (confirmed present in `SegoeIcons.ttf`'s cmap the same
way as `AC_ICON` above). Only one lightbulb glyph turned up in Microsoft's
docs for the visible codepoint ranges -- no separate lit/unlit variant
confirmed, so this one glyph is reused for both states with color (yellow
on / gray off) carrying the on/off signal, same pattern as the fan icon's
static `LV_SYMBOL_LOOP`. If a filled/outline pair is ever confirmed to
exist (unchecked ranges: PUA F000-F5FF, F600-F8CC), swap this out.

## Buttonmatrix label text != HA service value (M7)

Climate/fan detail buttonmatrix labels are short display text, not what
gets sent to `call_service` -- three of them differ from HA's actual
values (confirmed against this HA instance's Hitachi/Xiaomi entities):

| UI label | HA service value | Where |
|---|---|---|
| `fan` (hvac mode) | `fan_only` | `climate.set_hvac_mode` |
| `med` (fan mode) | `medium` | `climate.set_fan_mode` |
| `直吹` (preset) | `Straight Wind` | `fan.set_preset_mode` |
| `自然` (preset) | `Natural Wind` | `fan.set_preset_mode` |

Never pass buttonmatrix label text straight to `ha_call_service_str()` --
always go through the label->value array (`ui_detail_climate.cpp`'s
`HVAC_VALUES`/`FAN_VALUES`, `ui_detail_fan.cpp`'s `PRESET_VALUES`), and the
reverse (value->index via `index_of()`/`preset_index_of()`) when setting
the buttonmatrix's checked state from HA's current state. An invalid
value fails the HA service call silently server-side -- no error surfaces
on the ESP32 side, it just doesn't work.

## LV_SYMBOL_* icons are Montserrat-only, not in the Noto subset

`LV_SYMBOL_LEFT`/`POWER`/`UP`/`DOWN` (and all other built-in LVGL icons)
are private-use codepoints baked into LVGL's own Montserrat font builds --
they don't exist in `font_noto_20`/`font_noto_28` (our CJK subset only
covers actual Han characters + ASCII + °, see the CJK font pipeline
section above). Never set an LV_SYMBOL string on a label using
`&font_noto_*` -- it renders as tofu. Rule: CJK text -> `font_noto_*`,
symbol icons -> `lv_font_montserrat_*`, never mix glyph classes on one
label. `ui_manager_create_symbol_button()` encodes this split so
detail-page back/power/up/down buttons don't need to think about it again.

## Climate call_service round-trip latency: ~1.7-2.0s (not local)

Measured via `monitor_filters = time` (platformio.ini) on `climate.ting`:
`set_temperature` sent at 22:55:15.282, confirming `subscribe_trigger` event
(new `target_temp`) landed at 22:55:16.945 -- and again 17.672 -> 19.616 on a
second tap. This entity's underlying integration round-trips through a
cloud-backed AC bridge, not a local protocol like Zigbee -- treat climate
calls as 1-2s+ one-way, not near-instant like the Aqara lights.

Consequence: a flat short suppression window (as used for optimistic light
taps) isn't reliable here -- the periodic detail-page refresh (250ms tick)
would repaint from ha_client's still-stale struct before the confirmation
lands, visibly flickering the tapped control back to its pre-tap value and
then forward again once the real event arrives. Fixed in
`ui_detail_climate.cpp`/`ui_detail_fan.cpp` by tracking each optimistic
value as "pending" and only letting refresh repaint once the confirmed
snapshot matches it (or a 5s safety timeout elapses, treating the call as
failed and reverting to true state).

## Idle backlight off -- needs PWM ramp, not a raw digitalWrite step

First attempt used a plain `digitalWrite(TFT_BL, ...)` off/on toggle (30s
idle timeout, wake on touch). Confirmed on hardware: waking the screen
produced a visible brightness flicker, not a clean instant-on. TFT_eSPI only
drives `TFT_BL` with a single `digitalWrite` during `tft.init()` (no PWM, no
ongoing ownership -- see its `init()` source), so this board's backlight
circuit needs a soft-start ramp rather than a raw digital step to switch
cleanly.

Fixed by switching `TFT_BL` to LEDC PWM (`ledcAttach(TFT_BL, 5000, 8)` once
in `setup()`, after `tft.init()`) and ramping duty 0<->255 in small steps
(`RAMP_STEP`/`RAMP_STEP_MS` in `main.cpp`) on idle/wake instead of jumping
directly between full and off. `TFT_BACKLIGHT_ON` is `HIGH` on this board,
so PWM duty 255 = on, 0 = off maps directly -- would need inverting if a
board variant used the opposite polarity.

## Reference material

- `C:\Users\Ruanyouyi\Downloads\ESP32-Cheap-Yellow-Display-main` — local
  clone of witnessmenow/ESP32-Cheap-Yellow-Display, used as ground truth.
  `Examples/Basics/1-HelloWorld` and `2-TouchTest` are the two examples
  actually confirmed working on this physical board.
- `Examples/LVGL9/LVGL_Arduino` in that repo uses `lv_tft_espi_create()` —
  **not verified on this board**, do not assume it works without testing.
