#include "ui_page_study.h"

#include "ui_manager.h"

StudyPageWidgets ui_page_study_create() {
  StudyPageWidgets w;
  w.screen = lv_obj_create(NULL);
  ui_manager_create_header(w.screen, "書房");

  // 3 icons, 80px wide, 20px gaps, centered in 320px: start x=20.
  w.ac_icon = ui_manager_create_icon(w.screen, 20, 64, "廳", "climate.ting");
  w.light1_icon = ui_manager_create_icon(w.screen, 120, 64, "書房吊燈",
                                          "light.aqara_smart_wall_switch_z1_pro_4");
  w.light2_icon = ui_manager_create_icon(w.screen, 220, 64, "書房坎燈",
                                          "light.aqara_smart_wall_switch_z1_pro_3");

  return w;
}
