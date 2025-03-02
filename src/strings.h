#pragma once
#ifndef PROGMEM
  #define PROGMEM 
#endif

const char L_SETTINGS[]                             PROGMEM = "SETTINGS";
const char L_SETTINGS_OT[]                          PROGMEM = "SETTINGS.OT";
const char L_SETTINGS_DHW[]                         PROGMEM = "SETTINGS.DHW";
const char L_SETTINGS_HEATING[]                     PROGMEM = "SETTINGS.HEATING";
const char L_NETWORK[]                              PROGMEM = "NETWORK";
const char L_NETWORK_SETTINGS[]                     PROGMEM = "NETWORK.SETTINGS";
const char L_PORTAL_WEBSERVER[]                     PROGMEM = "PORTAL.WEBSERVER";
const char L_PORTAL_DNSSERVER[]                     PROGMEM = "PORTAL.DNSSERVER";
const char L_PORTAL_CAPTIVE[]                       PROGMEM = "PORTAL.CAPTIVE";
const char L_PORTAL_OTA[]                           PROGMEM = "PORTAL.OTA";
const char L_MAIN[]                                 PROGMEM = "MAIN";
const char L_MQTT[]                                 PROGMEM = "MQTT";
const char L_MQTT_HA[]                              PROGMEM = "MQTT.HA";
const char L_MQTT_MSG[]                             PROGMEM = "MQTT.MSG";
const char L_OT[]                                   PROGMEM = "OT";
const char L_OT_DHW[]                               PROGMEM = "OT.DHW";
const char L_OT_HEATING[]                           PROGMEM = "OT.HEATING";
const char L_OT_CH2[]                               PROGMEM = "OT.CH2";
const char L_SENSORS[]                              PROGMEM = "SENSORS";
const char L_SENSORS_SETTINGS[]                     PROGMEM = "SENSORS.SETTINGS";
const char L_SENSORS_DALLAS[]                       PROGMEM = "SENSORS.DALLAS";
const char L_SENSORS_NTC[]                          PROGMEM = "SENSORS.NTC";
const char L_SENSORS_BLE[]                          PROGMEM = "SENSORS.BLE";
const char L_REGULATOR[]                            PROGMEM = "REGULATOR";
const char L_REGULATOR_PID[]                        PROGMEM = "REGULATOR.PID";
const char L_REGULATOR_EQUITHERM[]                  PROGMEM = "REGULATOR.EQUITHERM";
const char L_CASCADE_INPUT[]                        PROGMEM = "CASCADE.INPUT";
const char L_CASCADE_OUTPUT[]                       PROGMEM = "CASCADE.OUTPUT";
const char L_EXTPUMP[]                              PROGMEM = "EXTPUMP";


const char S_ACTIONS[]                              PROGMEM = "actions";
const char S_ACTIVE[]                               PROGMEM = "active";
const char S_ADDRESS[]                              PROGMEM = "address";
const char S_ANTI_STUCK_INTERVAL[]                  PROGMEM = "antiStuckInterval";
const char S_ANTI_STUCK_TIME[]                      PROGMEM = "antiStuckTime";
const char S_AP[]                                   PROGMEM = "ap";
const char S_APP_VERSION[]                          PROGMEM = "appVersion";
const char S_AUTH[]                                 PROGMEM = "auth";
const char S_BACKTRACE[]                            PROGMEM = "backtrace";
const char S_BATTERY[]                              PROGMEM = "battery";
const char S_BAUDRATE[]                             PROGMEM = "baudrate";
const char S_BLOCKING[]                             PROGMEM = "blocking";
const char S_BSSID[]                                PROGMEM = "bssid";
const char S_BUILD[]                                PROGMEM = "build";
const char S_CASCADE_CONTROL[]                      PROGMEM = "cascadeControl";
const char S_CHANNEL[]                              PROGMEM = "channel";
const char S_CH2_ALWAYS_ENABLED[]                   PROGMEM = "ch2AlwaysEnabled";
const char S_CHIP[]                                 PROGMEM = "chip";
const char S_CODE[]                                 PROGMEM = "code";
const char S_CONNECTED[]                            PROGMEM = "connected";
const char S_CONTINUES[]                            PROGMEM = "continues";
const char S_COOLING[]                              PROGMEM = "cooling";
const char S_COOLING_SUPPORT[]                      PROGMEM = "coolingSupport";
const char S_CORE[]                                 PROGMEM = "core";
const char S_CORES[]                                PROGMEM = "cores";
const char S_CRASH[]                                PROGMEM = "crash";
const char S_CURRENT_TEMP[]                         PROGMEM = "currentTemp";
const char S_DATA[]                                 PROGMEM = "data";
const char S_DATE[]                                 PROGMEM = "date";
const char S_DEADBAND[]                             PROGMEM = "deadband";
const char S_DHW[]                                  PROGMEM = "dhw";
const char S_DHW_BLOCKING[]                         PROGMEM = "dhwBlocking";
const char S_DHW_SUPPORT[]                          PROGMEM = "dhwSupport";
const char S_DHW_TO_CH2[]                           PROGMEM = "dhwToCh2";
const char S_DIAG[]                                 PROGMEM = "diag";
const char S_DNS[]                                  PROGMEM = "dns";
const char S_DT[]                                   PROGMEM = "dt";
const char S_D_FACTOR[]                             PROGMEM = "d_factor";
const char S_D_MULTIPLIER[]                         PROGMEM = "d_multiplier";
const char S_EMERGENCY[]                            PROGMEM = "emergency";
const char S_ENABLED[]                              PROGMEM = "enabled";
const char S_ENV[]                                  PROGMEM = "env";
const char S_EPC[]                                  PROGMEM = "epc";
const char S_EQUITHERM[]                            PROGMEM = "equitherm";
const char S_EXPONENT[]                             PROGMEM = "exponent";
const char S_EXTERNAL_PUMP[]                        PROGMEM = "externalPump";
const char S_FACTOR[]                               PROGMEM = "factor";
const char S_FAULT[]                                PROGMEM = "fault";
const char S_FILTERING[]                            PROGMEM = "filtering";
const char S_FILTERING_FACTOR[]                     PROGMEM = "filteringFactor";
const char S_FLAGS[]                                PROGMEM = "flags";
const char S_FLAME[]                                PROGMEM = "flame";
const char S_FLASH[]                                PROGMEM = "flash";
const char S_FREE[]                                 PROGMEM = "free";
const char S_FREQ[]                                 PROGMEM = "freq";
const char S_GATEWAY[]                              PROGMEM = "gateway";
const char S_GET_MIN_MAX_TEMP[]                     PROGMEM = "getMinMaxTemp";
const char S_GPIO[]                                 PROGMEM = "gpio";
const char S_HEAP[]                                 PROGMEM = "heap";
const char S_HEATING[]                              PROGMEM = "heating";
const char S_HEATING_TO_CH2[]                       PROGMEM = "heatingToCh2";
const char S_HEATING_STATE_TO_SUMMER_WINTER_MODE[]  PROGMEM = "heatingStateToSummerWinterMode";
const char S_HIDDEN[]                               PROGMEM = "hidden";
const char S_HOME_ASSISTANT_DISCOVERY[]             PROGMEM = "homeAssistantDiscovery";
const char S_HOSTNAME[]                             PROGMEM = "hostname";
const char S_HUMIDITY[]                             PROGMEM = "humidity";
const char S_HYSTERESIS[]                           PROGMEM = "hysteresis";
const char S_ID[]                                   PROGMEM = "id";
const char S_IMMERGAS_FIX[]                         PROGMEM = "immergasFix";
const char S_INDOOR_TEMP[]                          PROGMEM = "indoorTemp";
const char S_INDOOR_TEMP_CONTROL[]                  PROGMEM = "indoorTempControl";
const char S_IN_GPIO[]                              PROGMEM = "inGpio";
const char S_INPUT[]                                PROGMEM = "input";
const char S_INTERVAL[]                             PROGMEM = "interval";
const char S_INVERT_STATE[]                         PROGMEM = "invertState";
const char S_IP[]                                   PROGMEM = "ip";
const char S_I_FACTOR[]                             PROGMEM = "i_factor";
const char S_I_MULTIPLIER[]                         PROGMEM = "i_multiplier";
const char S_LOGIN[]                                PROGMEM = "login";
const char S_LOG_LEVEL[]                            PROGMEM = "logLevel";
const char S_MAC[]                                  PROGMEM = "mac";
const char S_MASTER[]                               PROGMEM = "master";
const char S_MAX[]                                  PROGMEM = "max";
const char S_MAX_FREE_BLOCK[]                       PROGMEM = "maxFreeBlock";
const char S_MAX_MODULATION[]                       PROGMEM = "maxModulation";
const char S_MAX_POWER[]                            PROGMEM = "maxPower";
const char S_MAX_TEMP[]                             PROGMEM = "maxTemp";
const char S_MAX_TEMP_SYNC_WITH_TARGET_TEMP[]       PROGMEM = "maxTempSyncWithTargetTemp";
const char S_MDNS[]                                 PROGMEM = "mdns";
const char S_MEMBER_ID[]                            PROGMEM = "memberId";
const char S_MIN[]                                  PROGMEM = "min";
const char S_MIN_FREE[]                             PROGMEM = "minFree";
const char S_MIN_MAX_FREE_BLOCK[]                   PROGMEM = "minMaxFreeBlock";
const char S_MIN_POWER[]                            PROGMEM = "minPower";
const char S_MIN_TEMP[]                             PROGMEM = "minTemp";
const char S_MODEL[]                                PROGMEM = "model";
const char S_MODULATION[]                           PROGMEM = "modulation";
const char S_MODULATION_SYNC_WITH_HEATING[]         PROGMEM = "modulationSyncWithHeating";
const char S_MQTT[]                                 PROGMEM = "mqtt";
const char S_NAME[]                                 PROGMEM = "name";
const char S_NATIVE_HEATING_CONTROL[]               PROGMEM = "nativeHeatingControl";
const char S_NETWORK[]                              PROGMEM = "network";
const char S_NTP[]                                  PROGMEM = "ntp";
const char S_OFFSET[]                               PROGMEM = "offset";
const char S_ON_ENABLED_HEATING[]                   PROGMEM = "onEnabledHeating";
const char S_ON_FAULT[]                             PROGMEM = "onFault";
const char S_ON_LOSS_CONNECTION[]                   PROGMEM = "onLossConnection";
const char S_OPENTHERM[]                            PROGMEM = "opentherm";
const char S_OPTIONS[]                              PROGMEM = "options";
const char S_OUTDOOR_TEMP[]                         PROGMEM = "outdoorTemp";
const char S_OUT_GPIO[]                             PROGMEM = "outGpio";
const char S_OUTPUT[]                               PROGMEM = "output";
const char S_PASSWORD[]                             PROGMEM = "password";
const char S_PID[]                                  PROGMEM = "pid";
const char S_PORT[]                                 PROGMEM = "port";
const char S_PORTAL[]                               PROGMEM = "portal";
const char S_POST_CIRCULATION_TIME[]                PROGMEM = "postCirculationTime";
const char S_POWER[]                                PROGMEM = "power";
const char S_PREFIX[]                               PROGMEM = "prefix";
const char S_PROTOCOL_VERSION[]                     PROGMEM = "protocolVersion";
const char S_PURPOSE[]                              PROGMEM = "purpose";
const char S_P_FACTOR[]                             PROGMEM = "p_factor";
const char S_P_MULTIPLIER[]                         PROGMEM = "p_multiplier";
const char S_REAL_SIZE[]                            PROGMEM = "realSize";
const char S_REASON[]                               PROGMEM = "reason";
const char S_RESET_DIAGNOSTIC[]                     PROGMEM = "resetDiagnostic";
const char S_RESET_FAULT[]                          PROGMEM = "resetFault";
const char S_RESET_REASON[]                         PROGMEM = "resetReason";
const char S_RESTART[]                              PROGMEM = "restart";
const char S_RETURN_TEMP[]                          PROGMEM = "returnTemp";
const char S_REV[]                                  PROGMEM = "rev";
const char S_RSSI[]                                 PROGMEM = "rssi";
const char S_RX_LED_GPIO[]                          PROGMEM = "rxLedGpio";
const char S_SDK[]                                  PROGMEM = "sdk";
const char S_SENSORS[]                              PROGMEM = "sensors";
const char S_SERIAL[]                               PROGMEM = "serial";
const char S_SERVER[]                               PROGMEM = "server";
const char S_SETTINGS[]                             PROGMEM = "settings";
const char S_SHIFT[]                                PROGMEM = "shift";
const char S_SIGNAL_QUALITY[]                       PROGMEM = "signalQuality";
const char S_SIZE[]                                 PROGMEM = "size";
const char S_SLAVE[]                                PROGMEM = "slave";
const char S_SLOPE[]                                PROGMEM = "slope";
const char S_SSID[]                                 PROGMEM = "ssid";
const char S_STA[]                                  PROGMEM = "sta";
const char S_STATE[]                                PROGMEM = "state";
const char S_STATIC_CONFIG[]                        PROGMEM = "staticConfig";
const char S_STATUS_LED_GPIO[]                      PROGMEM = "statusLedGpio";
const char S_SETPOINT_TEMP[]                        PROGMEM = "setpointTemp";
const char S_SUBNET[]                               PROGMEM = "subnet";
const char S_SUMMER_WINTER_MODE[]                   PROGMEM = "summerWinterMode";
const char S_SYSTEM[]                               PROGMEM = "system";
const char S_TARGET[]                               PROGMEM = "target";
const char S_TARGET_DIFF_FACTOR[]                   PROGMEM = "targetDiffFactor";
const char S_TARGET_TEMP[]                          PROGMEM = "targetTemp";
const char S_TELNET[]                               PROGMEM = "telnet";
const char S_TEMPERATURE[]                          PROGMEM = "temperature";
const char S_THRESHOLD_HIGH[]                       PROGMEM = "thresholdHigh";
const char S_THRESHOLD_LOW[]                        PROGMEM = "thresholdLow";
const char S_THRESHOLD_TIME[]                       PROGMEM = "thresholdTime";
const char S_TIMEZONE[]                             PROGMEM = "timezone";
const char S_TOTAL[]                                PROGMEM = "total";
const char S_TRESHOLD_TIME[]                        PROGMEM = "tresholdTime";
const char S_TURBO[]                                PROGMEM = "turbo";
const char S_TURBO_FACTOR[]                         PROGMEM = "turboFactor";
const char S_TYPE[]                                 PROGMEM = "type";
const char S_UNIT_SYSTEM[]                          PROGMEM = "unitSystem";
const char S_UPTIME[]                               PROGMEM = "uptime";
const char S_USE[]                                  PROGMEM = "use";
const char S_USE_DHCP[]                             PROGMEM = "useDhcp";
const char S_USER[]                                 PROGMEM = "user";
const char S_VALUE[]                                PROGMEM = "value";
const char S_VERSION[]                              PROGMEM = "version";
