#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>
#include <metrics.h>
//#define PORTAL_CACHE "max-age=86400"
#define PORTAL_CACHE nullptr

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <UpgradeHandler.h>
#include <WiFi.h>
#include <MycilaWebSerial.h>

#include <esp_core_dump.h>
#include <esp_partition.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

WebSerial* webSerial = nullptr;

AsyncWebServer* webServer = nullptr;

enum class RequestInterface : uint8_t {
  UNKNOWN,
  AP,
  STA,
  ETH
};

static RequestInterface getRequestInterface(AsyncWebServerRequest* request) {
  if (request == nullptr || request->client() == nullptr) {
    return RequestInterface::UNKNOWN;
  }

  // Classify the connection by its destination address.
  const IPAddress localIP = request->client()->localIP();
  if (localIP == IPAddress(0, 0, 0, 0)) {
    return RequestInterface::UNKNOWN;
  }

  const IPAddress apIP = WiFi.AP.localIP();
  const IPAddress staIP = WiFi.STA.localIP();
  if (localIP == apIP && localIP == staIP) {
    // Matching AP/STA addresses cannot identify the interface.
    return RequestInterface::UNKNOWN;
  }

  if (localIP == apIP) {
    return RequestInterface::AP;
  }

  if (localIP == staIP) {
    return RequestInterface::STA;
  }

  return RequestInterface::UNKNOWN;
}

static void onCaptivePortal(AsyncWebServerRequest *request) {
  const auto& url = request->url();

  if (url.equals("/connecttest.txt")) {
    request->redirect("http://logout.net", 302);

    ESP_LOGD(L_PORTAL_CAPTIVE, "Redirect to http://logout.net with 302 code");

  } else if (url.equals("/wpad.dat")) {
    request->send(404);

    ESP_LOGD(L_PORTAL_CAPTIVE, "Send empty page with 404 code");

  } else if (url.equals("/success.txt")) {
    request->send(200);

    ESP_LOGD(L_PORTAL_CAPTIVE, "Send empty page with 200 code");

  } else {
    std::string portalUrl;
    portalUrl.reserve(7 + 15 + 1);
    portalUrl = "http://";
    portalUrl += WiFi.AP.localIP().toString().c_str();
    portalUrl += '/';

    request->redirect(portalUrl.c_str(), 302);

    ESP_LOGD(L_PORTAL_CAPTIVE, "Redirect to portal page with 302 code");
  }
}

static bool isAuthenticationRequired(AsyncWebServerRequest* request) {
  if (isNetworkInterfaceStarted(NetworkMode::AP) && getRequestInterface(request) == RequestInterface::AP) {
    return false;
  }

  return settings.portal.auth && strlen(settings.portal.password);
}

static void setDownloadFilename(AsyncWebServerResponse* response, const char* type, const char* extension) {
  const time_t now = time(nullptr);

  tm localNow {};
  localtime_r(&now, &localNow);

  char localNowValue[20];
  strftime(localNowValue, sizeof(localNowValue), "%Y-%m-%d-%H-%M-%S", &localNow);

  char contentDisposition[96];
  snprintf(
    contentDisposition, sizeof(contentDisposition),
    "attachment; filename=\"%s_%s_%s.%s\"",
    networkSettings.hostname, localNowValue, type, extension
  );
  response->addHeader("Content-Disposition", contentDisposition);
}

// auth middleware
static AsyncMiddlewareFunction authMiddleware([](AsyncWebServerRequest *request, ArMiddlewareNext next) {
  if (!isAuthenticationRequired(request)) {
    return next();
  }

  if (request->authenticate(settings.portal.login, settings.portal.password, PROJECT_NAME)) {
    return next();
  }

  return request->requestAuthentication(AsyncAuthType::AUTH_BASIC, PROJECT_NAME, "Authentication failed");
});

static void initWebSerial() {
  // Register before System starts. Unbuffered WebSerial preserves newlines inside each record.
  webSerial = new WebSerial();
  webSerial->begin(webServer, nullptr, "/webserial");

  webServer->addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
    if (request->url().equals("/webserial")) {
      if (!settings.system.webSerial.enabled) {
        request->send(404);
        return;
      }

      // Refresh credentials before each handshake, including after re-enabling.
      if (isAuthenticationRequired(request)) {
        webSerial->setAuthentication(settings.portal.login, settings.portal.password);
      } else {
        webSerial->setAuthentication("", "");
      }
    }
    next();
  });
}

static void routes() {
  // index page
  webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/index.html");
  });
  webServer->serveStatic("/index.html", LittleFS, "/pages/index.html", PORTAL_CACHE);

  // dashboard page
  webServer->serveStatic("/dashboard.html", LittleFS, "/pages/dashboard.html", PORTAL_CACHE)
    .addMiddleware(&authMiddleware);

  // network settings page
  webServer->serveStatic("/network.html", LittleFS, "/pages/network.html", PORTAL_CACHE)
    .addMiddleware(&authMiddleware);

  // settings page
  webServer->serveStatic("/settings.html", LittleFS, "/pages/settings.html", PORTAL_CACHE)
    .addMiddleware(&authMiddleware);

  // sensors page
  webServer->serveStatic("/sensors.html", LittleFS, "/pages/sensors.html", PORTAL_CACHE)
    .addMiddleware(&authMiddleware);

  // upgrade page
  webServer->serveStatic("/upgrade.html", LittleFS, "/pages/upgrade.html", PORTAL_CACHE)
    .addMiddleware(&authMiddleware);

  // logs page
  webServer->serveStatic("/logs.html", LittleFS, "/pages/logs.html", PORTAL_CACHE)
    .addMiddleware(&authMiddleware);

  // not found
  webServer->onNotFound([](AsyncWebServerRequest *request) {
    const auto& url = request->url();
    ESP_LOGD(L_PORTAL_WEBSERVER, "Page not found, uri: %s", url.c_str());

    if (url.equals("/index.html")) {
      request->send(200, "text/plain", "The file system is not flashed!");

    } else if (getNetworkState(NetworkMode::AP) == NetworkState::PORTAL_STARTED && getRequestInterface(request) == RequestInterface::AP) {
      onCaptivePortal(request);

    } else {
      request->send(404, "text/plain", "Page not found");
    }
  });

  webServer->serveStatic("/robots.txt", LittleFS, "/static/robots.txt", PORTAL_CACHE);
  webServer->serveStatic("/favicon.ico", LittleFS, "/static/images/favicon.ico", PORTAL_CACHE);
  webServer->serveStatic("/static", LittleFS, "/static", PORTAL_CACHE);
}

static void restApi() {
  // OTA
  auto upgradeHandler = (new UpgradeHandler("/api/upgrade"))
    ->setBeforeUpgradeCallback([](AsyncWebServerRequest *request, UpgradeHandler::UpgradeType type) -> bool {
    // Upload callbacks run before middleware; reject before pausing tasks or writing.
    if (isAuthenticationRequired(request) && !request->authenticate(settings.portal.login, settings.portal.password, PROJECT_NAME)) {
      return false;
    }

    if (restartTask.scheduled() || restartTask.active()) {
      return false;
    }
    // UpgradeHandler invokes this callback for every upload chunk.
    if (vars.states.upgrading) {
      return true;
    }

    vars.states.upgrading = true;
    coreTaskManager.pause();
    // Core owns MQTT; finish its current callback before stopping the client.
    if (!coreTaskManager.waitForPaused(5000)) {
      vars.states.upgrading = false;
      if (!restartTask.scheduled() && !restartTask.active()) {
        coreTaskManager.resume();
      }
      ESP_LOGW(L_PORTAL_OTA, "Core task manager pause not confirmed, upgrade cancelled");
      return false;
    }

    stopMqtt(false);

    // An MQTT command already in flight may have scheduled a restart before end().
    if (restartTask.scheduled() || restartTask.active()) {
      vars.states.upgrading = false;
      return false;
    }

    sensorsTaskManager.pause();
    if (!sensorsTaskManager.waitForPaused(5000)) {
      requestReconfigureMqtt();
      vars.states.upgrading = false;
      if (!restartTask.scheduled() && !restartTask.active()) {
        coreTaskManager.resume();
        sensorsTaskManager.resume();
      }
      ESP_LOGW(L_PORTAL_OTA, "Sensors task manager pause not confirmed, upgrade cancelled");
      return false;
    }

    openthermTaskManager.pause();
    if (!openthermTaskManager.waitForPaused(5000)) {
      requestReconfigureMqtt();
      vars.states.upgrading = false;
      if (!restartTask.scheduled() && !restartTask.active()) {
        coreTaskManager.resume();
        sensorsTaskManager.resume();
        openthermTaskManager.resume();
      }
      ESP_LOGW(L_PORTAL_OTA, "OpenTherm task manager pause not confirmed, upgrade cancelled");
      return false;
    }

    return true;
  })->setAfterUpgradeCallback([](AsyncWebServerRequest *request, const UpgradeHandler::UpgradeResult& fwResult, const UpgradeHandler::UpgradeResult& fsResult) {
    if (request && isAuthenticationRequired(request) && !request->authenticate(settings.portal.login, settings.portal.password, PROJECT_NAME)) {
      return request->requestAuthentication(AsyncAuthType::AUTH_BASIC, PROJECT_NAME, "Authentication failed");
    }

    unsigned short status = 406;
    if (fwResult.status == UpgradeHandler::UpgradeStatus::SUCCESS || fsResult.status == UpgradeHandler::UpgradeStatus::SUCCESS) {
      status = 202;
      if (!restartTask.scheduled() && !restartTask.active()) {
        restartTask.resume(15000);
      }

    } else {
      requestReconfigureMqtt();
    }

    vars.states.upgrading = false;
    if (status != 202 && !restartTask.scheduled() && !restartTask.active()) {
      coreTaskManager.resume();
      sensorsTaskManager.resume();
      openthermTaskManager.resume();
    }

    if (!request) {
      return;
    }

    // make response
    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();

    auto fwDoc = doc["firmware"].to<JsonObject>();
    fwDoc["status"] = static_cast<uint8_t>(fwResult.status);
    fwDoc["error"] = fwResult.error;

    auto fsDoc = doc["filesystem"].to<JsonObject>();
    fsDoc["status"] = static_cast<uint8_t>(fsResult.status);
    fsDoc["error"] = fsResult.error;

    // send response
    response->setLength();
    response->setCode(status);
    request->send(response);
  });
  upgradeHandler->addMiddleware(&authMiddleware);
  webServer->addHandler(upgradeHandler);


  // backup
  webServer->on("/api/backup/save", HTTP_GET, [](AsyncWebServerRequest *request) {
    // make response
    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();

    // add network settings
    auto networkDoc = doc[S_NETWORK].to<JsonObject>();
    toJson(networkSettings, networkDoc);

    // add main settings
    auto settingsDoc = doc[S_SETTINGS].to<JsonObject>();
    toJson(settings.system, settingsDoc[S_SYSTEM].to<JsonObject>());
    toJson(settings.portal, settingsDoc[S_PORTAL].to<JsonObject>());
    toJson(settings.opentherm, settingsDoc[S_OPENTHERM].to<JsonObject>());
    toJson(settings.mqtt, settingsDoc[S_MQTT].to<JsonObject>());
    toJson(settings.emergency, settingsDoc[S_EMERGENCY].to<JsonObject>());
    toJson(settings.heating, settingsDoc[S_HEATING].to<JsonObject>());
    toJson(settings.dhw, settingsDoc[S_DHW].to<JsonObject>());
    toJson(settings.equitherm, settingsDoc[S_EQUITHERM].to<JsonObject>());
    toJson(settings.pid, settingsDoc[S_PID].to<JsonObject>());
    toJson(settings.externalPump, settingsDoc[S_EXTERNAL_PUMP].to<JsonObject>());
    toJson(settings.cascadeControl, settingsDoc[S_CASCADE_CONTROL].to<JsonObject>());

    // add sensors settings
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      if (const auto sensor = Sensors::getSettings(sensorId)) {
        auto sensorsettingsDoc = doc[S_SENSORS][sensorId].to<JsonObject>();
        toJson(sensorId, *sensor, sensorsettingsDoc);
      }
    }

    // send response
    setDownloadFilename(response, "backup", "json");
    response->setLength();
    request->send(response);
  }).addMiddleware(&authMiddleware);

  // restart
  webServer->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
      return request->send(503);
    }

    restartTask.resume(15000);
    request->send(202);
  }).addMiddleware(&authMiddleware);

  // network
  webServer->on("/api/network/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    // make response
    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();
    toJson(networkSettings, doc);

    // send response
    response->setLength();
    request->send(response);
  }).addMiddleware(&authMiddleware);

  auto& nsHandler = webServer->on("/api/network/settings", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
    if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
      return request->send(503);
    }

    if (requestDoc.isNull() || !requestDoc.size()) {
      return request->send(400);
    }

    // prepare request
    const auto result = fromJson(requestDoc, networkSettings);
    if (result.changed) {
      requestSaveNetworkSettings();
    }
    requestDoc.clear();

    if (request->hasParam("noresponse")) {
      request->send(result.changed ? 201 : 200);

    } else {
      // make response
      auto *response = new AsyncJsonResponse();
      auto& responseDoc = response->getRoot();
      toJson(networkSettings, responseDoc);

      // send response
      response->setLength();
      response->setCode(result.changed ? 201 : 200);
      request->send(response);
    }

    if (result.changed && !request->hasParam("noreconnect")) {
      // Queue the response before a network reset can disconnect this request.
      reconnectNetwork();
    }
  });
  nsHandler.setMaxContentLength(512);
  nsHandler.addMiddleware(&authMiddleware);

  webServer->on("/api/network/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    auto apCount = WiFi.scanComplete();

    switch (apCount) {
      case WIFI_SCAN_FAILED:
        WiFi.scanNetworks(true, true, true);
        request->send(404);
        break;

      case WIFI_SCAN_RUNNING:
        request->send(202);
        break;

      default:
        // make respponse
        auto *response = new AsyncJsonResponse(true);
        auto& doc = response->getRoot();
        for (short int i = 0; i < apCount; i++) {
          doc[i][S_SSID] = WiFi.SSID(i);
          doc[i][S_BSSID] = WiFi.BSSIDstr(i);
          doc[i][S_SIGNAL_QUALITY] = rssiToSignalQuality(WiFi.RSSI(i));
          doc[i][S_CHANNEL] = WiFi.channel(i);
          doc[i][S_HIDDEN] = !WiFi.SSID(i).length();
          doc[i][S_AUTH] = WiFi.encryptionType(i);
        }

        // send response
        response->setLength();
        request->send(response);

        // clear
        WiFi.scanDelete();
    }
  }).addMiddleware(&authMiddleware);


  // settings
  webServer->on(AsyncURIMatcher::prefix("/api/settings/"), HTTP_GET, [](AsyncWebServerRequest *request) {
    const auto& url = request->url();
    const auto section = std::string_view(url.c_str(), url.length())
      .substr(sizeof("/api/settings/") - 1);
    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();

    if (section == S_SYSTEM) {
      toJson(settings.system, doc);

    } else if (section == S_PORTAL) {
      toJson(settings.portal, doc);

    } else if (section == S_OPENTHERM) {
      toJson(settings.opentherm, doc);

    } else if (section == S_MQTT) {
      toJson(settings.mqtt, doc);

    } else if (section == S_EMERGENCY) {
      toJson(settings.emergency, doc);

    } else if (section == S_HEATING) {
      toJson(settings.heating, doc);

    } else if (section == S_DHW) {
      toJson(settings.dhw, doc);

    } else if (section == S_PID) {
      toJson(settings.pid, doc);

    } else if (section == S_EQUITHERM) {
      toJson(settings.equitherm, doc);

    } else if (section == S_EXTERNAL_PUMP) {
      toJson(settings.externalPump, doc);

    } else if (section == S_CASCADE_CONTROL) {
      toJson(settings.cascadeControl, doc);

    } else {
      delete response;
      return request->send(404);
    }

    response->setLength();
    request->send(response);
  }).addMiddleware(&authMiddleware);

  auto& settingsHandler = webServer->on(AsyncURIMatcher::prefix("/api/settings/"), HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
    if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
      request->send(503);
      return;
    }

    if (!requestDoc.is<JsonObjectConst>() || !requestDoc.size()) {
      request->send(400);
      return;
    }

    const auto& url = request->url();
    const auto section = std::string_view(url.c_str(), url.length())
      .substr(sizeof("/api/settings/") - 1);
    SettingResult result;
    bool mqttChanged = false;

    if (section == S_SYSTEM) {
      result = fromJson(requestDoc, settings.system);

    } else if (section == S_PORTAL) {
      result = fromJson(requestDoc, settings.portal);

    } else if (section == S_OPENTHERM) {
      result = fromJson(requestDoc, settings.opentherm);

    } else if (section == S_MQTT) {
      const auto previousMqtt = settings.mqtt;
      result = fromJson(requestDoc, settings.mqtt);
      mqttChanged = previousMqtt.enabled != settings.mqtt.enabled
        || previousMqtt.port != settings.mqtt.port
        || previousMqtt.interval != settings.mqtt.interval
        || strcmp(previousMqtt.server, settings.mqtt.server) != 0
        || strcmp(previousMqtt.user, settings.mqtt.user) != 0
        || strcmp(previousMqtt.password, settings.mqtt.password) != 0
        || strcmp(previousMqtt.prefix, settings.mqtt.prefix) != 0;

    } else if (section == S_EMERGENCY) {
      result = fromJson(requestDoc, settings.emergency);

    } else if (section == S_HEATING) {
      result = fromJson(requestDoc, settings.heating);

    } else if (section == S_DHW) {
      result = fromJson(requestDoc, settings.dhw);

    } else if (section == S_PID) {
      result = fromJson(requestDoc, settings.pid);

    } else if (section == S_EQUITHERM) {
      result = fromJson(requestDoc, settings.equitherm);

    } else if (section == S_EXTERNAL_PUMP) {
      result = fromJson(requestDoc, settings.externalPump);

    } else if (section == S_CASCADE_CONTROL) {
      result = fromJson(requestDoc, settings.cascadeControl);

    } else {
      return request->send(404);
    }

    result.changed = normalizeSettings(settings) || result.changed;
    if (result.changed) {
      requestSaveSettings();
      configureLoggingTask.resume();
      if (mqttChanged) {
        requestReconfigureMqtt();
      }

      // If busy, the periodic task will publish the current settings.
      std::unique_lock<std::mutex> lock(mqttMutex, std::try_to_lock);
      if (lock.owns_lock() && mqttPublishSettingsTask) {
        mqttPublishSettingsTask->requestEarlyRun();
      }
    }
    requestDoc.clear();

    if (request->hasParam("noresponse")) {
      return request->send(result.changed ? 201 : 200);
    }

    auto *response = new AsyncJsonResponse();
    auto& responseDoc = response->getRoot();

    if (section == S_SYSTEM) {
      toJson(settings.system, responseDoc);

    } else if (section == S_PORTAL) {
      toJson(settings.portal, responseDoc);

    } else if (section == S_OPENTHERM) {
      toJson(settings.opentherm, responseDoc);

    } else if (section == S_MQTT) {
      toJson(settings.mqtt, responseDoc);

    } else if (section == S_EMERGENCY) {
      toJson(settings.emergency, responseDoc);

    } else if (section == S_HEATING) {
      toJson(settings.heating, responseDoc);

    } else if (section == S_DHW) {
      toJson(settings.dhw, responseDoc);

    } else if (section == S_PID) {
      toJson(settings.pid, responseDoc);

    } else if (section == S_EQUITHERM) {
      toJson(settings.equitherm, responseDoc);

    } else if (section == S_EXTERNAL_PUMP) {
      toJson(settings.externalPump, responseDoc);

    } else if (section == S_CASCADE_CONTROL) {
      toJson(settings.cascadeControl, responseDoc);
    }

    response->setLength();
    response->setCode(result.changed ? 201 : 200);
    request->send(response);
  });
  settingsHandler.setMaxContentLength(3072);
  settingsHandler.addMiddleware(&authMiddleware);


  // sensors list
  webServer->on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
    // make response
    bool detailed = request->hasParam("detailed");
    auto *response = new AsyncJsonResponse(true);
    auto& responseDoc = response->getRoot();

    // add sensors
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      if (detailed) {
        if (const auto snapshot = Sensors::getSnapshot(sensorId)) {
          responseDoc[sensorId][S_ENABLED] = snapshot->settings.enabled;
          responseDoc[sensorId][S_NAME] = static_cast<const char*>(snapshot->settings.name);
          responseDoc[sensorId][S_SLUG] = static_cast<const char*>(snapshot->settings.slug);
          responseDoc[sensorId][S_PURPOSE] = static_cast<uint8_t>(snapshot->settings.purpose);
          toJson(snapshot->settings, snapshot->result, responseDoc[sensorId]);
        }

      } else if (const auto sensor = Sensors::getSettings(sensorId)) {
        responseDoc[sensorId] = static_cast<const char*>(sensor->name);
      }
    }

    // send response
    response->setLength();
    request->send(response);
  }).addMiddleware(&authMiddleware);

  // sensor settings
  webServer->on(AsyncURIMatcher::exact("/api/sensor"), HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("id")) {
      return request->send(400);
    }

    const auto& id = request->getParam("id")->value();
    const auto sensorId = Sensors::parseId(id.c_str());
    if (!sensorId) {
      return request->send(404);
    }

    const auto sensor = Sensors::getSettings(*sensorId);
    if (!sensor) {
      return request->send(404);
    }
    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();
    toJson(*sensorId, *sensor, doc);

    response->setLength();
    request->send(response);
  }).addMiddleware(&authMiddleware);

  auto& ssHandler = webServer->on(AsyncURIMatcher::exact("/api/sensor"), HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
    if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
      return request->send(503);
    }

    if (requestDoc.isNull() || !requestDoc.size()) {
      return request->send(400);
    }

    if (!request->hasParam("id")) {
      return request->send(400);
    }

    auto& id = request->getParam("id")->value();
    const auto sensorId = Sensors::parseId(id.c_str());
    if (!sensorId) {
      return request->send(404);
    }

    const auto previousSettings = Sensors::getSettings(*sensorId);
    if (!previousSettings) {
      return request->send(404);
    }

    bool isDifferent = false;
    SettingResult result;
    {
      std::lock_guard<std::mutex> lock(Sensors::mutex);
      auto& sensor = Sensors::settings[*sensorId];

      result = fromJson(requestDoc, sensor);
      if (result.changed) {
        isDifferent |= previousSettings->enabled != sensor.enabled;
        isDifferent |= strcmp(previousSettings->name, sensor.name) != 0;
        isDifferent |= strcmp(previousSettings->slug, sensor.slug) != 0;
        isDifferent |= previousSettings->purpose != sensor.purpose;
        isDifferent |= previousSettings->type != sensor.type;
      }
    }

    if (result.changed) {
      if (isDifferent) {
        requestReconfigureMqttSensor(*sensorId, *previousSettings);
      }

      requestSaveSensorsSettings();
    }
    requestDoc.clear();

    if (request->hasParam("noresponse")) {
      return request->send(result.changed ? 201 : 200);
    }

    // make response
    auto *response = new AsyncJsonResponse();
    auto& responseDoc = response->getRoot();

    if (const auto sensor = Sensors::getSettings(*sensorId)) {
      toJson(*sensorId, *sensor, responseDoc);
    }

    // send response
    response->setLength();
    response->setCode(result.changed ? 201 : 200);
    request->send(response);
  });
  ssHandler.setMaxContentLength(1024);
  ssHandler.addMiddleware(&authMiddleware);


  // sensor measurements
  webServer->on(AsyncURIMatcher::exact("/api/sensor/value"), HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("id")) {
      return request->send(400);
    }

    const auto& id = request->getParam("id")->value();
    const auto sensorId = Sensors::parseId(id.c_str());
    if (!sensorId) {
      return request->send(404);
    }

    const auto sensor = Sensors::getSnapshot(*sensorId);
    if (!sensor) {
      return request->send(404);
    }

    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();
    toJson(sensor->settings, sensor->result, doc);
    response->setLength();
    request->send(response);
  }).addMiddleware(&authMiddleware);

  auto& sensorValueHandler = webServer->on(AsyncURIMatcher::exact("/api/sensor/value"), HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
    if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
      return request->send(503);
    }

    if (!requestDoc.is<JsonObjectConst>() || !requestDoc.size()) {
      return request->send(400);
    }

    if (!request->hasParam("id")) {
      return request->send(400);
    }

    const auto& id = request->getParam("id")->value();
    const auto sensorId = Sensors::parseId(id.c_str());
    if (!sensorId) {
      return request->send(404);
    }

    if (!requestDoc[S_VALUE].is<float>()) {
      return request->send(400);
    }

    const auto value = requestDoc[S_VALUE].as<float>();
    requestDoc.clear();
    if (!Sensors::setManualValueById(*sensorId, value, settings.system.unitSystem)) {
      return request->send(400);
    }

    requestPublishMqttSensor(*sensorId);

    if (request->hasParam("noresponse")) {
      return request->send(201);
    }

    const auto sensor = Sensors::getSnapshot(*sensorId);
    if (!sensor) {
      return request->send(404);
    }

    auto *response = new AsyncJsonResponse();
    auto& responseDoc = response->getRoot();
    toJson(sensor->settings, sensor->result, responseDoc);
    response->setLength();
    response->setCode(201);
    request->send(response);
  });
  sensorValueHandler.setMaxContentLength(1024);
  sensorValueHandler.addMiddleware(&authMiddleware);


  // vars
  webServer->on("/api/vars", HTTP_GET, [](AsyncWebServerRequest *request) {
    // make response
    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();
    toJson(vars, doc);

    // send response
    response->setLength();
    request->send(response);
  });

  auto& vHandler = webServer->on("/api/vars", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
    if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
      return request->send(503);
    }

    if (requestDoc.isNull() || !requestDoc.size()) {
      return request->send(400);
    }

    // prepare request
    const auto result = fromJson(requestDoc, vars);
    requestDoc.clear();

    // make response
    auto *response = new AsyncJsonResponse();
    auto& responseDoc = response->getRoot();
    toJson(vars, responseDoc);

    // send response
    response->setLength();
    response->setCode(result.changed ? 201 : 200);
    request->send(response);

    if (result.changed) {
      // If busy, the periodic task will publish the current state.
      std::unique_lock<std::mutex> lock(mqttMutex, std::try_to_lock);
      if (lock.owns_lock() && mqttPublishStateTask) {
        mqttPublishStateTask->requestEarlyRun();
      }
    }
  });
  vHandler.setMaxContentLength(1536);
  vHandler.addMiddleware(&authMiddleware);

  webServer->on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    // make response
    auto *response = new AsyncJsonResponse();
    auto& doc = response->getRoot();

    auto docSystem = doc[S_SYSTEM].to<JsonObject>();
    docSystem[S_RESET_REASON] = getResetReason();
    docSystem[S_UPTIME] = millis() / 1000;

    bool isConnected = isNetworkConnected();
    auto docNetwork = doc[S_NETWORK].to<JsonObject>();
    docNetwork[S_HOSTNAME] = networkSettings.hostname;
    docNetwork[S_MAC] = WiFi.STA.macAddress();
    docNetwork[S_CONNECTED] = isConnected;
    docNetwork[S_SSID] = strlen(networkSettings.sta.ssid) ? networkSettings.sta.ssid : nullptr;
    docNetwork[S_SIGNAL_QUALITY] = isConnected ? rssiToSignalQuality(WiFi.STA.RSSI()) : 0;
    docNetwork[S_CHANNEL] = isConnected ? networkSettings.sta.channel : 0;
    docNetwork[S_IP] = isConnected ? WiFi.STA.localIP().toString() : "";
    docNetwork[S_SUBNET] = isConnected ? WiFi.STA.subnetMask().toString() : "";
    docNetwork[S_GATEWAY] = isConnected ? WiFi.STA.gatewayIP().toString() : "";
    docNetwork[S_DNS] = isConnected ? WiFi.STA.dnsIP().toString() : "";

    auto docBuild = doc[S_BUILD].to<JsonObject>();
    docBuild[S_VERSION] = BUILD_VERSION;
    docBuild[S_COMMIT] = BUILD_COMMIT;
    docBuild[S_DATE] = __DATE__ " " __TIME__;
    docBuild[S_ENV] = BUILD_ENV;
    docBuild[S_CORE] = ESP.getCoreVersion();
    docBuild[S_SDK] = ESP.getSdkVersion();

    const auto memory = getMemoryMetrics();
    auto docHeap = doc[S_HEAP].to<JsonObject>();
    docHeap[S_TOTAL] = memory.heap.total;
    docHeap[S_FREE] = memory.heap.total_free_bytes;
    docHeap[S_MIN_FREE] = memory.heap.minimum_free_bytes;
    docHeap[S_MAX_FREE_BLOCK] = memory.heap.largest_free_block;
    docHeap[S_MIN_MAX_FREE_BLOCK] = memory.heap.minLargestBlock;

    auto docPsram = doc[S_PSRAM].to<JsonObject>();
    docPsram[S_TOTAL] = memory.psram.total;
    docPsram[S_FREE] = memory.psram.total_free_bytes;
    docPsram[S_MIN_FREE] = memory.psram.minimum_free_bytes;
    docPsram[S_MAX_FREE_BLOCK] = memory.psram.largest_free_block;

    auto docChip = doc[S_CHIP].to<JsonObject>();
    docChip[S_MODEL] = ESP.getChipModel();
    docChip[S_REV] = ESP.getChipRevision();
    docChip[S_CORES] = ESP.getChipCores();
    docChip[S_FREQ] = ESP.getCpuFreqMHz();

    auto docFlash = doc[S_FLASH].to<JsonObject>();
    docFlash[S_SIZE] = ESP.getFlashChipSize();

    // send response
    response->setLength();
    request->send(response);
  });

  webServer->on("/api/coredump", HTTP_GET, [](AsyncWebServerRequest* request) {
    size_t address = 0;
    size_t size = 0;

    const esp_err_t image = esp_core_dump_image_get(&address, &size);
    if (image != ESP_OK) {
      request->send(image == ESP_ERR_NOT_FOUND ? 404 : 500);
      return;
    }

    if (esp_core_dump_image_check() != ESP_OK) {
      request->send(500);
      return;
    }

    const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
      nullptr
    );
    if (partition == nullptr || partition->address != address || size > partition->size) {
      request->send(500);
      return;
    }

    auto* response = request->beginResponse("application/octet-stream", size,
      [partition, size](uint8_t* buffer, size_t maximum, size_t index) -> size_t {
        if (index >= size) {
          return 0;
        }

        const size_t remaining = size - index;
        const size_t count = remaining < maximum ? remaining : maximum;
        if (esp_partition_read(partition, index, buffer, count) != ESP_OK) {
          return 0;
        }

        return count;
      });
    setDownloadFilename(response, "coredump", "bin");
    request->send(response);
  }).addMiddleware(&authMiddleware);
}

// Initialization
void initPortal() {
  webServer = new AsyncWebServer(80);
  initWebSerial();
  routes();
  restApi();
}
