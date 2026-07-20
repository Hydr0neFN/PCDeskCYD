#pragma once

#include <lvgl.h>

#include "ui_manager.h"

struct KitchenPageWidgets {
  lv_obj_t *screen;
  IconWidgets ac_icon;
  IconWidgets fan_icon;
};

KitchenPageWidgets ui_page_kitchen_create();
