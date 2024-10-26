#define PORTAL_CACHE_TIME "max-age=86400"
#define PORTAL_CACHE (settings.system.logLevel >= TinyLogger::Level::TRACE ? nullptr : PORTAL_CACHE_TIME)
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
extern FileData fsSettings, fsNetworkSettings;
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
    #ifdef ARDUINO_ARCH_ESP8266
    this->webServer->enableETag(true);
    #endif

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
    this->webServer->addHandler(new StaticPage("/", &LittleFS, "/pages/index.html", PORTAL_CACHE));

    // dashboard page
    auto dashboardPage = (new StaticPage("/dashboard.html", &LittleFS, "/pages/dashboard.html", PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->requestAuthentication(DIGEST_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(dashboardPage);

    // restart
    this->webServer->on("/restart.html", HTTP_GET, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->send(401);
          return;
        }
      }

      vars.actions.restart = true;
      this->webServer->sendHeader("Location", "/");
      this->webServer->send(302);
    });

    // network settings page
    auto networkPage = (new StaticPage("/network.html", &LittleFS, "/pages/network.html", PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->requestAuthentication(DIGEST_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(networkPage);

    // settings page
    auto settingsPage = (new StaticPage("/settings.html", &LittleFS, "/pages/settings.html", PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->requestAuthentication(DIGEST_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(settingsPage);

    // upgrade page
    auto upgradePage = (new StaticPage("/upgrade.html", &LittleFS, "/pages/upgrade.html", PORTAL_CACHE))
      ->setBeforeSendCallback([this]() {
        if (this->isAuthRequired() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->requestAuthentication(DIGEST_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(upgradePage);

    // OTA
    auto upgradeHandler = (new UpgradeHandler("/api/upgrade"))->setCanUploadCallback([this](const String& uri) {
      if (this->isAuthRequired() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
        this->webServer->sendHeader("Connection", "close");
        this->webServer->send(401);
        return false;
      }

      return true;
    })->setBeforeUpgradeCallback([](UpgradeHandler::UpgradeType type) -> bool {
      return true;
    })->setAfterUpgradeCallback([this](const UpgradeHandler::UpgradeResult& fwResult, const UpgradeHandler::UpgradeResult& fsResult) {
      unsigned short status = 200;
      if (fwResult.status == UpgradeHandler::UpgradeStatus::SUCCESS || fsResult.status == UpgradeHandler::UpgradeStatus::SUCCESS) {
        vars.actions.restart = true;

      } else {
        status = 400;
      }

      String response = "{\"firmware\": {\"status\": ";
      response.concat((short int) fwResult.status);
      response.concat(", \"error\": \"");
      response.concat(fwResult.error);
      response.concat("\"}, \"filesystem\": {\"status\": ");
      response.concat((short int) fsResult.status);
      response.concat(", \"error\": \"");
      response.concat(fsResult.error);
      response.concat("\"}}");
      this->webServer->send(status, "application/json", response);
    });
    this->webServer->addHandler(upgradeHandler);


    // backup
    this->webServer->on("/api/backup/save", HTTP_GET, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      JsonDocument networkSettingsDoc;
      networkSettingsToJson(networkSettings, networkSettingsDoc);
      networkSettingsDoc.shrinkToFit();

      JsonDocument settingsDoc;
      settingsToJson(settings, settingsDoc);
      settingsDoc.shrinkToFit();

      JsonDocument doc;
      doc["network"] = networkSettingsDoc;
      doc["settings"] = settingsDoc;
      doc.shrinkToFit();

      this->webServer->sendHeader(F("Content-Disposition"), F("attachment; filename=\"backup.json\""));
      this->bufferedWebServer->send(200, "application/json", doc);
    });

    this->webServer->on("/api/backup/restore", HTTP_POST, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      String plain = this->webServer->arg(0);
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
      plain.clear();

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = false;
      if (doc["settings"] && jsonToSettings(doc["settings"], settings)) {
        vars.actions.restart = true;
        fsSettings.update();
        changed = true;
      }

      if (doc["network"] && jsonToNetworkSettings(doc["network"], networkSettings)) {
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
        changed = true;
      }

      doc.clear();
      doc.shrinkToFit();

      this->webServer->send(changed ? 201 : 200);
    });

    // network
    this->webServer->on("/api/network/settings", HTTP_GET, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      JsonDocument doc;
      networkSettingsToJson(networkSettings, doc);
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, "application/json", doc);
    });

    this->webServer->on("/api/network/settings", HTTP_POST, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      String plain = this->webServer->arg(0);
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
      plain.clear();

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = jsonToNetworkSettings(doc, networkSettings);
      doc.clear();
      doc.shrinkToFit();

      networkSettingsToJson(networkSettings, doc);
      doc.shrinkToFit();
      
      this->bufferedWebServer->send(changed ? 201 : 200, "application/json", doc);

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

    this->webServer->on("/api/network/scan", HTTP_GET, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->send(401);
          return;
        }
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
        String ssid = WiFi.SSID(i);
        doc[i]["ssid"] = ssid;
        doc[i]["bssid"] = WiFi.BSSIDstr(i);
        doc[i]["signalQuality"] = NetworkMgr::rssiToSignalQuality(WiFi.RSSI(i));
        doc[i]["channel"] = WiFi.channel(i);
        doc[i]["hidden"] = !ssid.length();
        #ifdef ARDUINO_ARCH_ESP8266
        const bss_info* info = WiFi.getScanInfoByIndex(i);
        doc[i]["auth"] = info->authmode;
        #else
        doc[i]["auth"] = WiFi.encryptionType(i);
        #endif
      }
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, "application/json", doc);

      WiFi.scanDelete();
    });


    // settings
    this->webServer->on("/api/settings", HTTP_GET, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      JsonDocument doc;
      settingsToJson(settings, doc);
      doc.shrinkToFit();
      
      this->bufferedWebServer->send(200, "application/json", doc);
    });

    this->webServer->on("/api/settings", HTTP_POST, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      String plain = this->webServer->arg(0);
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
      plain.clear();

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = jsonToSettings(doc, settings);
      doc.clear();
      doc.shrinkToFit();

      settingsToJson(settings, doc);
      doc.shrinkToFit();

      this->bufferedWebServer->send(changed ? 201 : 200, "application/json", doc);

      if (changed) {
        doc.clear();
        doc.shrinkToFit();

        fsSettings.update();
        tMqtt->resetPublishedSettingsTime();
      }
    });


    // vars
    this->webServer->on("/api/vars", HTTP_GET, [this]() {
      JsonDocument doc;
      varsToJson(vars, doc);
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, "application/json", doc);
    });

    this->webServer->on("/api/vars", HTTP_POST, [this]() {
      if (this->isAuthRequired()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      String plain = this->webServer->arg(0);
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
      plain.clear();

      if (dErr != DeserializationError::Ok || doc.isNull() || !doc.size()) {
        this->webServer->send(400);
        return;
      }

      bool changed = jsonToVars(doc, vars);
      doc.clear();
      doc.shrinkToFit();

      varsToJson(vars, doc);
      doc.shrinkToFit();
      
      this->bufferedWebServer->send(changed ? 201 : 200, "application/json", doc);

      if (changed) {
        doc.clear();
        doc.shrinkToFit();
        
        tMqtt->resetPublishedVarsTime();
      }
    });

    this->webServer->on("/api/info", HTTP_GET, [this]() {
      bool isConnected = network->isConnected();

      JsonDocument doc;
      doc["system"]["resetReason"] = getResetReason();
      doc["system"]["uptime"] = millis() / 1000ul;

      doc["network"]["hostname"] = networkSettings.hostname;
      doc["network"]["mac"] = network->getStaMac();
      doc["network"]["connected"] = isConnected;
      doc["network"]["ssid"] = network->getStaSsid();
      doc["network"]["signalQuality"] = isConnected ? NetworkMgr::rssiToSignalQuality(network->getRssi()) : 0;
      doc["network"]["channel"] = isConnected ? network->getStaChannel() : 0;
      doc["network"]["ip"] = isConnected ? network->getStaIp().toString() : "";
      doc["network"]["subnet"] = isConnected ? network->getStaSubnet().toString() : "";
      doc["network"]["gateway"] = isConnected ? network->getStaGateway().toString() : "";
      doc["network"]["dns"] = isConnected ? network->getStaDns().toString() : "";

      doc["build"]["version"] = BUILD_VERSION;
      doc["build"]["date"] = __DATE__ " " __TIME__;
      doc["build"]["env"] = BUILD_ENV;

      doc["heap"]["total"] = getTotalHeap();
      doc["heap"]["free"] = getFreeHeap();
      doc["heap"]["minFree"] = getFreeHeap(true);
      doc["heap"]["maxFreeBlock"] = getMaxFreeBlockHeap();
      doc["heap"]["minMaxFreeBlock"] = getMaxFreeBlockHeap(true);
      
      #ifdef ARDUINO_ARCH_ESP8266
      doc["build"]["core"] = ESP.getCoreVersion();
      doc["build"]["sdk"] = ESP.getSdkVersion();
      doc["chip"]["model"] = esp_is_8285() ? "ESP8285" : "ESP8266";
      doc["chip"]["rev"] = 0;
      doc["chip"]["cores"] = 1;
      doc["chip"]["freq"] = ESP.getCpuFreqMHz();
      doc["flash"]["size"] = ESP.getFlashChipSize();
      doc["flash"]["realSize"] = ESP.getFlashChipRealSize();
      #elif ARDUINO_ARCH_ESP32
      doc["build"]["core"] = ESP.getCoreVersion();
      doc["build"]["sdk"] = ESP.getSdkVersion();
      doc["chip"]["model"] = ESP.getChipModel();
      doc["chip"]["rev"] = ESP.getChipRevision();
      doc["chip"]["cores"] = ESP.getChipCores();
      doc["chip"]["freq"] = ESP.getCpuFreqMHz();
      doc["flash"]["size"] = ESP.getFlashChipSize();
      doc["flash"]["realSize"] = doc["flash"]["size"];
      #else
      doc["build"]["core"] = 0;
      doc["build"]["sdk"] = 0;
      doc["chip"]["model"] = 0;
      doc["chip"]["rev"] = 0;
      doc["chip"]["cores"] = 0;
      doc["chip"]["freq"] = 0;
      doc["flash"]["size"] = 0;
      doc["flash"]["realSize"] = 0;
      #endif

      doc.shrinkToFit();

      this->bufferedWebServer->send(200, "application/json", doc);
    });

    this->webServer->on("/api/debug", HTTP_GET, [this]() {
      JsonDocument doc;
      doc["build"]["version"] = BUILD_VERSION;
      doc["build"]["date"] = __DATE__ " " __TIME__;
      doc["build"]["env"] = BUILD_ENV;
      doc["heap"]["total"] = getTotalHeap();
      doc["heap"]["free"] = getFreeHeap();
      doc["heap"]["minFree"] = getFreeHeap(true);
      doc["heap"]["maxFreeBlock"] = getMaxFreeBlockHeap();
      doc["heap"]["minMaxFreeBlock"] = getMaxFreeBlockHeap(true);

      #if defined(ARDUINO_ARCH_ESP32)
      auto reason = esp_reset_reason();
      if (reason != ESP_RST_UNKNOWN && reason != ESP_RST_POWERON && reason != ESP_RST_SW) {
      #elif defined(ARDUINO_ARCH_ESP8266)
      auto reason = ESP.getResetInfoPtr()->reason;
      if (reason != REASON_DEFAULT_RST && reason != REASON_SOFT_RESTART && reason != REASON_EXT_SYS_RST) {
      #else
      if (false) {
      #endif
        doc["crash"]["reason"] = getResetReason();
        doc["crash"]["core"] = CrashRecorder::ext.core;
        doc["crash"]["heap"] = CrashRecorder::ext.heap;
        doc["crash"]["uptime"] = CrashRecorder::ext.uptime;

        if (CrashRecorder::backtrace.length > 0 && CrashRecorder::backtrace.length <= CrashRecorder::backtraceMaxLength) {
          String backtraceStr;
          arr2str(backtraceStr, CrashRecorder::backtrace.data, CrashRecorder::backtrace.length);
          doc["crash"]["backtrace"]["data"] = backtraceStr;
          doc["crash"]["backtrace"]["continues"] = CrashRecorder::backtrace.continues;
        }

        if (CrashRecorder::epc.length > 0 && CrashRecorder::epc.length <= CrashRecorder::epcMaxLength) {
          String epcStr;
          arr2str(epcStr, CrashRecorder::epc.data, CrashRecorder::epc.length);
          doc["crash"]["epc"] = epcStr;
        }
      }
      
      #ifdef ARDUINO_ARCH_ESP8266
      doc["build"]["core"] = ESP.getCoreVersion();
      doc["build"]["sdk"] = ESP.getSdkVersion();
      doc["chip"]["model"] = esp_is_8285() ? "ESP8285" : "ESP8266";
      doc["chip"]["rev"] = 0;
      doc["chip"]["cores"] = 1;
      doc["chip"]["freq"] = ESP.getCpuFreqMHz();
      doc["flash"]["size"] = ESP.getFlashChipSize();
      doc["flash"]["realSize"] = ESP.getFlashChipRealSize();
      #elif ARDUINO_ARCH_ESP32
      doc["build"]["core"] = ESP.getCoreVersion();
      doc["build"]["sdk"] = ESP.getSdkVersion();
      doc["chip"]["model"] = ESP.getChipModel();
      doc["chip"]["rev"] = ESP.getChipRevision();
      doc["chip"]["cores"] = ESP.getChipCores();
      doc["chip"]["freq"] = ESP.getCpuFreqMHz();
      doc["flash"]["size"] = ESP.getFlashChipSize();
      doc["flash"]["realSize"] = doc["flash"]["size"];
      #else
      doc["build"]["core"] = 0;
      doc["build"]["sdk"] = 0;
      doc["chip"]["model"] = 0;
      doc["chip"]["rev"] = 0;
      doc["chip"]["cores"] = 0;
      doc["chip"]["freq"] = 0;
      doc["flash"]["size"] = 0;
      doc["flash"]["realSize"] = 0;
      #endif
      
      doc.shrinkToFit();

      this->webServer->sendHeader(F("Content-Disposition"), F("attachment; filename=\"debug.json\""));
      this->bufferedWebServer->send(200, "application/json", doc, true);
    });


    // not found
    this->webServer->onNotFound([this]() {
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Page not found, uri: %s"), this->webServer->uri().c_str());

      const String uri = this->webServer->uri();
      if (uri.equals("/")) {
        this->webServer->send(200, "text/plain", F("The file system is not flashed!"));

      } else if (network->isApEnabled()) {
        this->onCaptivePortal();

      } else {
        this->webServer->send(404, "text/plain", F("Page not found"));
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
      ::delay(0);
      #endif

    } else if (this->stateWebServer() && !network->isApEnabled() && !network->isStaEnabled()) {
      this->stopWebServer();
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Stopped: AP and STA down"));

      #ifdef ARDUINO_ARCH_ESP8266
      ::delay(0);
      #endif
    }

    // dns server
    if (!this->stateDnsServer() && this->stateWebServer() && network->isApEnabled() && network->hasApClients() && millis() - this->dnsServerChangeState >= this->changeStateInterval) {
      this->startDnsServer();
      Log.straceln(FPSTR(L_PORTAL_DNSSERVER), F("Started: AP up"));
      
      #ifdef ARDUINO_ARCH_ESP8266
      ::delay(0);
      #endif

    } else if (this->stateDnsServer() && (!network->isApEnabled() || !this->stateWebServer())) {
      this->stopDnsServer();
      Log.straceln(FPSTR(L_PORTAL_DNSSERVER), F("Stopped: AP down"));

      #ifdef ARDUINO_ARCH_ESP8266
      ::delay(0);
      #endif
    }

    if (this->stateDnsServer()) {
      this->dnsServer->processNextRequest();
      #ifdef ARDUINO_ARCH_ESP8266
      ::delay(0);
      #endif
    }

    if (this->stateWebServer()) {
      this->webServer->handleClient();
    }

    if (!this->stateDnsServer() && !this->stateWebServer()) {
      this->delay(250);
    }
  }

  bool isAuthRequired() {
    return !network->isApEnabled() && settings.portal.auth && strlen(settings.portal.password);
  }

  void onCaptivePortal() {
    const String uri = this->webServer->uri();

    if (uri.equals("/connecttest.txt")) {
      this->webServer->sendHeader(F("Location"), F("http://logout.net"));
      this->webServer->send(302);

      Log.straceln(FPSTR(L_PORTAL_CAPTIVE), F("Redirect to http://logout.net with 302 code"));

    } else if (uri.equals("/wpad.dat")) {
      this->webServer->send(404);

      Log.straceln(FPSTR(L_PORTAL_CAPTIVE), F("Send empty page with 404 code"));

    } else if (uri.equals("/success.txt")) {
      this->webServer->send(200);

      Log.straceln(FPSTR(L_PORTAL_CAPTIVE), F("Send empty page with 200 code"));

    } else {
      String portalUrl = "http://" + network->getApIp().toString() + '/';

      this->webServer->sendHeader("Location", portalUrl.c_str());
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