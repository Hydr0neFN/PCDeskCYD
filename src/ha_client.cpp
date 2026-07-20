#include "ha_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

#include "config.h"
#include "secrets.h"

static WebSocketsClient webSocket;
static bool ws_begun = false;
static bool ha_authenticated = false;
static unsigned long wifi_retry_at = 0;
static const char *status_text = "WiFi connecting...";

static SemaphoreHandle_t g_entity_mutex = NULL;

void ha_client_lock() {
  xSemaphoreTake(g_entity_mutex, portMAX_DELAY);
}

void ha_client_unlock() {
  xSemaphoreGive(g_entity_mutex);
}

// --- Tracked entities -------------------------------------------------
// PLAN.md's 7 target entities. entity_id/kind never change after boot;
// the state structs are updated in place from REST (initial) and WS
// state_changed events (live) by apply_entity_state().
static ClimateState g_climate_ting;
static LightState g_light_study_pendant;
static LightState g_light_study_wall;
static ClimateState g_climate_ciwo;
static LightState g_light_bedroom;
static ClimateState g_climate_chu;
static FanState g_fan_kitchen;

static EntityBinding g_entities[] = {
    {"climate.ting", EntityKind::CLIMATE, &g_climate_ting},
    {"light.aqara_smart_wall_switch_z1_pro_4", EntityKind::LIGHT, &g_light_study_pendant},
    {"light.aqara_smart_wall_switch_z1_pro_3", EntityKind::LIGHT, &g_light_study_wall},
    {"climate.ci_wo", EntityKind::CLIMATE, &g_climate_ciwo},
    {"light.aqara_smart_wall_switch_z1_pro_16", EntityKind::LIGHT, &g_light_bedroom},
    {"climate.chu", EntityKind::CLIMATE, &g_climate_chu},
    {"fan.xiaomi_p85_8f39_fan", EntityKind::FAN, &g_fan_kitchen},
};
static constexpr size_t NUM_ENTITIES = sizeof(g_entities) / sizeof(g_entities[0]);

// Applies a HA "state object" (same shape whether it's a REST /api/states/*
// response or an event's data.new_state) to the bound struct.
static void apply_entity_state(EntityBinding &b, JsonObjectConst stateObj) {
  const char *state = stateObj["state"] | "";
  JsonObjectConst attrs = stateObj["attributes"];
  const char *friendly = attrs["friendly_name"] | b.entity_id;

  ha_client_lock();
  switch (b.kind) {
    case EntityKind::CLIMATE: {
      auto *s = static_cast<ClimateState *>(b.state);
      strlcpy(s->entity_id, b.entity_id, sizeof(s->entity_id));
      strlcpy(s->friendly_name, friendly, sizeof(s->friendly_name));
      strlcpy(s->state, state, sizeof(s->state));
      strlcpy(s->hvac_mode, state, sizeof(s->hvac_mode));  // climate state == hvac_mode
      s->current_temp = attrs["current_temperature"] | s->current_temp;
      s->target_temp = attrs["temperature"] | s->target_temp;
      strlcpy(s->fan_mode, attrs["fan_mode"] | "", sizeof(s->fan_mode));
      Serial.printf("[HA] climate %s (%s): state=%s cur=%.1f target=%.1f fan=%s\n", b.entity_id,
                    s->friendly_name, s->state, s->current_temp, s->target_temp, s->fan_mode);
      break;
    }
    case EntityKind::LIGHT: {
      auto *s = static_cast<LightState *>(b.state);
      strlcpy(s->entity_id, b.entity_id, sizeof(s->entity_id));
      strlcpy(s->friendly_name, friendly, sizeof(s->friendly_name));
      s->is_on = strcmp(state, "on") == 0;
      Serial.printf("[HA] light %s (%s): is_on=%d\n", b.entity_id, s->friendly_name, s->is_on);
      break;
    }
    case EntityKind::FAN: {
      auto *s = static_cast<FanState *>(b.state);
      strlcpy(s->entity_id, b.entity_id, sizeof(s->entity_id));
      strlcpy(s->friendly_name, friendly, sizeof(s->friendly_name));
      s->is_on = strcmp(state, "on") == 0;
      s->percentage = attrs["percentage"] | 0;
      strlcpy(s->preset_mode, attrs["preset_mode"] | "", sizeof(s->preset_mode));
      s->oscillating = attrs["oscillating"] | false;
      Serial.printf("[HA] fan %s (%s): is_on=%d pct=%d preset=%s osc=%d\n", b.entity_id,
                    s->friendly_name, s->is_on, s->percentage, s->preset_mode, s->oscillating);
      break;
    }
  }
  ha_client_unlock();
}

// One REST GET per entity (small response, well under the WS library's
// 15KB frame cap) instead of a single 100KB+ get_states WS dump -- see
// HARDWARE_NOTES.md for why the WS dump doesn't work on this stack.
static void fetch_initial_states() {
  for (size_t i = 0; i < NUM_ENTITIES; i++) {
    EntityBinding &b = g_entities[i];
    // 7 rapid sequential GETs occasionally hit HTTPC_ERROR_READ_TIMEOUT
    // (-11) against HA's default server -- retry once before giving up.
    // A permanent miss here just means that entity shows stale/zero state
    // until its next subscribe_trigger update, not a crash.
    int code = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
      HTTPClient http;
      String url = String("http://") + HA_HOST + ":" + HA_PORT + "/api/states/" + b.entity_id;
      http.begin(url);
      http.addHeader("Authorization", String("Bearer ") + HA_Token);
      code = http.GET();
      if (code == HTTP_CODE_OK) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getString());
        http.end();
        if (!err) {
          apply_entity_state(b, doc.as<JsonObjectConst>());
        } else {
          Serial.printf("[HA] REST parse error for %s: %s\n", b.entity_id, err.c_str());
        }
        break;
      }
      http.end();
      if (attempt == 0) {
        Serial.printf("[HA] REST GET %s failed, code=%d, retrying\n", b.entity_id, code);
        delay(200);
      } else {
        Serial.printf("[HA] REST GET %s failed after retry, code=%d\n", b.entity_id, code);
      }
    }
  }
  Serial.println("[HA] initial REST fetch complete");
}

static void send_auth() {
  JsonDocument doc;
  doc["type"] = "auth";
  doc["access_token"] = HA_Token;
  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(out);
}

// Every post-auth command needs a unique id; HA replies reference it
// (`{"type":"result","id":N,...}`). auth itself is id-less, left as-is above.
static uint32_t next_msg_id = 1;

// --- Outgoing call_service requests -------------------------------------
// Touch handlers run on core 1 (LVGL); WebSocket I/O must stay on core 0
// (this file's net task). A queue crosses that boundary safely instead of
// touching `webSocket` directly from the UI task.
enum class ServiceParamType { NONE, STRING, NUMBER, BOOL_VAL };

struct ServiceCallRequest {
  char domain[16];
  char service[32];
  char entity_id[64];
  char extra_key[24];
  char extra_str[32];
  float extra_num;
  bool extra_bool;
  ServiceParamType extra_type;
};
static QueueHandle_t g_service_call_queue = NULL;

static void enqueue_service_call(const char *domain, const char *service, const char *entity_id,
                                  ServiceCallRequest req) {
  strlcpy(req.domain, domain, sizeof(req.domain));
  strlcpy(req.service, service, sizeof(req.service));
  strlcpy(req.entity_id, entity_id, sizeof(req.entity_id));
  xQueueSend(g_service_call_queue, &req, pdMS_TO_TICKS(100));
}

void ha_call_service(const char *domain, const char *service, const char *entity_id) {
  ServiceCallRequest req = {};
  req.extra_type = ServiceParamType::NONE;
  enqueue_service_call(domain, service, entity_id, req);
}

void ha_call_service_str(const char *domain, const char *service, const char *entity_id,
                          const char *key, const char *value) {
  ServiceCallRequest req = {};
  req.extra_type = ServiceParamType::STRING;
  strlcpy(req.extra_key, key, sizeof(req.extra_key));
  strlcpy(req.extra_str, value, sizeof(req.extra_str));
  enqueue_service_call(domain, service, entity_id, req);
}

void ha_call_service_num(const char *domain, const char *service, const char *entity_id,
                          const char *key, float value) {
  ServiceCallRequest req = {};
  req.extra_type = ServiceParamType::NUMBER;
  strlcpy(req.extra_key, key, sizeof(req.extra_key));
  req.extra_num = value;
  enqueue_service_call(domain, service, entity_id, req);
}

void ha_call_service_bool(const char *domain, const char *service, const char *entity_id,
                           const char *key, bool value) {
  ServiceCallRequest req = {};
  req.extra_type = ServiceParamType::BOOL_VAL;
  strlcpy(req.extra_key, key, sizeof(req.extra_key));
  req.extra_bool = value;
  enqueue_service_call(domain, service, entity_id, req);
}

static void send_call_service(const ServiceCallRequest &req) {
  uint32_t id = next_msg_id++;
  JsonDocument doc;
  doc["id"] = id;
  doc["type"] = "call_service";
  doc["domain"] = req.domain;
  doc["service"] = req.service;
  doc["service_data"]["entity_id"] = req.entity_id;
  switch (req.extra_type) {
    case ServiceParamType::STRING:
      doc["service_data"][req.extra_key] = req.extra_str;
      break;
    case ServiceParamType::NUMBER:
      doc["service_data"][req.extra_key] = req.extra_num;
      break;
    case ServiceParamType::BOOL_VAL:
      doc["service_data"][req.extra_key] = req.extra_bool;
      break;
    case ServiceParamType::NONE:
      break;
  }
  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(out);
  Serial.printf("[HA] call_service %s.%s(%s), id=%u\n", req.domain, req.service, req.entity_id,
                id);
}

// subscribe_trigger, not subscribe_events: subscribe_events(state_changed) is
// a firehose over every entity in HA, including ones we don't track (e.g. a
// weather entity's forecast attributes). Any single untracked entity with a
// state object over ~15KB trips the same WEBSOCKETS_MAX_DATA_SIZE disconnect
// this replaces (see HARDWARE_NOTES.md). subscribe_trigger with an entity_id
// list is server-scoped -- HA only sends us frames for these 7 entities.
static void send_subscribe_trigger() {
  uint32_t id = next_msg_id++;
  JsonDocument doc;
  doc["id"] = id;
  doc["type"] = "subscribe_trigger";
  JsonObject trigger = doc["trigger"].to<JsonObject>();
  trigger["platform"] = "state";
  JsonArray entity_ids = trigger["entity_id"].to<JsonArray>();
  for (size_t i = 0; i < NUM_ENTITIES; i++) {
    entity_ids.add(g_entities[i].entity_id);
  }
  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(out);
  Serial.printf("[HA] sent subscribe_trigger(state, %u entities), id=%u\n",
                (unsigned)NUM_ENTITIES, id);
}

static void ws_event(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      ha_authenticated = false;
      status_text = "HA disconnected, reconnecting...";
      Serial.println("[HA] WebSocket disconnected");
      break;

    case WStype_CONNECTED:
      status_text = "WS connected, authenticating...";
      Serial.println("[HA] WebSocket connected, awaiting auth_required");
      break;

    case WStype_TEXT: {
      Serial.printf("[HA] WStype_TEXT length=%u freeHeap=%u\n", (unsigned)length,
                    ESP.getFreeHeap());

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) {
        Serial.printf("[HA] JSON parse error: %s\n", err.c_str());
        return;
      }
      const char *msg_type = doc["type"];
      if (!msg_type) return;

      if (strcmp(msg_type, "auth_required") == 0) {
        Serial.println("[HA] auth_required -> sending token");
        send_auth();
      } else if (strcmp(msg_type, "auth_ok") == 0) {
        ha_authenticated = true;
        status_text = "HA authenticated";
        Serial.println("[HA] auth_ok -> HA WebSocket authenticated");
        send_subscribe_trigger();
      } else if (strcmp(msg_type, "auth_invalid") == 0) {
        ha_authenticated = false;
        status_text = "HA auth invalid - check token";
        Serial.println("[HA] auth_invalid - check HA_Token in secrets.h");
      } else if (strcmp(msg_type, "result") == 0) {
        uint32_t id = doc["id"] | 0;
        bool success = doc["success"] | false;
        Serial.printf("[HA] result id=%u success=%d\n", id, success);
      } else if (strcmp(msg_type, "event") == 0) {
        // subscribe_trigger event shape: event.variables.trigger.{entity_id,to_state}.
        // Already server-scoped to our 7 entities, but match by id anyway
        // since trigger.entity_id tells us which struct to update.
        JsonObjectConst trigger = doc["event"]["variables"]["trigger"];
        const char *entity_id = trigger["entity_id"] | "";
        JsonObjectConst to_state = trigger["to_state"];
        if (to_state.isNull()) return;
        for (size_t i = 0; i < NUM_ENTITIES; i++) {
          if (strcmp(g_entities[i].entity_id, entity_id) == 0) {
            apply_entity_state(g_entities[i], to_state);
            break;
          }
        }
      }
      break;
    }

    default:
      break;
  }
}

static void ha_client_init() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(Wifi_SSID, Wifi_Password);
  Serial.printf("[WiFi] connecting to %s\n", Wifi_SSID);
  // Avoids ha_client_loop() immediately calling WiFi.begin() again before
  // this connection attempt resolves (harmless but logs a
  // "STA clear config failed" warning from the IDF WiFi task).
  wifi_retry_at = millis() + 5000;
}

static void ha_client_loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (ws_begun) {
      webSocket.disconnect();
      ws_begun = false;
    }
    ha_authenticated = false;
    status_text = "WiFi connecting...";

    unsigned long now = millis();
    if (now >= wifi_retry_at) {
      WiFi.begin(Wifi_SSID, Wifi_Password);
      wifi_retry_at = now + 5000;
    }
    return;
  }

  if (!ws_begun) {
    Serial.printf("[WiFi] connected: %s\n", WiFi.localIP().toString().c_str());
    fetch_initial_states();
    webSocket.begin(HA_HOST, HA_PORT, HA_WS_PATH);
    webSocket.onEvent(ws_event);
    webSocket.setReconnectInterval(5000);
    ws_begun = true;
    status_text = "WS connecting...";
  }

  webSocket.loop();

  ServiceCallRequest req;
  while (xQueueReceive(g_service_call_queue, &req, 0) == pdTRUE) {
    if (ha_authenticated) {
      send_call_service(req);
    } else {
      Serial.printf("[HA] dropped call_service %s.%s(%s) - not authenticated\n", req.domain,
                    req.service, req.entity_id);
    }
  }
}

const char *ha_client_status() {
  return status_text;
}

size_t ha_client_entity_count() {
  return NUM_ENTITIES;
}

const EntityBinding *ha_client_entities() {
  return g_entities;
}

static void net_task(void *) {
  ha_client_init();
  for (;;) {
    ha_client_loop();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void ha_client_start() {
  g_entity_mutex = xSemaphoreCreateMutex();
  g_service_call_queue = xQueueCreate(4, sizeof(ServiceCallRequest));
  xTaskCreatePinnedToCore(net_task, "ha_net", 8192, NULL, 1, NULL, 0);
}
