#pragma once

#include <lvgl.h>

#include "ui_manager.h"

struct StudyPageWidgets {
  lv_obj_t *screen;
  IconWidgets ac_icon;
  IconWidgets light1_icon;
  IconWidgets light2_icon;
};

StudyPageWidgets ui_page_study_create();
