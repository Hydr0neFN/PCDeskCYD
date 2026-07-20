#include "ui_page_bedroom.h"

#include "ui_manager.h"

BedroomPageWidgets ui_page_bedroom_create() {
  BedroomPageWidgets w;
  w.screen = lv_obj_create(NULL);
  ui_manager_create_header(w.screen, "次臥");

  // 2 icons, 80px wide, 20px gap, centered in 320px: start x=70.
  w.ac_icon = ui_manager_create_icon(w.screen, 70, 64, "次臥", "climate.ci_wo");
  w.light_icon = ui_manager_create_icon(w.screen, 170, 64, "次臥燈",
                                         "light.aqara_smart_wall_switch_z1_pro_16");

  return w;
}
