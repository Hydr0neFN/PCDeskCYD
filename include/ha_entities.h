#pragma once

struct ClimateState {
  char entity_id[64] = "";
  char friendly_name[32] = "";
  char state[16] = "";       // "off", "cool", "heat", etc. (mirrors hvac_mode)
  float current_temp = 0;
  float target_temp = 0;
  char fan_mode[16] = "";
  char hvac_mode[16] = "";
};

struct LightState {
  char entity_id[64] = "";
  char friendly_name[32] = "";
  bool is_on = false;
};

struct FanState {
  char entity_id[64] = "";
  char friendly_name[32] = "";
  bool is_on = false;
  int percentage = 0;
  char preset_mode[32] = "";
  bool oscillating = false;
};

enum class EntityKind { CLIMATE, LIGHT, FAN };

// entity_id/kind are fixed at compile time; state points at the matching
// Climate/Light/FanState instance the network task updates in place.
struct EntityBinding {
  const char *entity_id;
  EntityKind kind;
  void *state;
};
