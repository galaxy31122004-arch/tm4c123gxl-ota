#ifndef PHASE3_CONFIG_H
#define PHASE3_CONFIG_H

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
#else
#include "secrets.example.h"
#endif

#define PHASE3_MQTT_HOST "thingsboard.cloud"
#define PHASE3_MQTT_PORT 1883U
#define PHASE3_DEVICE_MODEL "TM4C123GXL"

#endif
