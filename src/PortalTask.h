//#define PORTAL_CACHE "max-age=86400"
#define PORTAL_CACHE nullptr
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WebServer.h>
#include <Updater.h>
using WebServer = ESP8266WebServer;
#else
#include <WebServer.h>
#include <Update.h>
#endif
#include <BufferedWebServer.h>
#include <StaticPage.h>
#include <DynamicPage.h>
#include <UpgradeHandler.h>
#include <DNSServer.h>

using namespace NetworkUtils;

extern NetworkMgr* network;
extern FileData fsNetworkSettings, fsSettings, fsSensorsSettings;
extern MqttTask* tMqtt;


class PortalTask : public LeanTask {
public:
  PortalTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {
    this->webServer = new WebServer(80);
    this->bufferedWebServer = new BufferedWebServer(this->webServer, 32u);
    this->dnsServer = new DNSServer();
  }

  ~PortalTask() {
    delete this->bufferedWebServer;

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

  WebServer* webServer = nullptr;
  BufferedWebServer* bufferedWebServer = nullptr;
  DNSServer* dnsServer = nullptr;

  bool webServerEnabled = false;
  bool dnsServerEnabled = false;
  unsigned long webServerChangeState = 0;
  unsigned long dnsServerChangeState = 0;

  #if defined(ARDUINO_ARCH_ESP32)
  const char* getTaskName() override {
    return "Portal";
  }

  /*BaseType_t getTaskCore() override {
    return 1;
  }*/

  int getTaskPriority() override {
    return 1;
  }
  #endif

  void setup() {
    this->dnsServer->setTTL(0);
    this->dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    this->webServer->enableETag(true, [](FS &fs, const String &fName) -> const String {
      char buf[32];
      {
        MD5Builder md5;
        md5.begin();
        md5.add(fName);
        md5.add(" " BUILD_ENV " " BUILD_VERSION " " __DATE__ " " __TIME__);
        md5.calculate();
        md5.getChars(buf);
      }

      String etag;
      etag.reserve(34);
      etag += '\"';
      etag.concat(buf, 32);
      etag += '\"';
      return etag;
    });

    // index page
    /*auto indexPage = (new DynamicPage("/", &LittleFS, "/pages/index.html"))
      ->setTemplateCallback([](const char* var) -> String {
        String result;

        if (strcmp(var, "ver") == 0) {
          result = BUILD_VERSION;
        }

        return result;
      });
    this->webServer->addHandler(indexPage);*/
    this->webServer->addHandler(new StaticPage("/", &LittleFS, F("/pages/index.html"), PORTAL_CACHE));

    // dashboard page
    auto dashboardPage = (new StaticPage("/dashboard.html", &LittleFS, F("/pages/dashboard.html"), PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->isValidCredentials()) {
          this->webServer->requestAuthentication(BASIC_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(dashboardPage);

    // restart
    this->webServer->on(F("/restart.html"), HTTP_GET, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        this->webServer->requestAuthentication(BASIC_AUTH);
        return;
      }

      vars.actions.restart = true;
      this->webServer->sendHeader(F("Location"), "/");
      this->webServer->send(302);
    });

    // network settings page
    auto networkPage = (new StaticPage("/network.html", &LittleFS, F("/pages/network.html"), PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->isValidCredentials()) {
          this->webServer->requestAuthentication(BASIC_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(networkPage);

    // settings page
    auto settingsPage = (new StaticPage("/settings.html", &LittleFS, F("/pages/settings.html"), PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->isValidCredentials()) {
          this->webServer->requestAuthentication(BASIC_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(settingsPage);

    // sensors page
    auto sensorsPage = (new StaticPage("/sensors.html", &LittleFS, F("/pages/sensors.html"), PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->isValidCredentials()) {
          this->webServer->requestAuthentication(BASIC_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(sensorsPage);

    // upgrade page
    auto upgradePage = (new StaticPage("/upgrade.html", &LittleFS, F("/pages/upgrade.html"), PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->isValidCredentials()) {
          this->webServer->requestAuthentication(BASIC_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(upgradePage);

    // OTA
    auto upgradeHandler = (new UpgradeHandler("/api/upgrade"))->setCanUploadCallback([this](const String& uri) {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        this->webServer->sendHeader(F("Connection"), F("close"));
        this->webServer->send(401);
        return false;
      }

      return true;
    })->setBeforeUpgradeCallback([](UpgradeHandler::UpgradeType type) -> bool {
      if (vars.states.restarting) {
        return false;
      }

      vars.states.upgrading = true;
      return true;
    })->setAfterUpgradeCallback([this](const UpgradeHandler::UpgradeResult& fwResult, const UpgradeHandler::UpgradeResult& fsResult) {
      unsigned short status = 200;
      if (fwResult.status == UpgradeHandler::UpgradeStatus::SUCCESS || fsResult.status == UpgradeHandler::UpgradeStatus::SUCCESS) {
        vars.actions.restart = true;

      } else {
        status = 400;
      }

      String response = F("{\"firmware\": {\"status\": ");
      response.concat((short int) fwResult.status);
      response.concat(F(", \"error\": \""));
      response.concat(fwResult.error);
      response.concat(F("\"}, \"filesystem\": {\"status\": "));
      response.concat((short int) fsResult.status);
      response.concat(F(", \"error\": \""));
      response.concat(fsResult.error);
      response.concat(F("\"}}"));
      this->webServer->send(status, F("application/json"), response);

      vars.states.upgrading = false;
    });
    this->webServer->addHandler(upgradeHandler);


    // backup
    this->webServer->on(F("/api/backup/save"), HTTP_GET, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }

      JsonDocument doc;

      auto networkDoc = doc[FPSTR(S_NETWORK)].to<JsonObject>();
      networkSettingsToJson(networkSettings, networkDoc);

      auto settingsDoc = doc[FPSTR(S_SETTINGS)].to<JsonObject>();
      settingsToJson(settings, settingsDoc);

      for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
        auto sensorsettingsDoc = doc[FPSTR(S_SENSORS)][sensorId].to<JsonObject>();
        sensorSettingsToJson(sensorId, Sensors::settings[sensorId], sensorsettingsDoc);
      }

      doc.shrinkToFit();

      this->webServer->sendHeader(F("Content-Disposition"), F("attachment; filename=\"backup.json\""));
      this->bufferedWebServer->send(200, F("application/json"), doc);
    });

    this->webServer->on(F("/api/backup/restore"), HTTP_POST, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }

      if (vars.states.restarting) {
        return this->webServer->send(503);
      }

      const String& plain = this->webServer->arg(0);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/backup/restore %d bytes: %s"), plain.length(), plain.c_str());

      if (plain.length() < 5) {
        this->webServer->send(406);
        return;

      } else if (plain.length() > 2536) {
        this->webServer->send(413);
        return;
      }

      JsonDocument doc;
      DeserializationError dErr = deserializeJson(doc, plain);

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = false;
      if (!doc[FPSTR(S_NETWORK)].isNull() && jsonToNetworkSettings(doc[FPSTR(S_NETWORK)], networkSettings)) {
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

      if (!doc[FPSTR(S_SETTINGS)].isNull() && jsonToSettings(doc[FPSTR(S_SETTINGS)], settings)) {
        fsSettings.update();
        changed = true;
      }

      if (!doc[FPSTR(S_SENSORS)].isNull()) {
        for (auto sensor : doc[FPSTR(S_SENSORS)].as<JsonObject>()) {
          if (!isDigit(sensor.key().c_str())) {
            continue;
          }

          int sensorId = atoi(sensor.key().c_str());
          if (sensorId < 0 || sensorId > 255 || !Sensors::isValidSensorId(sensorId)) {
            continue;
          }
          
          if (jsonToSensorSettings(sensorId, sensor.value(), Sensors::settings[sensorId])) {
            fsSensorsSettings.update();
            changed = true;
          }
        }
      }
      
      doc.clear();
      doc.shrinkToFit();

      if (changed) {
        vars.actions.restart = true;
      }

      this->webServer->send(changed ? 201 : 200);
    });

    // network
    this->webServer->on(F("/api/network/settings"), HTTP_GET, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }

      JsonDocument doc;
      networkSettingsToJson(networkSettings, doc);
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, F("application/json"), doc);
    });

    this->webServer->on(F("/api/network/settings"), HTTP_POST, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }
      
      if (vars.states.restarting) {
        return this->webServer->send(503);
      }

      const String& plain = this->webServer->arg(0);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/network/settings %d bytes: %s"), plain.length(), plain.c_str());

      if (plain.length() < 5) {
        this->webServer->send(406);
        return;

      } else if (plain.length() > 512) {
        this->webServer->send(413);
        return;
      }

      JsonDocument doc;
      DeserializationError dErr = deserializeJson(doc, plain);

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = jsonToNetworkSettings(doc, networkSettings);
      doc.clear();
      doc.shrinkToFit();

      networkSettingsToJson(networkSettings, doc);
      doc.shrinkToFit();
      
      this->bufferedWebServer->send(changed ? 201 : 200, F("application/json"), doc);

      if (changed) {
        doc.clear();
        doc.shrinkToFit();

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

    this->webServer->on(F("/api/network/scan"), HTTP_GET, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }

      auto apCount = WiFi.scanComplete();
      if (apCount <= 0) {
        if (apCount != WIFI_SCAN_RUNNING) {
          #ifdef ARDUINO_ARCH_ESP8266
          WiFi.scanNetworks(true, true);
          #else
          WiFi.scanNetworks(true, true, true);
          #endif
        }
        
        this->webServer->send(404);
        return;
      }
      
      JsonDocument doc;
      for (short int i = 0; i < apCount; i++) {
        const String& ssid = WiFi.SSID(i);
        doc[i][FPSTR(S_SSID)] = ssid;
        doc[i][FPSTR(S_BSSID)] = WiFi.BSSIDstr(i);
        doc[i][FPSTR(S_SIGNAL_QUALITY)] = NetworkMgr::rssiToSignalQuality(WiFi.RSSI(i));
        doc[i][FPSTR(S_CHANNEL)] = WiFi.channel(i);
        doc[i][FPSTR(S_HIDDEN)] = !ssid.length();
        #ifdef ARDUINO_ARCH_ESP8266
        const bss_info* info = WiFi.getScanInfoByIndex(i);
        doc[i][FPSTR(S_AUTH)] = info->authmode;
        #else
        doc[i][FPSTR(S_AUTH)] = WiFi.encryptionType(i);
        #endif
      }
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, F("application/json"), doc);

      WiFi.scanDelete();
    });


    // settings
    this->webServer->on(F("/api/settings"), HTTP_GET, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }

      JsonDocument doc;
      settingsToJson(settings, doc);
      doc.shrinkToFit();
      
      this->bufferedWebServer->send(200, F("application/json"), doc);
    });

    this->webServer->on(F("/api/settings"), HTTP_POST, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }
      
      if (vars.states.restarting) {
        return this->webServer->send(503);
      }

      const String& plain = this->webServer->arg(0);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/settings %d bytes: %s"), plain.length(), plain.c_str());

      if (plain.length() < 5) {
        this->webServer->send(406);
        return;

      } else if (plain.length() > 2536) {
        this->webServer->send(413);
        return;
      }

      JsonDocument doc;
      DeserializationError dErr = deserializeJson(doc, plain);

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = jsonToSettings(doc, settings);
      doc.clear();
      doc.shrinkToFit();

      settingsToJson(settings, doc);
      doc.shrinkToFit();

      this->bufferedWebServer->send(changed ? 201 : 200, F("application/json"), doc);

      if (changed) {
        doc.clear();
        doc.shrinkToFit();

        fsSettings.update();
        tMqtt->resetPublishedSettingsTime();
      }
    });


    // sensors list
    this->webServer->on(F("/api/sensors"), HTTP_GET, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }

      bool detailed = false;
      if (this->webServer->hasArg(F("detailed"))) {
        detailed = this->webServer->arg(F("detailed")).toInt() > 0;
      }
      
      JsonDocument doc;
      for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
        if (detailed) {
          auto& sSensor = Sensors::settings[sensorId];
          doc[sensorId][FPSTR(S_ENABLED)] = sSensor.enabled;
          doc[sensorId][FPSTR(S_NAME)] = sSensor.name;
          doc[sensorId][FPSTR(S_PURPOSE)] = static_cast<uint8_t>(sSensor.purpose);
          sensorResultToJson(sensorId, doc[sensorId]);

        } else {
          doc[sensorId] = Sensors::settings[sensorId].name;
        }
      }
      
      doc.shrinkToFit();
      this->bufferedWebServer->send(200, F("application/json"), doc);
    });

    // sensor settings
    this->webServer->on(F("/api/sensor"), HTTP_GET, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }
      
      if (!this->webServer->hasArg(F("id"))) {
        return this->webServer->send(400);
      }

      auto id = this->webServer->arg(F("id"));
      if (!isDigit(id.c_str())) {
        return this->webServer->send(400);
      }

      uint8_t sensorId = id.toInt();
      id.clear();
      if (!Sensors::isValidSensorId(sensorId)) {
        return this->webServer->send(404);
      }

      JsonDocument doc;
      sensorSettingsToJson(sensorId, Sensors::settings[sensorId], doc);
      doc.shrinkToFit();
      this->bufferedWebServer->send(200, F("application/json"), doc);
    });
    
    this->webServer->on(F("/api/sensor"), HTTP_POST, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }
      
      if (vars.states.restarting) {
        return this->webServer->send(503);
      }

      #ifdef ARDUINO_ARCH_ESP8266
      if (!this->webServer->hasArg(F("id")) || this->webServer->args() != 1) {
        return this->webServer->send(400);
      }
      #else
      if (!this->webServer->hasArg(F("id")) || this->webServer->args() != 2) {
        return this->webServer->send(400);
      }
      #endif

      auto id = this->webServer->arg(F("id"));
      if (!isDigit(id.c_str())) {
        return this->webServer->send(400);
      }

      uint8_t sensorId = id.toInt();
      id.clear();
      if (!Sensors::isValidSensorId(sensorId)) {
        return this->webServer->send(404);
      }

      auto plain = this->webServer->arg(1);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/sensor/?id=%hhu %d bytes: %s"), sensorId, plain.length(), plain.c_str());

      if (plain.length() < 5) {
        return this->webServer->send(406);

      } else if (plain.length() > 1024) {
        return this->webServer->send(413);
      }

      bool changed = false;
      auto prevSettings = Sensors::settings[sensorId];
      {
        JsonDocument doc;
        DeserializationError dErr = deserializeJson(doc, plain);
        plain.clear();

        if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
          return this->webServer->send(400);
        }

        if (jsonToSensorSettings(sensorId, doc, Sensors::settings[sensorId])) {
          changed = true;
        }
      }

      {
        JsonDocument doc;
        auto& sSettings = Sensors::settings[sensorId];
        sensorSettingsToJson(sensorId, sSettings, doc);
        doc.shrinkToFit();
        
        this->bufferedWebServer->send(changed ? 201 : 200, F("application/json"), doc);
      }

      if (changed) {
        tMqtt->rebuildHaEntity(sensorId, prevSettings);
        fsSensorsSettings.update();
      }
    });


    // vars
    this->webServer->on(F("/api/vars"), HTTP_GET, [this]() {
      JsonDocument doc;
      varsToJson(vars, doc);
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, F("application/json"), doc);
    });

    this->webServer->on(F("/api/vars"), HTTP_POST, [this]() {
      if (this->isAuthRequired() && !this->isValidCredentials()) {
        return this->webServer->send(401);
      }

      const String& plain = this->webServer->arg(0);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/vars %d bytes: %s"), plain.length(), plain.c_str());

      if (plain.length() < 5) {
        this->webServer->send(406);
        return;

      } else if (plain.length() > 1536) {
        this->webServer->send(413);
        return;
      }

      JsonDocument doc;
      DeserializationError dErr = deserializeJson(doc, plain);

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = jsonToVars(doc, vars);
      doc.clear();
      doc.shrinkToFit();

      varsToJson(vars, doc);
      doc.shrinkToFit();
      
      this->bufferedWebServer->send(changed ? 201 : 200, F("application/json"), doc);

      if (changed) {
        doc.clear();
        doc.shrinkToFit();
        
        tMqtt->resetPublishedVarsTime();
      }
    });

    this->webServer->on(F("/api/info"), HTTP_GET, [this]() {
      bool isConnected = network->isConnected();

      JsonDocument doc;

      auto docSystem = doc[FPSTR(S_SYSTEM)].to<JsonObject>();
      docSystem[FPSTR(S_RESET_REASON)] = getResetReason();
      docSystem[FPSTR(S_UPTIME)] = millis() / 1000;

      auto docNetwork = doc[FPSTR(S_NETWORK)].to<JsonObject>();
      docNetwork[FPSTR(S_HOSTNAME)] = networkSettings.hostname;
      docNetwork[FPSTR(S_MAC)] = network->getStaMac();
      docNetwork[FPSTR(S_CONNECTED)] = isConnected;
      docNetwork[FPSTR(S_SSID)] = network->getStaSsid();
      docNetwork[FPSTR(S_SIGNAL_QUALITY)] = isConnected ? NetworkMgr::rssiToSignalQuality(network->getRssi()) : 0;
      docNetwork[FPSTR(S_CHANNEL)] = isConnected ? network->getStaChannel() : 0;
      docNetwork[FPSTR(S_IP)] = isConnected ? network->getStaIp().toString() : "";
      docNetwork[FPSTR(S_SUBNET)] = isConnected ? network->getStaSubnet().toString() : "";
      docNetwork[FPSTR(S_GATEWAY)] = isConnected ? network->getStaGateway().toString() : "";
      docNetwork[FPSTR(S_DNS)] = isConnected ? network->getStaDns().toString() : "";

      auto docBuild = doc[FPSTR(S_BUILD)].to<JsonObject>();
      docBuild[FPSTR(S_VERSION)] = BUILD_VERSION;
      docBuild[FPSTR(S_DATE)] = __DATE__ " " __TIME__;
      docBuild[FPSTR(S_ENV)] = BUILD_ENV;
      #ifdef ARDUINO_ARCH_ESP8266
      docBuild[FPSTR(S_CORE)] = ESP.getCoreVersion();
      docBuild[FPSTR(S_SDK)] = ESP.getSdkVersion();
      #elif ARDUINO_ARCH_ESP32
      docBuild[FPSTR(S_CORE)] = ESP.getCoreVersion();
      docBuild[FPSTR(S_SDK)] = ESP.getSdkVersion();
      #else
      docBuild[FPSTR(S_CORE)] = 0;
      docBuild[FPSTR(S_SDK)] = 0;
      #endif

      auto docHeap = doc[FPSTR(S_HEAP)].to<JsonObject>();
      docHeap[FPSTR(S_TOTAL)] = getTotalHeap();
      docHeap[FPSTR(S_FREE)] = getFreeHeap();
      docHeap[FPSTR(S_MIN_FREE)] = getFreeHeap(true);
      docHeap[FPSTR(S_MAX_FREE_BLOCK)] = getMaxFreeBlockHeap();
      docHeap[FPSTR(S_MIN_MAX_FREE_BLOCK)] = getMaxFreeBlockHeap(true);
      
      auto docChip = doc[FPSTR(S_CHIP)].to<JsonObject>();
      #ifdef ARDUINO_ARCH_ESP8266
      docChip[FPSTR(S_MODEL)] = esp_is_8285() ? F("ESP8285") : F("ESP8266");
      docChip[FPSTR(S_REV)] = 0;
      docChip[FPSTR(S_CORES)] = 1;
      docChip[FPSTR(S_FREQ)] = ESP.getCpuFreqMHz();
      #elif ARDUINO_ARCH_ESP32
      docChip[FPSTR(S_MODEL)] = ESP.getChipModel();
      docChip[FPSTR(S_REV)] = ESP.getChipRevision();
      docChip[FPSTR(S_CORES)] = ESP.getChipCores();
      docChip[FPSTR(S_FREQ)] = ESP.getCpuFreqMHz();
      #else
      docChip[FPSTR(S_MODEL)] = 0;
      docChip[FPSTR(S_REV)] = 0;
      docChip[FPSTR(S_CORES)] = 0;
      docChip[FPSTR(S_FREQ)] = 0;
      #endif

      auto docFlash = doc[FPSTR(S_FLASH)].to<JsonObject>();
      #ifdef ARDUINO_ARCH_ESP8266
      docFlash[FPSTR(S_SIZE)] = ESP.getFlashChipSize();
      docFlash[FPSTR(S_REAL_SIZE)] = ESP.getFlashChipRealSize();
      #elif ARDUINO_ARCH_ESP32
      docFlash[FPSTR(S_SIZE)] = ESP.getFlashChipSize();
      docFlash[FPSTR(S_REAL_SIZE)] = docFlash[FPSTR(S_SIZE)];
      #else
      docFlash[FPSTR(S_SIZE)] = 0;
      docFlash[FPSTR(S_REAL_SIZE)] = 0;
      #endif

      doc.shrinkToFit();

      this->bufferedWebServer->send(200, F("application/json"), doc);
    });

    this->webServer->on(F("/api/debug"), HTTP_GET, [this]() {
      JsonDocument doc;

      auto docBuild = doc[FPSTR(S_BUILD)].to<JsonObject>();
      docBuild[FPSTR(S_VERSION)] = BUILD_VERSION;
      docBuild[FPSTR(S_DATE)] = __DATE__ " " __TIME__;
      docBuild[FPSTR(S_ENV)] = BUILD_ENV;
      #ifdef ARDUINO_ARCH_ESP8266
      docBuild[FPSTR(S_CORE)] = ESP.getCoreVersion();
      docBuild[FPSTR(S_SDK)] = ESP.getSdkVersion();
      #elif ARDUINO_ARCH_ESP32
      docBuild[FPSTR(S_CORE)] = ESP.getCoreVersion();
      docBuild[FPSTR(S_SDK)] = ESP.getSdkVersion();
      #else
      docBuild[FPSTR(S_CORE)] = 0;
      docBuild[FPSTR(S_SDK)] = 0;
      #endif

      auto docHeap = doc[FPSTR(S_HEAP)].to<JsonObject>();
      docHeap[FPSTR(S_TOTAL)] = getTotalHeap();
      docHeap[FPSTR(S_FREE)] = getFreeHeap();
      docHeap[FPSTR(S_MIN_FREE)] = getFreeHeap(true);
      docHeap[FPSTR(S_MAX_FREE_BLOCK)] = getMaxFreeBlockHeap();
      docHeap[FPSTR(S_MIN_MAX_FREE_BLOCK)] = getMaxFreeBlockHeap(true);
      
      auto docChip = doc[FPSTR(S_CHIP)].to<JsonObject>();
      #ifdef ARDUINO_ARCH_ESP8266
      docChip[FPSTR(S_MODEL)] = esp_is_8285() ? F("ESP8285") : F("ESP8266");
      docChip[FPSTR(S_REV)] = 0;
      docChip[FPSTR(S_CORES)] = 1;
      docChip[FPSTR(S_FREQ)] = ESP.getCpuFreqMHz();
      #elif ARDUINO_ARCH_ESP32
      docChip[FPSTR(S_MODEL)] = ESP.getChipModel();
      docChip[FPSTR(S_REV)] = ESP.getChipRevision();
      docChip[FPSTR(S_CORES)] = ESP.getChipCores();
      docChip[FPSTR(S_FREQ)] = ESP.getCpuFreqMHz();
      #else
      docChip[FPSTR(S_MODEL)] = 0;
      docChip[FPSTR(S_REV)] = 0;
      docChip[FPSTR(S_CORES)] = 0;
      docChip[FPSTR(S_FREQ)] = 0;
      #endif

      auto docFlash = doc[FPSTR(S_FLASH)].to<JsonObject>();
      #ifdef ARDUINO_ARCH_ESP8266
      docFlash[FPSTR(S_SIZE)] = ESP.getFlashChipSize();
      docFlash[FPSTR(S_REAL_SIZE)] = ESP.getFlashChipRealSize();
      #elif ARDUINO_ARCH_ESP32
      docFlash[FPSTR(S_SIZE)] = ESP.getFlashChipSize();
      docFlash[FPSTR(S_REAL_SIZE)] = docFlash[FPSTR(S_SIZE)];
      #else
      docFlash[FPSTR(S_SIZE)] = 0;
      docFlash[FPSTR(S_REAL_SIZE)] = 0;
      #endif

      #if defined(ARDUINO_ARCH_ESP32)
      auto reason = esp_reset_reason();
      if (reason != ESP_RST_UNKNOWN && reason != ESP_RST_POWERON && reason != ESP_RST_SW) {
      #elif defined(ARDUINO_ARCH_ESP8266)
      auto reason = ESP.getResetInfoPtr()->reason;
      if (reason != REASON_DEFAULT_RST && reason != REASON_SOFT_RESTART && reason != REASON_EXT_SYS_RST) {
      #else
      if (false) {
      #endif
        auto docCrash = doc[FPSTR(S_CRASH)].to<JsonObject>();
        docCrash[FPSTR(S_REASON)] = getResetReason();
        docCrash[FPSTR(S_CORE)] = CrashRecorder::ext.core;
        docCrash[FPSTR(S_HEAP)] = CrashRecorder::ext.heap;
        docCrash[FPSTR(S_UPTIME)] = CrashRecorder::ext.uptime;

        if (CrashRecorder::backtrace.length > 0 && CrashRecorder::backtrace.length <= CrashRecorder::backtraceMaxLength) {
          String backtraceStr;
          arr2str(backtraceStr, CrashRecorder::backtrace.data, CrashRecorder::backtrace.length);
          docCrash[FPSTR(S_BACKTRACE)][FPSTR(S_DATA)] = backtraceStr;
          docCrash[FPSTR(S_BACKTRACE)][FPSTR(S_CONTINUES)] = CrashRecorder::backtrace.continues;
        }

        if (CrashRecorder::epc.length > 0 && CrashRecorder::epc.length <= CrashRecorder::epcMaxLength) {
          String epcStr;
          arr2str(epcStr, CrashRecorder::epc.data, CrashRecorder::epc.length);
          docCrash[FPSTR(S_EPC)] = epcStr;
        }
      }
      
      doc.shrinkToFit();

      this->webServer->sendHeader(F("Content-Disposition"), F("attachment; filename=\"debug.json\""));
      this->bufferedWebServer->send(200, F("application/json"), doc, true);
    });


    // not found
    this->webServer->onNotFound([this]() {
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Page not found, uri: %s"), this->webServer->uri().c_str());

      const String& uri = this->webServer->uri();
      if (uri.equals(F("/"))) {
        this->webServer->send(200, F("text/plain"), F("The file system is not flashed!"));

      } else if (network->isApEnabled()) {
        this->onCaptivePortal();

      } else {
        this->webServer->send(404, F("text/plain"), F("Page not found"));
      }
    });

    this->webServer->serveStatic("/favicon.ico", LittleFS, "/static/images/favicon.ico", PORTAL_CACHE);
    this->webServer->serveStatic("/static", LittleFS, "/static", PORTAL_CACHE);
  }

  void loop() {
    // web server
    if (!this->stateWebServer() && (network->isApEnabled() || network->isConnected()) && millis() - this->webServerChangeState >= this->changeStateInterval) {
      #ifdef ARDUINO_ARCH_ESP32
      this->delay(250);
      #endif
      
      this->startWebServer();
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Started: AP up or STA connected"));

      #ifdef ARDUINO_ARCH_ESP8266
      ::optimistic_yield(1000);
      #endif

    } else if (this->stateWebServer() && !network->isApEnabled() && !network->isStaEnabled()) {
      this->stopWebServer();
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Stopped: AP and STA down"));

      #ifdef ARDUINO_ARCH_ESP8266
      ::optimistic_yield(1000);
      #endif
    }

    // dns server
    if (!this->stateDnsServer() && this->stateWebServer() && network->isApEnabled() && network->hasApClients() && millis() - this->dnsServerChangeState >= this->changeStateInterval) {
      this->startDnsServer();
      Log.straceln(FPSTR(L_PORTAL_DNSSERVER), F("Started: AP up"));
      
      #ifdef ARDUINO_ARCH_ESP8266
      ::optimistic_yield(1000);
      #endif

    } else if (this->stateDnsServer() && (!network->isApEnabled() || !this->stateWebServer())) {
      this->stopDnsServer();
      Log.straceln(FPSTR(L_PORTAL_DNSSERVER), F("Stopped: AP down"));

      #ifdef ARDUINO_ARCH_ESP8266
      ::optimistic_yield(1000);
      #endif
    }

    if (this->stateDnsServer()) {
      this->dnsServer->processNextRequest();

      #ifdef ARDUINO_ARCH_ESP8266
      ::optimistic_yield(1000);
      #endif
    }

    if (this->stateWebServer()) {
      #ifdef ARDUINO_ARCH_ESP32
      // Fix ERR_CONNECTION_RESET for Chrome based browsers
      auto& client = this->webServer->client();
      if (!client.getNoDelay()) {
        client.setNoDelay(true);
      }
      #endif

      this->webServer->handleClient();
    }

    if (!this->stateDnsServer() && !this->stateWebServer()) {
      this->delay(250);
    }
  }

  bool isAuthRequired() {
    return !network->isApEnabled() && settings.portal.auth && strlen(settings.portal.password);
  }

  bool isValidCredentials() {
    return this->webServer->authenticate(settings.portal.login, settings.portal.password);
  }

  void onCaptivePortal() {
    const String& uri = this->webServer->uri();

    if (uri.equals(F("/connecttest.txt"))) {
      this->webServer->sendHeader(F("Location"), F("http://logout.net"));
      this->webServer->send(302);

      Log.straceln(FPSTR(L_PORTAL_CAPTIVE), F("Redirect to http://logout.net with 302 code"));

    } else if (uri.equals(F("/wpad.dat"))) {
      this->webServer->send(404);

      Log.straceln(FPSTR(L_PORTAL_CAPTIVE), F("Send empty page with 404 code"));

    } else if (uri.equals(F("/success.txt"))) {
      this->webServer->send(200);

      Log.straceln(FPSTR(L_PORTAL_CAPTIVE), F("Send empty page with 200 code"));

    } else {
      String portalUrl = F("http://");
      portalUrl += network->getApIp().toString().c_str();
      portalUrl += '/';

      this->webServer->sendHeader(F("Location"), portalUrl);
      this->webServer->send(302);

      Log.straceln(FPSTR(L_PORTAL_CAPTIVE), F("Redirect to portal page with 302 code"));
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
    #ifdef ARDUINO_ARCH_ESP8266
    this->webServer->getServer().setNoDelay(true);
    #endif
    this->webServerEnabled = true;
    this->webServerChangeState = millis();
  }

  void stopWebServer() {
    if (!this->stateWebServer()) {
      return;
    }

    //this->webServer->handleClient();
    this->webServer->stop();
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
    this->dnsServerChangeState = millis();
  }

  void stopDnsServer() {
    if (!this->stateDnsServer()) {
      return;
    }

    //this->dnsServer->processNextRequest();
    this->dnsServer->stop();
    this->dnsServerEnabled = false;
    this->dnsServerChangeState = millis();
  }
};