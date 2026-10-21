//#define PORTAL_CACHE "max-age=86400"
#define PORTAL_CACHE nullptr
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <UpgradeHandler.h>
#include <DNSServer.h>

using namespace NetworkUtils;

extern NetworkMgr* network;
extern FileData fsNetworkSettings, fsSettings, fsSensorsSettings;
extern MqttTask* tMqtt;
extern WebSerial* webSerial;


class PortalTask : public LeanTask {
public:
  explicit PortalTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {
    this->webServer = new AsyncWebServer(80);
    this->dnsServer = new DNSServer();
  }

  ~PortalTask() {
    if (this->webServer != nullptr) {
      this->stopWebServer();
      delete this->webServer;
    }

    if (this->dnsServer != nullptr) {
      this->stopDnsServer();
      delete this->dnsServer;
    }
  }

protected:
  const unsigned int changeStateInterval = 5000;

  AsyncWebServer* webServer = nullptr;
  DNSServer* dnsServer = nullptr;

  bool webServerEnabled = false;
  bool dnsServerEnabled = false;
  unsigned long webServerChangeState = 0;
  bool mDnsState = false;

  const char* getTaskName() override {
    return "Portal";
  }

  /*BaseType_t getTaskCore() override {
    return 1;
  }*/

  int getTaskPriority() override {
    return 1;
  }

  void setup() {
    this->dnsServer->setTTL(0);
    this->dnsServer->setErrorReplyCode(DNSReplyCode::NoError);

    // auth middleware
    static AsyncMiddlewareFunction authMiddleware([](AsyncWebServerRequest *request, ArMiddlewareNext next) {
      if (!isAuthenticationRequired()) {
        return next();
      }

      if (request->authenticate(settings.portal.login, settings.portal.password, PROJECT_NAME)) {
        return next();
      }

      return request->requestAuthentication(AsyncAuthType::AUTH_BASIC, PROJECT_NAME, "Authentication failed");
    });

    // web serial
    if (webSerial != nullptr) {
      webSerial->begin(this->webServer, nullptr, "/webserial");

      // Refresh credentials before each handshake to use the current portal settings.
      this->webServer->addMiddleware([](AsyncWebServerRequest *request, ArMiddlewareNext next) {
        if (request->url().equals("/webserial")) {
          if (isAuthenticationRequired()) {
            webSerial->setAuthentication(settings.portal.login, settings.portal.password);
          } else {
            webSerial->setAuthentication("", "");
          }
        }

        return next();
      });
    }

    // index page
    this->webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/index.html");
    });
    this->webServer->serveStatic("/index.html", LittleFS, "/pages/index.html", PORTAL_CACHE);

    // dashboard page
    this->webServer->serveStatic("/dashboard.html", LittleFS, "/pages/dashboard.html", PORTAL_CACHE)
      .addMiddleware(&authMiddleware);

    // restart
    this->webServer->on("/restart.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      vars.actions.restart = true;
      request->redirect("/");
    }).addMiddleware(&authMiddleware);

    // network settings page
    this->webServer->serveStatic("/network.html", LittleFS, "/pages/network.html", PORTAL_CACHE)
      .addMiddleware(&authMiddleware);

    // settings page
    this->webServer->serveStatic("/settings.html", LittleFS, "/pages/settings.html", PORTAL_CACHE)
      .addMiddleware(&authMiddleware);

    // sensors page
    this->webServer->serveStatic("/sensors.html", LittleFS, "/pages/sensors.html", PORTAL_CACHE)
      .addMiddleware(&authMiddleware);

    // upgrade page
    this->webServer->serveStatic("/upgrade.html", LittleFS, "/pages/upgrade.html", PORTAL_CACHE)
      .addMiddleware(&authMiddleware);

    // logs page
    this->webServer->serveStatic("/logs.html", LittleFS, "/pages/logs.html", PORTAL_CACHE)
      .addMiddleware(&authMiddleware);

    // OTA
    auto upgradeHandler = (new UpgradeHandler("/api/upgrade"))
      ->setBeforeUpgradeCallback([](AsyncWebServerRequest *request, UpgradeHandler::UpgradeType type) -> bool {
      if (vars.states.restarting) {
        return false;
      }

      vars.states.upgrading = true;
      return true;

    })->setAfterUpgradeCallback([](AsyncWebServerRequest *request, const UpgradeHandler::UpgradeResult& fwResult, const UpgradeHandler::UpgradeResult& fsResult) {
      unsigned short status = 406;
      if (fwResult.status == UpgradeHandler::UpgradeStatus::SUCCESS || fsResult.status == UpgradeHandler::UpgradeStatus::SUCCESS) {
        status = 202;
        vars.actions.restart = true;
      }
      vars.states.upgrading = false;

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
    this->webServer->addHandler(upgradeHandler);


    // backup
    this->webServer->on("/api/backup/save", HTTP_GET, [](AsyncWebServerRequest *request) {
      // make response
      auto *response = new AsyncJsonResponse();
      auto& doc = response->getRoot();

      // add network settings
      auto networkDoc = doc[S_NETWORK].to<JsonObject>();
      networkSettingsToJson(networkSettings, networkDoc);

      // add main settings
      auto settingsDoc = doc[S_SETTINGS].to<JsonObject>();
      settingsToJson(settings, settingsDoc);

      // add sensors settings
      for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
        auto sensorsettingsDoc = doc[S_SENSORS][sensorId].to<JsonObject>();
        sensorSettingsToJson(sensorId, Sensors::settings[sensorId], sensorsettingsDoc);
      }

      char filename[64];
      getFilename(filename, sizeof(filename), "backup");

      char contentDispositionValue[128];
      snprintf(
        contentDispositionValue,
        sizeof(contentDispositionValue),
        "attachment; filename=\"%s\"",
        filename
      );

      // send response
      response->addHeader("Content-Disposition", contentDispositionValue);
      response->setLength();
      request->send(response);
    }).addMiddleware(&authMiddleware);

    // backup restore
    auto& brHandler = this->webServer->on("/api/backup/restore", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
      if (vars.states.restarting) {
        return request->send(503);
      }

      if (requestDoc.isNull() || !requestDoc.size()) {
        return request->send(400);
      }

      // prepare request
      bool changed = false;

      // restore network settings
      if (!requestDoc[S_NETWORK].isNull() && jsonToNetworkSettings(requestDoc[S_NETWORK], networkSettings)) {
        fsNetworkSettings.update();
        network->setHostname(networkSettings.hostname)
          ->setStaCredentials(networkSettings.sta.ssid, networkSettings.sta.password, networkSettings.sta.channel)
          ->setApCredentials(networkSettings.ap.ssid, networkSettings.ap.password, networkSettings.ap.channel)
          ->setUseDhcp(networkSettings.useDhcp)
          ->setStaticConfig(
            networkSettings.staticConfig.ip,
            networkSettings.staticConfig.gateway,
            networkSettings.staticConfig.subnet,
            networkSettings.staticConfig.dns
          );
        changed = true;
      }

      // restore main settings
      if (!requestDoc[S_SETTINGS].isNull() && jsonToSettings(requestDoc[S_SETTINGS], settings)) {
        fsSettings.update();
        changed = true;
      }

      // restore sensors settings
      if (!requestDoc[S_SENSORS].isNull()) {
        for (auto sensor : requestDoc[S_SENSORS].as<JsonObject>()) {
          uint8_t sensorId;
          if (!Sensors::parseId(sensor.key().c_str(), sensorId)) {
            continue;
          }

          if (jsonToSensorSettings(sensorId, sensor.value(), Sensors::settings[sensorId])) {
            fsSensorsSettings.update();
            changed = true;
          }
        }
      }
      requestDoc.clear();

      // send response
      request->send(changed ? 201 : 200);

      if (changed) {
        vars.actions.restart = true;
      }
    });
    brHandler.setMaxContentLength(3072);
    brHandler.addMiddleware(&authMiddleware);

    // network
    this->webServer->on("/api/network/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
      // make response
      auto *response = new AsyncJsonResponse();
      auto& doc = response->getRoot();
      networkSettingsToJson(networkSettings, doc);

      // send response
      response->setLength();
      request->send(response);
    }).addMiddleware(&authMiddleware);

    auto& nsHandler = this->webServer->on("/api/network/settings", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
      if (vars.states.restarting) {
        return request->send(503);
      }

      if (requestDoc.isNull() || !requestDoc.size()) {
        return request->send(400);
      }

      // prepare request
      bool changed = jsonToNetworkSettings(requestDoc, networkSettings);
      requestDoc.clear();

      // make response
      auto *response = new AsyncJsonResponse();
      auto& responseDoc = response->getRoot();
      networkSettingsToJson(networkSettings, responseDoc);

      // send response
      response->setLength();
      response->setCode(changed ? 201 : 200);
      request->send(response);

      if (changed) {
        fsNetworkSettings.update();
        network->setHostname(networkSettings.hostname)
          ->setStaCredentials(networkSettings.sta.ssid, networkSettings.sta.password, networkSettings.sta.channel)
          ->setApCredentials(networkSettings.ap.ssid, networkSettings.ap.password, networkSettings.ap.channel)
          ->setUseDhcp(networkSettings.useDhcp)
          ->setStaticConfig(
            networkSettings.staticConfig.ip,
            networkSettings.staticConfig.gateway,
            networkSettings.staticConfig.subnet,
            networkSettings.staticConfig.dns
          )
          ->reconnect();
      }
    });
    nsHandler.setMaxContentLength(512);
    nsHandler.addMiddleware(&authMiddleware);

    this->webServer->on("/api/network/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
      auto apCount = WiFi.scanComplete();

      switch (apCount) {
        case WIFI_SCAN_FAILED:
          WiFi.scanNetworks(true, true, false);
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
            doc[i][S_SIGNAL_QUALITY] = NetworkMgr::rssiToSignalQuality(WiFi.RSSI(i));
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
    this->webServer->on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
      // make response
      auto *response = new AsyncJsonResponse();
      auto& doc = response->getRoot();
      settingsToJson(settings, doc);

      // send response
      response->setLength();
      request->send(response);
    }).addMiddleware(&authMiddleware);

    auto& sHandler = this->webServer->on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
      if (vars.states.restarting) {
        return request->send(503);
      }

      if (requestDoc.isNull() || !requestDoc.size()) {
        return request->send(400);
      }

      // prepare request
      bool changed = jsonToSettings(requestDoc, settings);
      requestDoc.clear();

      // make response
      auto *response = new AsyncJsonResponse();
      auto& responseDoc = response->getRoot();
      settingsToJson(settings, responseDoc);

      // send response
      response->setLength();
      response->setCode(changed ? 201 : 200);
      request->send(response);

      if (changed) {
        fsSettings.update();
        tMqtt->resetPublishedSettingsTime();
      }
    });
    sHandler.setMaxContentLength(3072);
    sHandler.addMiddleware(&authMiddleware);


    // sensors list
    this->webServer->on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
      // make response
      bool detailed = request->hasParam("detailed");
      auto *response = new AsyncJsonResponse(true);
      auto& responseDoc = response->getRoot();

      // add sensors
      for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
        if (detailed) {
          auto& sSensor = Sensors::settings[sensorId];
          responseDoc[sensorId][S_ENABLED] = sSensor.enabled;
          responseDoc[sensorId][S_NAME] = sSensor.name;
          responseDoc[sensorId][S_PURPOSE] = static_cast<uint8_t>(sSensor.purpose);
          sensorResultToJson(sensorId, responseDoc[sensorId]);

        } else {
          responseDoc[sensorId] = Sensors::settings[sensorId].name;
        }
      }

      // send response
      response->setLength();
      request->send(response);
    }).addMiddleware(&authMiddleware);

    // sensor settings
    this->webServer->on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!request->hasParam("id")) {
        return request->send(400);
      }

      const auto& id = request->getParam("id")->value();
      uint8_t sensorId;
      if (!Sensors::parseId(id.c_str(), sensorId)) {
        return request->send(404);
      }

      auto *response = new AsyncJsonResponse();
      auto& doc = response->getRoot();
      sensorSettingsToJson(sensorId, Sensors::settings[sensorId], doc);

      response->setLength();
      request->send(response);
    }).addMiddleware(&authMiddleware);

    auto& ssHandler = this->webServer->on("/api/sensor", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
      if (vars.states.restarting) {
        return request->send(503);
      }

      if (requestDoc.isNull() || !requestDoc.size()) {
        return request->send(400);
      }

      if (!request->hasParam("id")) {
        return request->send(400);
      }

      auto& id = request->getParam("id")->value();
      uint8_t sensorId;
      if (!Sensors::parseId(id.c_str(), sensorId)) {
        return request->send(404);
      }

      // prepare request
      auto prevSettings = Sensors::settings[sensorId];
      bool changed = jsonToSensorSettings(sensorId, requestDoc, Sensors::settings[sensorId]);
      requestDoc.clear();

      // make response
      auto *response = new AsyncJsonResponse();
      auto& responseDoc = response->getRoot();

      auto& sSettings = Sensors::settings[sensorId];
      sensorSettingsToJson(sensorId, sSettings, responseDoc);

      // send response
      response->setLength();
      response->setCode(changed ? 201 : 200);
      request->send(response);

      if (changed) {
        tMqtt->reconfigureSensor(sensorId, prevSettings);
        fsSensorsSettings.update();
      }
    });
    ssHandler.setMaxContentLength(1024);
    ssHandler.addMiddleware(&authMiddleware);


    // vars
    this->webServer->on("/api/vars", HTTP_GET, [](AsyncWebServerRequest *request) {
      // make response
      auto *response = new AsyncJsonResponse();
      auto& doc = response->getRoot();
      varsToJson(vars, doc);

      // send response
      response->setLength();
      request->send(response);
    });

    auto& vHandler = this->webServer->on("/api/vars", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &requestDoc) {
      if (vars.states.restarting) {
        return request->send(503);
      }

      if (requestDoc.isNull() || !requestDoc.size()) {
        return request->send(400);
      }

      // prepare request
      bool changed = jsonToVars(requestDoc, vars);
      requestDoc.clear();

      // make response
      auto *response = new AsyncJsonResponse();
      auto& responseDoc = response->getRoot();
      varsToJson(vars, responseDoc);

      // send response
      response->setLength();
      response->setCode(changed ? 201 : 200);
      request->send(response);

      if (changed) {
        tMqtt->resetPublishedVarsTime();
      }
    });
    vHandler.setMaxContentLength(1536);
    vHandler.addMiddleware(&authMiddleware);

    this->webServer->on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
      // make response
      auto *response = new AsyncJsonResponse();
      auto& doc = response->getRoot();

      auto docSystem = doc[S_SYSTEM].to<JsonObject>();
      docSystem[S_RESET_REASON] = getResetReason();
      docSystem[S_UPTIME] = millis() / 1000;

      bool isConnected = network->isConnected();
      auto docNetwork = doc[S_NETWORK].to<JsonObject>();
      docNetwork[S_HOSTNAME] = networkSettings.hostname;
      docNetwork[S_MAC] = network->getStaMac();
      docNetwork[S_CONNECTED] = isConnected;
      docNetwork[S_SSID] = network->getStaSsid();
      docNetwork[S_SIGNAL_QUALITY] = isConnected ? NetworkMgr::rssiToSignalQuality(network->getRssi()) : 0;
      docNetwork[S_CHANNEL] = isConnected ? network->getStaChannel() : 0;
      docNetwork[S_IP] = isConnected ? network->getStaIp().toString() : "";
      docNetwork[S_SUBNET] = isConnected ? network->getStaSubnet().toString() : "";
      docNetwork[S_GATEWAY] = isConnected ? network->getStaGateway().toString() : "";
      docNetwork[S_DNS] = isConnected ? network->getStaDns().toString() : "";

      auto docBuild = doc[S_BUILD].to<JsonObject>();
      docBuild[S_VERSION] = BUILD_VERSION;
      docBuild[S_COMMIT] = BUILD_COMMIT;
      docBuild[S_DATE] = __DATE__ " " __TIME__;
      docBuild[S_ENV] = BUILD_ENV;
      docBuild[S_CORE] = ESP.getCoreVersion();
      docBuild[S_SDK] = ESP.getSdkVersion();

      auto docHeap = doc[S_HEAP].to<JsonObject>();
      docHeap[S_TOTAL] = getTotalHeap();
      docHeap[S_FREE] = getFreeHeap();
      docHeap[S_MIN_FREE] = getFreeHeap(true);
      docHeap[S_MAX_FREE_BLOCK] = getMaxFreeBlockHeap();
      docHeap[S_MIN_MAX_FREE_BLOCK] = getMaxFreeBlockHeap(true);

      auto docPsram = doc[S_PSRAM].to<JsonObject>();
      docPsram[S_TOTAL] = ESP.getPsramSize();
      docPsram[S_FREE] = ESP.getFreePsram();
      docPsram[S_MIN_FREE] = ESP.getMinFreePsram();
      docPsram[S_MAX_FREE_BLOCK] = ESP.getMaxAllocPsram();

      auto docChip = doc[S_CHIP].to<JsonObject>();
      docChip[S_MODEL] = ESP.getChipModel();
      docChip[S_REV] = ESP.getChipRevision();
      docChip[S_CORES] = ESP.getChipCores();
      docChip[S_FREQ] = ESP.getCpuFreqMHz();

      auto docFlash = doc[S_FLASH].to<JsonObject>();
      docFlash[S_SIZE] = ESP.getFlashChipSize();
      docFlash[S_REAL_SIZE] = docFlash[S_SIZE];

      // send response
      response->setLength();
      request->send(response);
    });

    this->webServer->on("/api/debug", HTTP_GET, [](AsyncWebServerRequest *request) {
      // make response
      auto *response = new AsyncJsonResponse();
      auto& doc = response->getRoot();

      auto docBuild = doc[S_BUILD].to<JsonObject>();
      docBuild[S_VERSION] = BUILD_VERSION;
      docBuild[S_COMMIT] = BUILD_COMMIT;
      docBuild[S_DATE] = __DATE__ " " __TIME__;
      docBuild[S_ENV] = BUILD_ENV;
      docBuild[S_CORE] = ESP.getCoreVersion();
      docBuild[S_SDK] = ESP.getSdkVersion();

      auto docHeap = doc[S_HEAP].to<JsonObject>();
      docHeap[S_TOTAL] = getTotalHeap();
      docHeap[S_FREE] = getFreeHeap();
      docHeap[S_MIN_FREE] = getFreeHeap(true);
      docHeap[S_MAX_FREE_BLOCK] = getMaxFreeBlockHeap();
      docHeap[S_MIN_MAX_FREE_BLOCK] = getMaxFreeBlockHeap(true);

      auto docChip = doc[S_CHIP].to<JsonObject>();
      docChip[S_MODEL] = ESP.getChipModel();
      docChip[S_REV] = ESP.getChipRevision();
      docChip[S_CORES] = ESP.getChipCores();
      docChip[S_FREQ] = ESP.getCpuFreqMHz();

      auto docFlash = doc[S_FLASH].to<JsonObject>();
      docFlash[S_SIZE] = ESP.getFlashChipSize();
      docFlash[S_REAL_SIZE] = docFlash[S_SIZE];

      auto reason = esp_reset_reason();
      if (reason != ESP_RST_UNKNOWN && reason != ESP_RST_POWERON && reason != ESP_RST_SW) {
        auto docCrash = doc[S_CRASH].to<JsonObject>();
        docCrash[S_REASON] = getResetReason();
        docCrash[S_CORE] = CrashRecorder::ext.core;
        docCrash[S_HEAP] = CrashRecorder::ext.heap;
        docCrash[S_UPTIME] = CrashRecorder::ext.uptime;

        if (CrashRecorder::backtrace.length > 0 && CrashRecorder::backtrace.length <= CrashRecorder::backtraceMaxLength) {
          String backtraceStr;
          arr2str(backtraceStr, CrashRecorder::backtrace.data, CrashRecorder::backtrace.length);
          docCrash[S_BACKTRACE][S_DATA] = backtraceStr;
          docCrash[S_BACKTRACE][S_CONTINUES] = CrashRecorder::backtrace.continues;
        }

        if (CrashRecorder::epc.length > 0 && CrashRecorder::epc.length <= CrashRecorder::epcMaxLength) {
          String epcStr;
          arr2str(epcStr, CrashRecorder::epc.data, CrashRecorder::epc.length);
          docCrash[S_EPC] = epcStr;
        }
      }
      char filename[64];
      getFilename(filename, sizeof(filename), "debug");

      char contentDispositionValue[128];
      snprintf(
        contentDispositionValue,
        sizeof(contentDispositionValue),
        "attachment; filename=\"%s\"",
        filename
      );

      // send response
      response->addHeader("Content-Disposition", contentDispositionValue);
      response->setLength();
      request->send(response);
    }).addMiddleware(&authMiddleware);


    // not found
    this->webServer->onNotFound([this](AsyncWebServerRequest *request) {
      const auto& url = request->url();
      Log.straceln(L_PORTAL_WEBSERVER, "Page not found, uri: %s", url.c_str());

      if (url.equals("/")) {
        request->send(200, "text/plain", "The file system is not flashed!");

      } else if (network->isApEnabled()) {
        this->onCaptivePortal(request);

      } else {
        request->send(404, "text/plain", "Page not found");
      }
    });

    this->webServer->serveStatic("/robots.txt", LittleFS, "/static/robots.txt", PORTAL_CACHE);
    this->webServer->serveStatic("/favicon.ico", LittleFS, "/static/images/favicon.ico", PORTAL_CACHE);
    this->webServer->serveStatic("/static", LittleFS, "/static", PORTAL_CACHE);
  }

  void loop() {
    // web server
    if (!this->stateWebServer() && (network->isApEnabled() || network->isConnected()) && millis() - this->webServerChangeState >= this->changeStateInterval) {
      this->startWebServer();
      Log.straceln(L_PORTAL_WEBSERVER, "Started: AP up or STA connected");

      // Enabling mDNS
      if (!this->mDnsState && settings.portal.mdns) {
        if (MDNS.begin(networkSettings.hostname)) {
          MDNS.addService("http", "tcp", 80);
          this->mDnsState = true;

          Log.straceln(L_PORTAL_WEBSERVER, "mDNS enabled and service added");
        }
      }

    } else if (this->stateWebServer() && !network->isApEnabled() && !network->isStaEnabled()) {
      this->stopWebServer();
      Log.straceln(L_PORTAL_WEBSERVER, "Stopped: AP and STA down");

      // Disabling mDNS
      if (this->mDnsState) {
        MDNS.end();
        this->mDnsState = false;

        Log.straceln(L_PORTAL_WEBSERVER, "mDNS disabled");
      }
    }

    // Disabling mDNS if disabled in settings
    if (this->mDnsState && !settings.portal.mdns) {
      MDNS.end();
      this->mDnsState = false;

      Log.straceln(L_PORTAL_WEBSERVER, "mDNS disabled");
    }

    // dns server
    if (!this->stateDnsServer() && !network->isConnected() && network->isApEnabled() && this->stateWebServer()) {
      this->startDnsServer();
      Log.straceln(L_PORTAL_DNSSERVER, "Started: AP up");

    } else if (this->stateDnsServer() && (network->isConnected() || !network->isApEnabled() || !this->stateWebServer())) {
      this->stopDnsServer();
      Log.straceln(L_PORTAL_DNSSERVER, "Stopped: AP down/STA connected");
    }

    if (this->stateDnsServer()) {
      this->dnsServer->processNextRequest();
    }

    if (!this->stateDnsServer()) {
      this->delay(250);
    }
  }

  void onCaptivePortal(AsyncWebServerRequest *request) {
    const auto& url = request->url();

    if (url.equals("/connecttest.txt")) {
      request->redirect("http://logout.net", 302);

      Log.straceln(L_PORTAL_CAPTIVE, "Redirect to http://logout.net with 302 code");

    } else if (url.equals("/wpad.dat")) {
      request->send(404);

      Log.straceln(L_PORTAL_CAPTIVE, "Send empty page with 404 code");

    } else if (url.equals("/success.txt")) {
      request->send(200);

      Log.straceln(L_PORTAL_CAPTIVE, "Send empty page with 200 code");

    } else {
      String portalUrl = "http://";
      portalUrl += network->getApIp().toString().c_str();
      portalUrl += '/';

      request->redirect(portalUrl, 302);

      Log.straceln(L_PORTAL_CAPTIVE, "Redirect to portal page with 302 code");
    }
  }

  bool stateWebServer() {
    return this->webServerEnabled;
  }

  void startWebServer() {
    if (this->stateWebServer()) {
      return;
    }

    this->webServer->begin();
    this->webServerEnabled = true;
    this->webServerChangeState = millis();
  }

  void stopWebServer() {
    if (!this->stateWebServer()) {
      return;
    }

    this->webServer->end();
    this->webServerEnabled = false;
    this->webServerChangeState = millis();
  }

  bool stateDnsServer() {
    return this->dnsServerEnabled;
  }

  void startDnsServer() {
    if (this->stateDnsServer()) {
      return;
    }

    this->dnsServer->start(53, "*", network->getApIp());
    this->dnsServerEnabled = true;
  }

  void stopDnsServer() {
    if (!this->stateDnsServer()) {
      return;
    }

    //this->dnsServer->processNextRequest();
    this->dnsServer->stop();
    this->dnsServerEnabled = false;
  }

  static bool isAuthenticationRequired() {
    return !network->isApEnabled() && settings.portal.auth && strlen(settings.portal.password);
  }

  static void getFilename(char* filename, size_t maxSizeFilename, const char* type) {
    const time_t now = time(nullptr);

    tm localNow {};
    localtime_r(&now, &localNow);

    char localNowValue[20];
    strftime(localNowValue, sizeof(localNowValue), "%Y-%m-%d-%H-%M-%S", &localNow);

    snprintf(filename, maxSizeFilename, "%s_%s_%s.json", networkSettings.hostname, localNowValue, type);
  }
};
