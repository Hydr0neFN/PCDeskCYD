#pragma once

#include <lvgl.h>

#include "ui_manager.h"

struct BedroomPageWidgets {
  lv_obj_t *screen;
  IconWidgets ac_icon;
  IconWidgets light_icon;
};

BedroomPageWidgets ui_page_bedroom_create();
