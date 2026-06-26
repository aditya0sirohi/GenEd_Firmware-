#pragma once
// ESP32 HAL Implementation Stubs
// Portability boundary — swap this for SimHAL on host
// Uses ESP-IDF APIs: esp_http_client, nvs_flash,
// esp_wifi, mbedtls, gpio, ledc

#include "../include/hal.h"

// ESP32 implementations — to be completed with ESP-IDF
class Esp32Storage  : public IStorage  { /* TODO */ };
class Esp32Network  : public INetwork  { /* TODO */ };
class Esp32Time     : public ITime     { /* TODO */ };
class Esp32Power    : public IPower    { /* TODO */ };
class Esp32IO       : public IIO       { /* TODO */ };
class Esp32Security : public ISecurity { /* TODO */ };

HAL create_esp32_hal();
