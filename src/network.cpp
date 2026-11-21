#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <Network.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstdlib>
#include <vector>

// Requests and state shared with other workers.
static std::atomic<bool> reconnectRequested{false};
static std::atomic<bool> staStarted{false};
static std::atomic<bool> apStarted{false};
static std::atomic<NetworkState> staState{NetworkState::NETWORK_DISABLED};
static std::atomic<NetworkState> apState{NetworkState::NETWORK_DISABLED};

// Subscriptions are fixed before the worker starts.
static std::vector<NetworkEventCallback> networkCallbacks;
static std::vector<NetworkStateCallback> stateCallbacks;

// Framework callbacks only copy events. Driver and service operations stay in this worker.
struct PendingNetworkEvent {
  uint32_t value; // IPv4 address for STA_GOT_IP and AP_STARTED.
  NetworkEvent event;
  uint8_t reason; // STA disconnect reason.
};

static constexpr size_t NETWORK_EVENT_QUEUE_LENGTH = 32;
static StaticQueue_t networkEventQueueStorage;
static uint8_t networkEventBuffer[NETWORK_EVENT_QUEUE_LENGTH * sizeof(PendingNetworkEvent)];
static QueueHandle_t networkEventQueue = nullptr;
static std::atomic<bool> networkEventsOverflowed{false};
static std::atomic<bool> networkRecoveryRequested{false};

// Station state and timers; only the system worker accesses these values.
static unsigned long lastConnectionAttempt = 0;
static unsigned long connectedTime = 0;
static unsigned long disconnectedTime = 0;
static unsigned long lastRssiUpdate = 0;
static uint32_t connectedIp = 0;
static uint8_t disconnectReason = 0;
static bool staAssociated = false;
static bool connectionPending = false;
static bool ntpStarted = false;

// Fallback access point
static unsigned long lastApAttempt = 0;
static uint32_t apIp = 0;
static uint8_t apClients = 0;

// Full driver recovery
static unsigned long lastNetworkRecoveryAttempt = 0;
static bool recoveryPending = false;

// DNS is asynchronous in the current framework; it needs no polling task.
static bool webServerReady = false;
static DNSServer dnsServer;
static bool dnsStarted = false;
static bool mdnsStarted = false;
static unsigned long lastDnsAttempt = 0;
static unsigned long lastMdnsAttempt = 0;
static char mdnsHostname[sizeof(networkSettings.hostname)] = {};

// Event subscriptions
void onNetworkEvent(NetworkEventCallback callback) {
  if (!callback) {
    return;
  }

  for (auto registered : networkCallbacks) {
    if (registered == callback) {
      return;
    }
  }

  networkCallbacks.push_back(callback);
}

void onNetworkStateChange(NetworkStateCallback callback) {
  if (callback) {
    stateCallbacks.push_back(std::move(callback));
  }
}

// Published interface state
NetworkState getNetworkState(NetworkMode mode) {
  if (mode == NetworkMode::STA) {
    return staState.load();
  }

  if (mode == NetworkMode::AP) {
    return apState.load();
  }

  // Ethernet initialization and routing will be implemented separately.
  return NetworkState::NETWORK_DISABLED;
}

bool isNetworkConnected() {
  return getNetworkState(NetworkMode::STA) == NetworkState::NETWORK_CONNECTED;
}

NetworkMode getNetworkMode() {
  if (isNetworkConnected()) {
    return NetworkMode::STA;
  }

  return apStarted.load() ? NetworkMode::AP : NetworkMode::NONE;
}

bool isNetworkInterfaceStarted(NetworkMode mode) {
  if (mode == NetworkMode::STA) {
    return staStarted.load();
  }

  return mode == NetworkMode::AP && apStarted.load();
}

// Requests from other tasks; driver operations run in the system worker.
void reconnectNetwork() {
  reconnectRequested.store(true);
}

// State transitions and notifications
static void notifyNetworkEvent(NetworkEvent event) {
  for (auto callback : networkCallbacks) {
    callback(event);
  }
}

static void setNetworkState(NetworkMode mode, NetworkState next) {
  auto& state = mode == NetworkMode::STA ? staState : apState;
  const NetworkState previous = state.load();

  if (previous == next) {
    return;
  }

  if (mode == NetworkMode::STA) {
    if (previous == NetworkState::NETWORK_CONNECTED) {
      disconnectedTime = millis();
      connectedIp = 0;
      vars.network.rssi = 0;
      ntpStarted = false;
      Sensors::setConnectionStatusByType(Sensors::Type::MANUAL, false, false);
    }

    if (next == NetworkState::NETWORK_CONNECTED) {
      connectedTime = millis();
    }
  }

  // Publish before callbacks so getters and JSON already reflect the new state.
  state.store(next);
  for (auto& callback : stateCallbacks) {
    callback(mode, previous, next);
  }

  if (mode == NetworkMode::STA) {
    if (next == NetworkState::NETWORK_CONNECTED) {
      notifyNetworkEvent(NetworkEvent::CONNECTED);

    } else if (previous == NetworkState::NETWORK_CONNECTED) {
      notifyNetworkEvent(NetworkEvent::DISCONNECTED);
    }
  }
}

// Framework events
static void queueNetworkEvent(NetworkEvent event, uint32_t value = 0, uint8_t reason = 0) {
  const PendingNetworkEvent pending{value, event, reason};

  // Never wait here: this worker can be waiting for the framework inside STA/AP.begin().
  if (xQueueSend(networkEventQueue, &pending, 0) != pdTRUE) {
    networkEventsOverflowed.store(true);
  }
}

static void handleWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  // This callback runs in the Arduino event task; only copy the event data here.
  switch (event) {
    // Station
    case ARDUINO_EVENT_WIFI_STA_START:
      queueNetworkEvent(NetworkEvent::STA_STARTED);
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
      queueNetworkEvent(NetworkEvent::STA_STOPPED);
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      queueNetworkEvent(NetworkEvent::STA_CONNECTED);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      queueNetworkEvent(NetworkEvent::STA_DISCONNECTED, 0, info.wifi_sta_disconnected.reason);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      queueNetworkEvent(NetworkEvent::STA_GOT_IP, info.got_ip.ip_info.ip.addr);
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      queueNetworkEvent(NetworkEvent::STA_LOST_IP);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
      queueNetworkEvent(NetworkEvent::STA_GOT_IP6);
      break;

    // Access point
    case ARDUINO_EVENT_WIFI_AP_START:
      queueNetworkEvent(NetworkEvent::AP_STARTED, static_cast<uint32_t>(WiFi.AP.localIP()));
      break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
      queueNetworkEvent(NetworkEvent::AP_STOPPED);
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      queueNetworkEvent(NetworkEvent::AP_CLIENT_CONNECTED);
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      queueNetworkEvent(NetworkEvent::AP_CLIENT_DISCONNECTED);
      break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      queueNetworkEvent(NetworkEvent::AP_CLIENT_GOT_IP);
      break;
    case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
      queueNetworkEvent(NetworkEvent::AP_GOT_IP6);
      break;

    default:
      break;
  }
}

static void postInterfaceStopped(arduino_event_id_t type) {
  // onDisable() can remove the native STOP handler. Keep our notification ordered
  // after already queued framework START/GOT_IP events and before the next start.
  arduino_event_t event{};
  event.event_id = type;
  if (!Network.postEvent(&event)) {
    ESP_LOGE(L_NETWORK, "Failed post interface stop, reset WiFi requested");
    networkRecoveryRequested.store(true);
  }
}

// WiFi interfaces
static void configureWifiRadio() {
  // Apply the existing channel policy after the WiFi driver has started.
  const wifi_country_t country = {
    "JP",
    1,
    14,
    CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER,
    WIFI_COUNTRY_POLICY_MANUAL
  };

  esp_wifi_set_country(&country);
  //WiFi.setSleep(USE_BLE ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
  WiFi.setSleep(WIFI_PS_NONE);
}

static void stopAccessPoint() {
  if (!WiFi.AP.end()) {
    ESP_LOGE(L_NETWORK, "Failed stop AP, reset WiFi requested");
    networkRecoveryRequested.store(true);
    return;
  }

  postInterfaceStopped(ARDUINO_EVENT_WIFI_AP_STOP);
}

static void startAccessPoint() {
  lastApAttempt = millis();
  setNetworkState(NetworkMode::AP, NetworkState::AP_STARTING);

  // Keep STA available for scans and retries while the fallback AP is running.
  WiFi.STA.begin();
  if (!WiFi.AP.begin()) {
    // begin() can enable AP but time out before its STARTED event arrives.
    stopAccessPoint();
    setNetworkState(NetworkMode::AP, NetworkState::NETWORK_DISABLED);
    ESP_LOGE(L_NETWORK, "Failed start AP");
    return;
  }

  configureWifiRadio();
  WiFi.AP.setHostname(networkSettings.hostname);
  if (!WiFi.AP.create(
    networkSettings.ap.ssid,
    networkSettings.ap.password[0] ? networkSettings.ap.password : nullptr,
    networkSettings.ap.channel
  )) {
    stopAccessPoint();
    setNetworkState(NetworkMode::AP, NetworkState::NETWORK_DISABLED);
    ESP_LOGE(L_NETWORK, "Failed configure AP");
  }
}

static bool resetWifi() {
  const bool restoreAp = WiFi.AP.started();
  connectionPending = false;

  // A failed driver reset must also respect the retry cooldown.
  lastConnectionAttempt = millis();
  if (!WiFi.mode(WIFI_OFF)) {
    ESP_LOGE(L_NETWORK, "Failed reset WiFi");
    return false;
  }

  postInterfaceStopped(ARDUINO_EVENT_WIFI_STA_STOP);
  if (restoreAp) {
    postInterfaceStopped(ARDUINO_EVENT_WIFI_AP_STOP);
  }

  lastConnectionAttempt = 0;
  if (restoreAp) {
    startAccessPoint();
  }

  return true;
}

static bool startNetworkConnection() {
  // Ordinary retries preserve AP. Escalate only if STA cannot be stopped.
  const bool wasStarted = WiFi.STA.started();

  if (!WiFi.STA.end()) {
    ESP_LOGW(L_NETWORK, "Failed stop STA, reset WiFi...");
    if (!resetWifi()) {
      return false;
    }

  } else if (wasStarted) {
    postInterfaceStopped(ARDUINO_EVENT_WIFI_STA_STOP);
  }

  if (!WiFi.STA.begin()) {
    return false;
  }

  configureWifiRadio();
  WiFi.STA.setAutoReconnect(false);
  WiFi.STA.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.STA.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  if (!WiFi.STA.setHostname(networkSettings.hostname)) {
    ESP_LOGE(L_NETWORK, "Set hostname '%s': fail", networkSettings.hostname);
  }

  if (networkSettings.useDhcp) {
    if (!WiFi.STA.config()) {
      return false;
    }

  } else {
    IPAddress ip, gateway, subnet, dns;
    ip.fromString(networkSettings.staticConfig.ip);
    gateway.fromString(networkSettings.staticConfig.gateway);
    subnet.fromString(networkSettings.staticConfig.subnet);
    dns.fromString(networkSettings.staticConfig.dns);

    if (!WiFi.STA.config(ip, gateway, subnet, dns)) {
      return false;
    }
  }

  setNetworkState(
    NetworkMode::STA,
    connectedTime ? NetworkState::NETWORK_RECONNECTING : NetworkState::NETWORK_CONNECTING
  );

  return WiFi.STA.connect(
    networkSettings.sta.ssid,
    networkSettings.sta.password[0] ? networkSettings.sta.password : nullptr,
    networkSettings.sta.channel
  );
}

// Station connection and recovery timeouts
static void updateConnection() {
  if (!networkSettings.sta.ssid[0]) {
    return;
  }

  if (connectionPending) {
    if (millis() - lastConnectionAttempt < 10000) {
      return;
    }

    connectionPending = false;
    lastConnectionAttempt = millis();
    setNetworkState(NetworkMode::STA, NetworkState::NETWORK_TIMEOUT);

    ESP_LOGD(
      L_NETWORK,
      "Connection attempt timed out. Status: %d",
      WiFi.STA.status()
    );
  }

  // Associated without IP: allow DHCP to recover before resetting the whole driver.
  if (staAssociated) {
    if (millis() - lastConnectionAttempt > 90000) {
      ESP_LOGW(L_NETWORK, "Connection timeout, reset WiFi...");
      resetWifi();
    }

    return;
  }

  if (!lastConnectionAttempt || millis() - lastConnectionAttempt > 15000) {
    ESP_LOGI(L_NETWORK, "Try connect...");

    connectionPending = startNetworkConnection();
    lastConnectionAttempt = millis();

    if (!connectionPending) {
      setNetworkState(NetworkMode::STA, NetworkState::NETWORK_TIMEOUT);
      ESP_LOGW(L_NETWORK, "Failed start connection");
    }
  }
}

// Fallback access point
static void updateAccessPoint() {
  if (isNetworkConnected()) {
    if (apStarted.load() && millis() - connectedTime > 15000 && !apClients) {
      ESP_LOGI(L_NETWORK, "Stop AP because STA connected");
      stopAccessPoint();
    }

    return;
  }

  if (!apStarted.load() && (!networkSettings.sta.ssid[0] || millis() - disconnectedTime > 185000)) {
    if (!lastApAttempt || millis() - lastApAttempt > 15000) {
      ESP_LOGI(L_NETWORK, "Start fallback AP");
      startAccessPoint();
    }
  }
}

// Captive DNS and mDNS
static void stopCaptiveDns(bool complete = false) {
  if (!dnsStarted) {
    return;
  }

  dnsServer.stop();
  dnsStarted = false;
  lastDnsAttempt = 0;

  if (complete) {
    setNetworkState(NetworkMode::AP, NetworkState::PORTAL_COMPLETE);
  }

  setNetworkState(
    NetworkMode::AP,
    apStarted.load() ? NetworkState::AP_STARTED : NetworkState::NETWORK_DISABLED
  );
  notifyNetworkEvent(NetworkEvent::PORTAL_STOPPED);
  ESP_LOGD(L_PORTAL_DNSSERVER, "Stopped");
}

static void stopMdns() {
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
    lastMdnsAttempt = 0;
    notifyNetworkEvent(NetworkEvent::MDNS_STOPPED);
    ESP_LOGD(L_PORTAL_WEBSERVER, "mDNS disabled");
  }
}

static void updateNetworkServices() {
  const bool ready = webServerReady;

  // Serve captive DNS only while HTTP is ready and no upstream connection exists.
  const bool captive = ready
    && apStarted.load()
    && apIp
    && !isNetworkConnected();

  if (!captive) {
    stopCaptiveDns(isNetworkConnected());

  } else if (!dnsStarted && (!lastDnsAttempt || millis() - lastDnsAttempt >= 5000)) {
    lastDnsAttempt = millis();
    setNetworkState(NetworkMode::AP, NetworkState::PORTAL_STARTING);

    if (dnsServer.start(53, "*", IPAddress(apIp))) {
      dnsStarted = true;
      setNetworkState(NetworkMode::AP, NetworkState::PORTAL_STARTED);
      notifyNetworkEvent(NetworkEvent::PORTAL_STARTED);
      ESP_LOGD(L_PORTAL_DNSSERVER, "Started: AP up");

    } else {
      dnsServer.stop();
      setNetworkState(NetworkMode::AP, NetworkState::AP_STARTED);
      ESP_LOGE(L_PORTAL_DNSSERVER, "Failed start DNS");
    }
  }

  // Advertise HTTP on either interface, including while the fallback AP is active.
  const bool enableMdns = ready
    && settings.portal.mdns
    && (apStarted.load() || isNetworkConnected());

  if (!enableMdns) {
    stopMdns();

  } else {
    if (mdnsStarted && strcmp(mdnsHostname, networkSettings.hostname) != 0) {
      stopMdns();
      lastMdnsAttempt = 0;
    }

    if (!mdnsStarted && (!lastMdnsAttempt || millis() - lastMdnsAttempt >= 5000)) {
      lastMdnsAttempt = millis();
      bool started = MDNS.begin(networkSettings.hostname);

      if (started) {
        started = MDNS.addService("http", "tcp", 80);
      }

      if (started) {
        strcpy(mdnsHostname, networkSettings.hostname);
        mdnsStarted = true;
        notifyNetworkEvent(NetworkEvent::MDNS_STARTED);
        ESP_LOGD(L_PORTAL_WEBSERVER, "mDNS enabled and service added");

      } else {
        MDNS.end();
        ESP_LOGE(L_PORTAL_WEBSERVER, "Failed start mDNS");
      }
    }
  }
}

// Event processing and recovery
static void processNetworkEvent(const PendingNetworkEvent& pending) {
  switch (pending.event) {
    // Station
    case NetworkEvent::STA_STARTED:
      // Repeated START must not invalidate an active connection.
      if (staStarted.exchange(true)) {
        break;
      }

      if (connectionPending) {
        setNetworkState(
          NetworkMode::STA,
          connectedTime ? NetworkState::NETWORK_RECONNECTING : NetworkState::NETWORK_CONNECTING
        );

      } else {
        setNetworkState(NetworkMode::STA, NetworkState::NETWORK_ENABLED);
      }
      break;
    case NetworkEvent::STA_STOPPED: {
      const bool started = staStarted.exchange(false);
      staAssociated = false;
      disconnectReason = 0;
      setNetworkState(NetworkMode::STA, NetworkState::NETWORK_DISABLED);
      if (!started) {
        return;
      }
      break;
    }

    case NetworkEvent::STA_CONNECTED:
      // Association alone does not confirm a usable IP address.
      staAssociated = true;
      break;
    case NetworkEvent::STA_GOT_IP:
      if (!pending.value) {
        return;
      }

      // GOT_IP confirms both DHCP and configured static IPv4 after association.
      // IP changes invalidate the previous client session, including A -> B -> A.
      if (isNetworkConnected() && connectedIp != pending.value) {
        disconnectReason = 0;
        setNetworkState(NetworkMode::STA, NetworkState::NETWORK_DISCONNECTED);
      }

      staAssociated = true;
      connectedIp = pending.value;
      if (!isNetworkConnected()) {
        vars.network.rssi = WiFi.STA.RSSI();
        lastRssiUpdate = millis();
        setNetworkState(NetworkMode::STA, NetworkState::NETWORK_CONNECTED);
      }
      break;
    case NetworkEvent::STA_DISCONNECTED:
      staAssociated = false;
      disconnectReason = pending.reason;
      setNetworkState(NetworkMode::STA, NetworkState::NETWORK_DISCONNECTED);
      break;
    case NetworkEvent::STA_LOST_IP:
      // Keep association so the DHCP recovery timeout remains active.
      disconnectReason = 0;
      setNetworkState(NetworkMode::STA, NetworkState::NETWORK_DISCONNECTED);
      break;

    // Access point
    case NetworkEvent::AP_STARTED:
      // Preserve clients and captive portal state on a repeated START.
      if (apStarted.exchange(true)) {
        break;
      }

      apIp = pending.value;
      apClients = 0;
      setNetworkState(NetworkMode::AP, NetworkState::AP_STARTED);
      break;
    case NetworkEvent::AP_STOPPED: {
      stopCaptiveDns();
      const bool started = apStarted.exchange(false);
      apIp = 0;
      apClients = 0;
      setNetworkState(NetworkMode::AP, NetworkState::NETWORK_DISABLED);
      if (!isNetworkConnected()) {
        stopMdns();
      }
      if (!started) {
        return;
      }
      break;
    }

    case NetworkEvent::AP_CLIENT_CONNECTED:
      if (apClients < UINT8_MAX) {
        ++apClients;
      }
      break;

    case NetworkEvent::AP_CLIENT_DISCONNECTED:
      if (apClients) {
        --apClients;
      }
      break;
    default:
      break;
  }

  notifyNetworkEvent(pending.event);
}

static void processNetworkEvents() {
  const bool overflowed = networkEventsOverflowed.exchange(false);
  const bool recoveryRequested = networkRecoveryRequested.exchange(false);

  if (overflowed || recoveryRequested) {
    // Lost events invalidate published state immediately, even during the reset cooldown.
    xQueueReset(networkEventQueue);
    stopCaptiveDns();
    stopMdns();
    staStarted.store(false);
    apStarted.store(false);
    staAssociated = false;
    setNetworkState(NetworkMode::STA, NetworkState::NETWORK_DISABLED);
    setNetworkState(NetworkMode::AP, NetworkState::NETWORK_DISABLED);

    if (overflowed) {
      ESP_LOGW(L_NETWORK, "Network event queue overflow, reset WiFi requested");
      notifyNetworkEvent(NetworkEvent::EVENTS_OVERFLOW);
    }

    recoveryPending = true;
  }

  if (recoveryPending) {
    // Limit driver resets, but keep the published state offline while waiting.
    if (lastNetworkRecoveryAttempt && millis() - lastNetworkRecoveryAttempt < 15000) {
      return;
    }

    lastNetworkRecoveryAttempt = millis();
    // Keep callbacks nonblocking; recover through the existing full-reset path.
    ESP_LOGW(L_NETWORK, "Reset WiFi after network event failure...");
    if (!resetWifi()) {
      return;
    }

    recoveryPending = false;
  }

  // Bound each pass so event bursts leave time for the rest of the worker.
  PendingNetworkEvent pending;
  for (size_t count = 0; count < NETWORK_EVENT_QUEUE_LENGTH; ++count) {
    if (xQueueReceive(networkEventQueue, &pending, 0) != pdTRUE) {
      break;
    }

    processNetworkEvent(pending);
  }
}

// Connection services
static void updateRssi() {
  if (isNetworkConnected() && millis() - lastRssiUpdate >= 1000) {
    vars.network.rssi = WiFi.STA.RSSI();
    lastRssiUpdate = millis();
  }
}

static void updateNtp() {
  if (!ntpStarted && settings.system.ntp.server[0]) {
    configTime(0, 0, settings.system.ntp.server);
    setenv("TZ", settings.system.ntp.timezone, 1);
    tzset();
    ntpStarted = true;
  }
}

static void logNetworkState(NetworkMode mode, NetworkState previous, NetworkState state) {
  if (mode != NetworkMode::STA) {
    return;
  }

  if (state == NetworkState::NETWORK_CONNECTED) {
    ESP_LOGI(
      L_NETWORK,
      "Connected, downtime: %lu s., IP: %s, RSSI: %hhd",
      (millis() - disconnectedTime) / 1000,
      IPAddress(connectedIp).toString().c_str(),
      vars.network.rssi
    );

  } else if (previous == NetworkState::NETWORK_CONNECTED) {
    ESP_LOGI(
      L_NETWORK,
      "Disconnected, reason: %hhu, uptime: %lu s.",
      disconnectReason,
      (millis() - connectedTime) / 1000
    );
  }
}

// Tasks
static Task networkStartTask("Start network services", Task::Type::ONCE, [](void*) {
  if (webServerReady || (!isNetworkInterfaceStarted(NetworkMode::AP) && !isNetworkConnected())) {
    return;
  }

  webServer->begin();
  webServerReady = true;
  configureLoggingTask.resume();
  ESP_LOGD(L_PORTAL_WEBSERVER, "Started: AP up or STA connected");
});

static Task reconnectTask("Reconnect network", Task::Type::ONCE, [](void*) {
  ESP_LOGI(L_NETWORK, "Reconnecting...");

  const bool wasStarted = WiFi.STA.started();
  if (!WiFi.STA.end()) {
    ESP_LOGW(L_NETWORK, "Failed stop STA, reset WiFi...");
    resetWifi();
    return;
  }

  if (wasStarted) {
    postInterfaceStopped(ARDUINO_EVENT_WIFI_STA_STOP);
  }

  lastConnectionAttempt = 0;
  connectionPending = false;
});

static Task networkTask("Network", [](void*) {
  // Delay explicit reconnects so the HTTP handler can finish its response.
  if (reconnectRequested.exchange(false)) {
    reconnectTask.resume(5000);
  }

  // Apply queued transitions before using the interfaces and services.
  processNetworkEvents();
  if (!isNetworkConnected()) {
    Sensors::setConnectionStatusByType(Sensors::Type::MANUAL, false, false);
  }

  if (recoveryPending
    || networkRecoveryRequested.load()
    || networkEventsOverflowed.load()
  ) {
    return;
  }

  updateRssi();
  updateNetworkServices();

  // Continue processing events and services during the reconnect delay.
  if (!reconnectTask.paused()) {
    return;
  }

  if (isNetworkConnected()) {
    if (connectionPending) {
      connectionPending = false;
      lastConnectionAttempt = millis();
    }

    updateNtp();
    updateAccessPoint();

  } else {
    updateAccessPoint();
    updateConnection();
  }
});

// Initialization
void initNetwork() {
  // The queue must exist before registering framework callbacks or starting WiFi.
  networkEventQueue = xQueueCreateStatic(
    NETWORK_EVENT_QUEUE_LENGTH,
    sizeof(PendingNetworkEvent),
    networkEventBuffer,
    &networkEventQueueStorage
  );

  if (!networkEventQueue) {
    ESP_LOGE(L_NETWORK, "Failed create network event queue");
    std::abort();
  }

  dnsServer.setTTL(0);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

  onNetworkEvent([](NetworkEvent event) {
    if (!webServerReady && (event == NetworkEvent::AP_STARTED || event == NetworkEvent::CONNECTED)) {
      networkStartTask.resume();
    }
  });

  Network.begin();
  Network.onEvent(handleWifiEvent);
  onNetworkStateChange(logNetworkState);
  WiFi.persistent(false);
  WiFi.STA.setAutoReconnect(false);
  Network.setHostname(networkSettings.hostname);

  networkTask.setInterval(100);
  systemTaskManager.addTask(reconnectTask);
  systemTaskManager.addTask(networkTask);
  systemTaskManager.addTask(networkStartTask);
}
