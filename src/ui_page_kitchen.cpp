#include "ui_page_kitchen.h"

#include "ui_manager.h"

KitchenPageWidgets ui_page_kitchen_create() {
  KitchenPageWidgets w;
  w.screen = lv_obj_create(NULL);
  ui_manager_create_header(w.screen, "廚房");

  // 2 icons, 80px wide, 20px gap, centered in 320px: start x=70.
  w.ac_icon = ui_manager_create_icon(w.screen, 70, 64, "廚", "climate.chu");
  w.fan_icon = ui_manager_create_icon(w.screen, 170, 64, "電風扇", "fan.xiaomi_p85_8f39_fan");

  return w;
}
