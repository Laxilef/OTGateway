#pragma once
#ifndef PROGMEM
  #define PROGMEM 
#endif

const char HA_ENTITY_BINARY_SENSOR[]            PROGMEM = "binary_sensor";
const char HA_ENTITY_BUTTON[]                   PROGMEM = "button";
const char HA_ENTITY_FAN[]                      PROGMEM = "fan";
const char HA_ENTITY_CLIMATE[]                  PROGMEM = "climate";
const char HA_ENTITY_NUMBER[]                   PROGMEM = "number";
const char HA_ENTITY_SELECT[]                   PROGMEM = "select";
const char HA_ENTITY_SENSOR[]                   PROGMEM = "sensor";
const char HA_ENTITY_SWITCH[]                   PROGMEM = "switch";

// https://www.home-assistant.io/integrations/mqtt/#supported-abbreviations-in-mqtt-discovery-messages
const char HA_DEFAULT_ENTITY_ID[]               PROGMEM = "def_ent_id";               // "default_entity_id "
const char HA_DEVICE[]                          PROGMEM = "dev";                      // "device"
const char HA_IDENTIFIERS[]                     PROGMEM = "ids";                      // "identifiers"
const char HA_SW_VERSION[]                      PROGMEM = "sw";                       // "sw_version"
const char HA_MANUFACTURER[]                    PROGMEM = "mf";                       // "manufacturer"
const char HA_MODEL[]                           PROGMEM = "mdl";                      // "model"
const char HA_NAME[]                            PROGMEM = "name";
const char HA_CONF_URL[]                        PROGMEM = "cu";                       // "configuration_url"
const char HA_COMMAND_TOPIC[]                   PROGMEM = "cmd_t";                    // "command_topic"
const char HA_COMMAND_TEMPLATE[]                PROGMEM = "cmd_tpl";                  // "command_template"
const char HA_ENABLED_BY_DEFAULT[]              PROGMEM = "en";                       // "enabled_by_default"
const char HA_UNIQUE_ID[]                       PROGMEM = "uniq_id";                  // "unique_id"
const char HA_ENTITY_CATEGORY[]                 PROGMEM = "ent_cat";                  // "entity_category"
const char HA_ENTITY_CATEGORY_DIAGNOSTIC[]      PROGMEM = "diagnostic";
const char HA_ENTITY_CATEGORY_CONFIG[]          PROGMEM = "config";
const char HA_STATE_TOPIC[]                     PROGMEM = "stat_t";                   // "state_topic"
const char HA_VALUE_TEMPLATE[]                  PROGMEM = "val_tpl";                  // "value_template"
const char HA_OPTIONS[]                         PROGMEM = "ops";                      // "options"
const char HA_AVAILABILITY[]                    PROGMEM = "avty";                     // "availability"
const char HA_AVAILABILITY_MODE[]               PROGMEM = "avty_mode";                // "availability_mode"
const char HA_TOPIC[]                           PROGMEM = "t";                        // "topic"
const char HA_DEVICE_CLASS[]                    PROGMEM = "dev_cla";                  // "device_class"
const char HA_UNIT_OF_MEASUREMENT[]             PROGMEM = "unit_of_meas";             // "unit_of_measurement"
const char HA_UNIT_OF_MEASUREMENT_C[]           PROGMEM = "°C";
const char HA_UNIT_OF_MEASUREMENT_F[]           PROGMEM = "°F";
const char HA_UNIT_OF_MEASUREMENT_PERCENT[]     PROGMEM = "%";
const char HA_UNIT_OF_MEASUREMENT_L_MIN[]       PROGMEM = "L/min";
const char HA_UNIT_OF_MEASUREMENT_GAL_MIN[]     PROGMEM = "gal/min";
const char HA_ICON[]                            PROGMEM = "ic";                       // "icon"
const char HA_MIN[]                             PROGMEM = "min";
const char HA_MAX[]                             PROGMEM = "max";
const char HA_STEP[]                            PROGMEM = "step";
const char HA_MODE[]                            PROGMEM = "mode";
const char HA_MODE_BOX[]                        PROGMEM = "box";
const char HA_STATE_ON[]                        PROGMEM = "stat_on";                  // "state_on"
const char HA_STATE_OFF[]                       PROGMEM = "stat_off";                 // "state_off"
const char HA_PAYLOAD_ON[]                      PROGMEM = "pl_on";                    // "payload_on"
const char HA_PAYLOAD_OFF[]                     PROGMEM = "pl_off";                   // "payload_off"
const char HA_STATE_CLASS[]                     PROGMEM = "stat_cla";                 // "state_class"
const char HA_STATE_CLASS_MEASUREMENT[]         PROGMEM = "measurement";
const char HA_EXPIRE_AFTER[]                    PROGMEM = "exp_aft";                  // "expire_after"
const char HA_CURRENT_TEMPERATURE_TOPIC[]       PROGMEM = "curr_temp_t";              // "current_temperature_topic"
const char HA_CURRENT_TEMPERATURE_TEMPLATE[]    PROGMEM = "curr_temp_tpl";            // "current_temperature_template"
const char HA_TEMPERATURE_COMMAND_TOPIC[]       PROGMEM = "temp_cmd_t";               // "temperature_command_topic"
const char HA_TEMPERATURE_COMMAND_TEMPLATE[]    PROGMEM = "temp_cmd_tpl";             // "temperature_command_template"
const char HA_TEMPERATURE_STATE_TOPIC[]         PROGMEM = "temp_stat_t";              // "temperature_state_topic"
const char HA_TEMPERATURE_STATE_TEMPLATE[]      PROGMEM = "temp_stat_tpl";            // "temperature_state_template"
const char HA_TEMPERATURE_UNIT[]                PROGMEM = "temp_unit";                // "temperature_unit"
const char HA_MODE_COMMAND_TOPIC[]              PROGMEM = "mode_cmd_t";               // "mode_command_topic"
const char HA_MODE_COMMAND_TEMPLATE[]           PROGMEM = "mode_cmd_tpl";             // "mode_command_template"
const char HA_MODE_STATE_TOPIC[]                PROGMEM = "mode_stat_t";              // "mode_state_topic"
const char HA_MODE_STATE_TEMPLATE[]             PROGMEM = "mode_stat_tpl";            // "mode_state_template"
const char HA_MODES[]                           PROGMEM = "modes";
const char HA_ACTION_TOPIC[]                    PROGMEM = "act_t";                    // "action_topic"
const char HA_ACTION_TEMPLATE[]                 PROGMEM = "act_tpl";                  // "action_template"
const char HA_MIN_TEMP[]                        PROGMEM = "min_temp";
const char HA_MAX_TEMP[]                        PROGMEM = "max_temp";
const char HA_TEMP_STEP[]                       PROGMEM = "temp_step";
const char HA_PRESET_MODE_COMMAND_TOPIC[]       PROGMEM = "pr_mode_cmd_t";            // "preset_mode_command_topic"
const char HA_PRESET_MODE_COMMAND_TEMPLATE[]    PROGMEM = "pr_mode_cmd_tpl";          // "preset_mode_command_template"
const char HA_PRESET_MODE_STATE_TOPIC[]         PROGMEM = "pr_mode_stat_t";           // "preset_mode_state_topic"
const char HA_PRESET_MODE_VALUE_TEMPLATE[]      PROGMEM = "pr_mode_val_tpl";          // "preset_mode_value_template"
const char HA_PRESET_MODES[]                    PROGMEM = "pr_modes";                 // "preset_modes"
