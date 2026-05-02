#include "esp32_http_app.h"
#include "esp32_adapter.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

namespace {

WebServer g_server(80);
DeviceController* g_dc = nullptr;
const DeviceConfig* g_cfg = nullptr;

// ── Helpers ───────────────────────────────────────────────────────────────────

int err_to_http(ErrCode e) {
    switch (e) {
        case ERR_NONE:          return 200;
        case ERR_ILLEGAL_STATE: return 409;
        case ERR_ESTOP:         return 503;
        default:                return 500;
    }
}

void send_action(int http_code, const DeviceStatus& st, bool ok,
                 JsonVariant id = JsonVariant()) {
    JsonDocument doc;
    if (!id.isNull()) doc["id"] = id;
    doc["status"] = ok ? "OK" : "ERROR";
    doc["state"]  = device_state_name(st.state);
    doc["motion_time_ms"] = (long)st.motion_time_ms;
    if (!ok) {
        doc["error_code"]    = device_err_name(st.last_error);
        doc["error_message"] = "";
    }
    String body;
    serializeJson(doc, body);
    g_server.send(http_code, "application/json", body);
}

void send_error_400(const char* error_code, const char* error_message) {
    JsonDocument doc;
    doc["status"]        = "ERROR";
    doc["state"]         = device_state_name(g_dc->GetStatus().state);
    doc["error_code"]    = error_code;
    doc["error_message"] = error_message;
    String body;
    serializeJson(doc, body);
    g_server.send(400, "application/json", body);
}

// ── Route handlers ────────────────────────────────────────────────────────────

void handle_home() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json",
                      "{\"status\":\"ERROR\",\"state\":\"ERROR\","
                      "\"error_code\":\"PROTOCOL_ERROR\","
                      "\"error_message\":\"Method not allowed\"}");
        return;
    }
    JsonDocument req;
    deserializeJson(req, g_server.arg("plain"));
    JsonVariant id = req["id"];

    g_dc->Home();
    DeviceStatus st = g_dc->GetStatus();
    bool ok = (st.state == ST_IDLE);
    send_action(ok ? 200 : err_to_http(st.last_error), st, ok, id);
}

void handle_insert() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json",
                      "{\"status\":\"ERROR\",\"state\":\"ERROR\","
                      "\"error_code\":\"PROTOCOL_ERROR\","
                      "\"error_message\":\"Method not allowed\"}");
        return;
    }
    JsonDocument req;
    DeserializationError err = deserializeJson(req, g_server.arg("plain"));
    if (err || !req["depth_mm"].is<JsonVariant>()) {
        send_error_400("PROTOCOL_ERROR", "depth_mm is required");
        return;
    }
    int depth_mm  = req["depth_mm"];
    int speed_mm_s = req["speed_mm_s"].is<JsonVariant>()
                     ? (int)req["speed_mm_s"]
                     : g_cfg->default_speed_mm_s;
    JsonVariant id = req["id"];

    if (depth_mm <= 0 || depth_mm > g_cfg->max_depth_mm) {
        send_error_400("UNSAFE_CONFIGURATION", "depth_mm out of safe range");
        return;
    }
    if (speed_mm_s < 5 || speed_mm_s > 80) {
        send_error_400("UNSAFE_CONFIGURATION", "speed_mm_s out of safe range");
        return;
    }

    g_dc->Insert(depth_mm, speed_mm_s);
    DeviceStatus st = g_dc->GetStatus();
    bool ok = (st.state == ST_INSERTED);
    send_action(ok ? 200 : err_to_http(st.last_error), st, ok, id);
}

void handle_remove() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json",
                      "{\"status\":\"ERROR\",\"state\":\"ERROR\","
                      "\"error_code\":\"PROTOCOL_ERROR\","
                      "\"error_message\":\"Method not allowed\"}");
        return;
    }
    JsonDocument req;
    deserializeJson(req, g_server.arg("plain"));
    JsonVariant id = req["id"];

    g_dc->Remove();
    DeviceStatus st = g_dc->GetStatus();
    bool ok = (st.state == ST_IDLE);
    send_action(ok ? 200 : err_to_http(st.last_error), st, ok, id);
}

void handle_status() {
    if (g_server.method() != HTTP_GET) {
        g_server.send(405, "application/json",
                      "{\"status\":\"ERROR\",\"state\":\"ERROR\","
                      "\"error_code\":\"PROTOCOL_ERROR\","
                      "\"error_message\":\"Method not allowed\"}");
        return;
    }
    DeviceStatus st = g_dc->GetStatus();

    JsonDocument doc;
    if (g_server.hasArg("id")) {
        String id_str = g_server.arg("id");
        long id_num = id_str.toInt();
        if (id_num != 0 || id_str == "0") doc["id"] = id_num;
        else                              doc["id"] = id_str;
    }
    doc["status"]                        = "OK";
    doc["state"]                         = device_state_name(st.state);
    doc["last_error_code"]               = device_err_name(st.last_error);
    doc["last_error_message"]            = "NONE";
    doc["protocol_version"]              = 1;
    doc["min_compatible_protocol_version"] = 1;
    doc["motion_time_ms"]                = (long)st.motion_time_ms;
    doc["features"].to<JsonArray>();

    String body;
    serializeJson(doc, body);
    g_server.send(200, "application/json", body);
}

void handle_abort() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json",
                      "{\"status\":\"ERROR\",\"state\":\"ERROR\","
                      "\"error_code\":\"PROTOCOL_ERROR\","
                      "\"error_message\":\"Method not allowed\"}");
        return;
    }
    JsonDocument req;
    deserializeJson(req, g_server.arg("plain"));
    JsonVariant id = req["id"];

    g_dc->Abort();
    DeviceStatus st = g_dc->GetStatus();
    send_action(200, st, true, id);
}

}  // namespace

// ── Public API ────────────────────────────────────────────────────────────────

WebServer* esp32_http_server() { return &g_server; }

void esp32_wifi_connect(const char* ssid, const char* password) {
    Serial.print(F("Connecting to "));
    Serial.print(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
}

void esp32_http_init(DeviceController* dc, const DeviceConfig* cfg, int port) {
    g_dc  = dc;
    g_cfg = cfg;

    g_server.on("/api/home",   handle_home);
    g_server.on("/api/insert", handle_insert);
    g_server.on("/api/remove", handle_remove);
    g_server.on("/api/status", handle_status);
    g_server.on("/api/abort",  handle_abort);

    g_server.begin(port);
    Serial.print(F("HTTP server on port "));
    Serial.println(port);
}

void esp32_http_loop() {
    g_server.handleClient();
}
