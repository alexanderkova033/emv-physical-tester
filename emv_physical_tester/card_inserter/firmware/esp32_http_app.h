#pragma once

#include <WebServer.h>
#include "card_inserter_use_case_controller.h"
#include "card_inserter_domain_types.h"

void esp32_wifi_connect(const char* ssid, const char* password);
void esp32_http_init(DeviceController* dc, const DeviceConfig* cfg, int port = 80);
void esp32_http_loop();
WebServer* esp32_http_server();
