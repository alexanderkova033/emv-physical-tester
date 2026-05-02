#include "esp32_pins.h"
#include "esp32_adapter.h"
#include "esp32_http_app.h"
#include "button_board_input_adapter.h"
#include "card_inserter_use_case_controller.h"

namespace {

constexpr int   kAngleHome    = 0;
constexpr int   kAngleRemove  = 30;
constexpr int   kAngleInsert  = 152;
constexpr int   kMaxDepthMm   = 50;
constexpr int   kDefaultDepth = 35;
constexpr int   kDefaultSpeed = 20;

DeviceController g_dc;
DeviceConfig     g_cfg;

}  // namespace

void setup() {
    esp32_hw_init(115200, PIN_SERVO_PWM, kAngleHome);

    g_cfg.angle_home        = kAngleHome;
    g_cfg.angle_remove      = kAngleRemove;
    g_cfg.angle_insert      = kAngleInsert;
    g_cfg.max_depth_mm      = kMaxDepthMm;
    g_cfg.default_depth_mm  = kDefaultDepth;
    g_cfg.default_speed_mm_s = kDefaultSpeed;

    esp32_wifi_connect("Wokwi-GUEST", "");

    // Bind ports — pass the server so delay_ms pumps handleClient during motion.
    DevicePorts ports{};
    esp32_http_init(&g_dc, &g_cfg, 80);
    esp32_bind_device_ports(&ports, esp32_http_server(), &g_dc);
    g_dc.Init(g_cfg, ports);
    g_dc.OnEstop();

    Serial.println(F("Ready. Use curl or the JVM client at http://<ip>/api/..."));
}

void loop() {
    g_dc.OnEstop();
    device_button_board_poll(&g_dc, kDefaultDepth, kDefaultSpeed);
    esp32_http_loop();
}
