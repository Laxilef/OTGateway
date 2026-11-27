#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

#define PROJECT_NAME                    "OpenTherm Gateway"
#define PROJECT_REPO                    "https://github.com/Laxilef/OTGateway"

#define MQTT_RECONNECT_INTERVAL         15000
#define MQTT_SENSOR_REPUBLISH_INTERVAL  300000u
#define SETTINGS_VALID_VALUE            "stvalid" // only 8 chars!
#define GPIO_IS_NOT_CONFIGURED          0xff

#define DEFAULT_HEATING_TARGET_TEMP     40
#define DEFAULT_HEATING_MIN_TEMP        20
#define DEFAULT_HEATING_MAX_TEMP        90

#define DEFAULT_DHW_TARGET_TEMP         40
#define DEFAULT_DHW_MIN_TEMP            30
#define DEFAULT_DHW_MAX_TEMP            60

#define THERMOSTAT_INDOOR_DEFAULT_TEMP  20
#define THERMOSTAT_INDOOR_MIN_TEMP      5
#define THERMOSTAT_INDOOR_MAX_TEMP      40

#define DEFAULT_NTC_NOMINAL_RESISTANCE  10000.0f
#define DEFAULT_NTC_NOMINAL_TEMP        25.0f
#define DEFAULT_NTC_REF_RESISTANCE      10000.0f
#define DEFAULT_NTC_BETA_FACTOR         3950.0f
#ifndef DEFAULT_NTC_VREF
  #define DEFAULT_NTC_VREF                3300.0f
#endif
#define DEFAULT_NTC_VLOW_THRESHOLD      25.0f
#define DEFAULT_NTC_VHIGH_THRESHOLD     3298.0f

#ifndef BUILD_VERSION
  #define BUILD_VERSION                 "0.0.0"
#endif

#ifndef BUILD_COMMIT
  #define BUILD_COMMIT                  "undefined"
#endif

#ifndef BUILD_ENV
  #define BUILD_ENV                     "undefined"
#endif

#ifndef LOGGING_QUEUE_SIZE
  #define LOGGING_QUEUE_SIZE 4096
#endif

#ifndef DEFAULT_SERIAL_ENABLED
  #define DEFAULT_SERIAL_ENABLED true
#endif

#ifndef DEFAULT_SERIAL_BAUD
  #define DEFAULT_SERIAL_BAUD 115200
#endif

#ifndef DEFAULT_WEBSERIAL_ENABLED
  #define DEFAULT_WEBSERIAL_ENABLED true
#endif

#ifndef USE_BLE
  #define USE_BLE false
#endif

#ifndef USE_ETHERNET
  #define USE_ETHERNET false
#endif

#ifndef DEFAULT_HOSTNAME
  #define DEFAULT_HOSTNAME ""
#endif

#ifndef DEFAULT_AP_SSID
  #define DEFAULT_AP_SSID "OpenTherm Gateway"
#endif

#ifndef DEFAULT_AP_PASSWORD
  #define DEFAULT_AP_PASSWORD "otgateway123456"
#endif

#ifndef DEFAULT_STA_SSID
  #define DEFAULT_STA_SSID ""
#endif

#ifndef DEFAULT_STA_PASSWORD
  #define DEFAULT_STA_PASSWORD ""
#endif

#ifndef DEFAULT_LOG_LEVEL
  #define DEFAULT_LOG_LEVEL ESP_LOG_VERBOSE
#endif

#ifndef DEFAULT_STATUS_LED_GPIO
  #define DEFAULT_STATUS_LED_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_ETH_CLK_GPIO
  #define DEFAULT_ETH_CLK_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_ETH_MOSI_GPIO
  #define DEFAULT_ETH_MOSI_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_ETH_MISO_GPIO
  #define DEFAULT_ETH_MISO_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_ETH_CS_GPIO
  #define DEFAULT_ETH_CS_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_ETH_IRQ_GPIO
  #define DEFAULT_ETH_IRQ_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_ETH_RST_GPIO
  #define DEFAULT_ETH_RST_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_PORTAL_LOGIN
  #define DEFAULT_PORTAL_LOGIN ""
#endif

#ifndef DEFAULT_PORTAL_PASSWORD
  #define DEFAULT_PORTAL_PASSWORD ""
#endif

#ifndef DEFAULT_MQTT_ENABLED
  #define DEFAULT_MQTT_ENABLED false
#endif

#ifndef DEFAULT_MQTT_SERVER
  #define DEFAULT_MQTT_SERVER ""
#endif

#ifndef DEFAULT_MQTT_PORT
  #define DEFAULT_MQTT_PORT 1883
#endif

#ifndef DEFAULT_MQTT_USER
  #define DEFAULT_MQTT_USER ""
#endif

#ifndef DEFAULT_MQTT_PASSWORD
  #define DEFAULT_MQTT_PASSWORD ""
#endif

#ifndef DEFAULT_MQTT_PREFIX
  #define DEFAULT_MQTT_PREFIX ""
#endif

#ifndef DEFAULT_HA_DISCOVERY_ENABLED
  #define DEFAULT_HA_DISCOVERY_ENABLED false
#endif

#ifndef DEFAULT_HA_DISCOVERY_PREFIX
  #define DEFAULT_HA_DISCOVERY_PREFIX "homeassistant"
#endif

#ifndef DEFAULT_OT_IN_GPIO
  #define DEFAULT_OT_IN_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_OT_OUT_GPIO
  #define DEFAULT_OT_OUT_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_OT_RX_LED_GPIO
  #define DEFAULT_OT_RX_LED_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_OT_FAULT_STATE_GPIO
  #define DEFAULT_OT_FAULT_STATE_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_SENSOR_OUTDOOR_GPIO
  #define DEFAULT_SENSOR_OUTDOOR_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_SENSOR_INDOOR_GPIO
  #define DEFAULT_SENSOR_INDOOR_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_SENSOR_INDOOR_TYPE
  #define DEFAULT_SENSOR_INDOOR_TYPE Sensors::Type::DALLAS_TEMP
#endif

#ifndef SENSORS_AMOUNT
  #define SENSORS_AMOUNT 20
#endif

#ifndef DEFAULT_EXT_PUMP_GPIO
  #define DEFAULT_EXT_PUMP_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef GPIO_IS_VALID_OUTPUT_GPIO
  #define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) ((gpio_num) >= 0 && (gpio_num) < SOC_GPIO_PIN_COUNT && \
                                               (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))
#endif

#ifndef GPIO_IS_VALID_INPUT_GPIO
  // ESP-IDF uses the general GPIO mask for digital input capability.
  #define GPIO_IS_VALID_INPUT_GPIO(gpio_num) GPIO_IS_VALID_GPIO(gpio_num)
#endif

#define GPIO_IS_VALID(gpioNum) ((gpioNum) != GPIO_IS_NOT_CONFIGURED && GPIO_IS_VALID_GPIO(gpioNum))
#define GPIO_IS_VALID_INPUT(gpioNum) ((gpioNum) != GPIO_IS_NOT_CONFIGURED && GPIO_IS_VALID_INPUT_GPIO(gpioNum))
#define GPIO_IS_VALID_OUTPUT(gpioNum) ((gpioNum) != GPIO_IS_NOT_CONFIGURED && GPIO_IS_VALID_OUTPUT_GPIO(gpioNum))
