const convertLegacyBackup = (data) => {
  const aliases = {
    alwaysSendIndoorTemp: 'always_send_indoor_temp',
    antiStuckInterval: 'antistuck_interval',
    antiStuckTime: 'antistuck_time',
    autoDiagReset: 'auto_diag_reset',
    autoFaultReset: 'auto_fault_reset',
    cascadeControl: 'cascade_control',
    ch2AlwaysEnabled: 'ch2_always_enabled',
    ch2DisablingAction: 'ch2_disabling_action',
    coolingSupport: 'cooling_support',
    dhwBlocking: 'dhw_blocking',
    dhwDisablingAction: 'dhw_disabling_action',
    dhwStateAsDhwBlocking: 'dhw_state_as_dhw_blocking',
    dhwSupport: 'dhw_support',
    dhwToCh2: 'dhw_to_ch2',
    externalPump: 'ext_pump',
    filteringFactor: 'filtering_factor',
    freezeProtection: 'freeze_protection',
    getMinMaxTemp: 'get_min_max_temp',
    heatingDisablingAction: 'heating_disabling_action',
    heatingStateToSummerWinterMode: 'heating_state_to_summer_winter_mode',
    heatingToCh2: 'heating_to_ch2',
    highTemp: 'high_temp',
    homeAssistantDiscovery: 'ha_discovery',
    ignoreDiagState: 'ignore_diag_state',
    immergasFix: 'immergas_fix',
    indoorTempAvgType: 'indoor_temp_avg_type',
    inGpio: 'in_gpio',
    invertState: 'invert_state',
    logLevel: 'loglevel',
    lowTemp: 'low_temp',
    maxModulation: 'max_modulation',
    maxPower: 'max_power',
    maxTemp: 'max_temp',
    maxTempSyncWithTargetTemp: 'max_temp_sync_with_target_temp',
    memberId: 'member_id',
    minPower: 'min_power',
    minTemp: 'min_temp',
    nativeOTC: 'native_otc',
    onEnabledHeating: 'on_enabled_heating',
    onFault: 'on_fault',
    onLossConnection: 'on_loss_connection',
    outdoorTempAvgType: 'outdoor_temp_avg_type',
    outGpio: 'out_gpio',
    overheatProtection: 'overheat_protection',
    postCirculationTime: 'post_circulation_time',
    rxLedGpio: 'rx_led_gpio',
    setDateAndTime: 'set_date_and_time',
    staticConfig: 'static_config',
    statusLedGpio: 'status_led_gpio',
    summerWinterMode: 'summer_winter_mode',
    targetDiffFactor: 'target_diff_factor',
    thresholdHigh: 'threshold_high',
    thresholdLow: 'threshold_low',
    thresholdTime: 'threshold_time',
    tresholdTime: 'threshold_time',
    turboFactor: 'turbo_factor',
    unitSystem: 'unit_system',
    useDhcp: 'use_dhcp',
    webSerial: 'web_serial'
  };
  const logLevels = [0, 1, 1, 2, 3, 3, 4, 5];

  const convert = (value) => {
    if (Array.isArray(value)) {
      return value.map(convert);
    }
    if (value === null || typeof value !== 'object') {
      return value;
    }

    const result = Object.create(null);
    for (const [key, field] of Object.entries(value)) {
      const convertedKey = Object.prototype.hasOwnProperty.call(aliases, key) ? aliases[key] : key;
      result[convertedKey] = convert(field);
    }
    return result;
  };

  const legacySystem = data?.settings?.system;
  if (legacySystem !== null && typeof legacySystem === 'object'
      && Object.prototype.hasOwnProperty.call(legacySystem, 'logLevel')) {
    const system = {...legacySystem};
    if (Object.prototype.hasOwnProperty.call(system, 'loglevel')) {
      delete system.logLevel;

    } else if (Number.isInteger(system.logLevel) && system.logLevel >= 0 && system.logLevel < logLevels.length) {
      system.logLevel = logLevels[system.logLevel];
    }

    data = {
      ...data,
      settings: {...data.settings, system}
    };
  }

  const result = convert(data);

  const network = result?.network;
  if (network !== null && typeof network === 'object' && !Array.isArray(network)) {
    const hasLegacyAp = Object.prototype.hasOwnProperty.call(network, 'ap');
    const hasLegacySta = Object.prototype.hasOwnProperty.call(network, 'sta');
    if (hasLegacyAp || hasLegacySta) {
      const wifi = network.wifi !== null && typeof network.wifi === 'object' && !Array.isArray(network.wifi)
        ? network.wifi : Object.create(null);

      if (hasLegacyAp && !Object.prototype.hasOwnProperty.call(wifi, 'ap')) {
        wifi.ap = network.ap;
      }
      if (hasLegacySta && !Object.prototype.hasOwnProperty.call(wifi, 'sta')) {
        wifi.sta = network.sta;
      }
      network.wifi = wifi;
      delete network.ap;
      delete network.sta;
    }

    if (!Object.prototype.hasOwnProperty.call(network, 'type')) {
      if (typeof network.ethernet?.enabled === 'boolean') {
        network.type = network.ethernet.enabled ? 3 : 2;

      } else if (hasLegacyAp || hasLegacySta) {
        network.type = 2;
      }
    }

    if (network.ethernet !== null && typeof network.ethernet === 'object' && !Array.isArray(network.ethernet)) {
      if (Object.prototype.hasOwnProperty.call(network.ethernet, 'pins')
          && !Object.prototype.hasOwnProperty.call(network.ethernet, 'gpio')) {
        network.ethernet.gpio = network.ethernet.pins;
      }
      delete network.ethernet.pins;
      delete network.ethernet.enabled;
    }
  }

  const opentherm = result?.settings?.opentherm;
  if (opentherm !== null && typeof opentherm === 'object') {
    delete opentherm.unit_system;
  }

  return result;
};
