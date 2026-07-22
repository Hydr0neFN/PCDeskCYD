#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "ha_client.h"
#include "ui_detail_climate.h"
#include "ui_detail_fan.h"
#include "ui_manager.h"

// CYD touch uses non-default SPI pins, separate bus from the display.
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

static TFT_eSPI tft = TFT_eSPI();
static SPIClass touchscreenSpi = SPIClass(VSPI);
static XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// 64 lines (~40KB) instead of the original 24 (~15KB): fewer SPI flush
// transactions per full-screen redraw, noticeably smoother nav-tab slide
// animation. RAM headroom is ample (20% used) so this is cheap.
static constexpr int DRAW_BUF_LINES = 64;
static uint16_t draw_buf[320 * DRAW_BUF_LINES] __attribute__((aligned(4)));

static void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(reinterpret_cast<uint16_t *>(px_map), w * h, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

// Idle backlight off via LEDC PWM, ramped rather than a raw digitalWrite
// step -- an earlier digitalWrite on/off attempt caused a visible
// brightness flicker on wake (see HARDWARE_NOTES.md). TFT_BACKLIGHT_ON is
// HIGH on this board, so PWM duty 255 = full on, 0 = off maps directly.
static constexpr unsigned long IDLE_TIMEOUT_MS = 30000;
static constexpr uint32_t BACKLIGHT_PWM_FREQ = 5000;
static constexpr uint8_t BACKLIGHT_PWM_RES = 8;
static constexpr unsigned long RAMP_STEP_MS = 8;
static constexpr int RAMP_STEP = 6;

static unsigned long s_last_touch_ms = 0;
static unsigned long s_last_ramp_ms = 0;
static int s_backlight_duty = 255;
static int s_backlight_target = 255;

static void my_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    // Raw ADC calibration range for XPT2046 on CYD.
    data->point.x = map(p.x, 200, 3700, 1, 320);
    data->point.y = map(p.y, 240, 3800, 1, 240);
    data->state = LV_INDEV_STATE_PRESSED;

    s_last_touch_ms = millis();
    s_backlight_target = 255;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32-CYD: Milestone 7 - Climate + fan detail pages");

  ha_client_start();  // runs on its own task, pinned to core 0

  touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSpi);
  touchscreen.setRotation(1);  // matches tft.setRotation(1) below

  tft.init();
  tft.setRotation(1);  // proven orientation: Examples/Basics/1-HelloWorld, 2-TouchTest

  ledcAttach(TFT_BL, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
  ledcWrite(TFT_BL, s_backlight_duty);
  s_last_touch_ms = millis();

  lv_init();
  lv_tick_set_cb(millis);

  lv_display_t *disp = lv_display_create(320, 240);
  lv_display_set_flush_cb(disp, my_flush_cb);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *touch_indev = lv_indev_create();
  lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch_indev, my_touch_read_cb);

  ui_manager_init();

  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  // Poll ha_client's entity structs at a fixed cadence rather than every
  // LVGL tick -- keeps refresh well under the M5 gate's 1-2s latency
  // budget without hammering LVGL's style/text setters (and the SPI
  // flushes they'd trigger) 200x/sec for values that rarely change.
  static unsigned long last_refresh = 0;
  unsigned long now = millis();
  if (now - last_refresh >= 250) {
    last_refresh = now;
    ui_manager_refresh();
    ui_detail_climate_refresh();
    ui_detail_fan_refresh();
  }

  if (now - s_last_touch_ms >= IDLE_TIMEOUT_MS) {
    s_backlight_target = 0;
  }
  if (s_backlight_duty != s_backlight_target && now - s_last_ramp_ms >= RAMP_STEP_MS) {
    s_last_ramp_ms = now;
    if (s_backlight_duty < s_backlight_target) {
      s_backlight_duty = min(s_backlight_duty + RAMP_STEP, s_backlight_target);
    } else {
      s_backlight_duty = max(s_backlight_duty - RAMP_STEP, s_backlight_target);
    }
    ledcWrite(TFT_BL, s_backlight_duty);
  }

  lv_timer_handler();
  delay(5);
}
