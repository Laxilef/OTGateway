#define PORTAL_CACHE_TIME "max-age=86400"
#define PORTAL_CACHE settings.system.debug ? nullptr : PORTAL_CACHE_TIME
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

extern NetworkTask* tNetwork;
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
    if (this->bufferedWebServer != nullptr) {
      delete this->bufferedWebServer;
    }

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
  const unsigned int changeStateInterval = 1000;

  WebServer* webServer = nullptr;
  BufferedWebServer* bufferedWebServer = nullptr;
  DNSServer* dnsServer = nullptr;

  bool webServerEnabled = false;
  bool dnsServerEnabled = false;
  unsigned long webServerChangeState = 0;
  unsigned long dnsServerChangeState = 0;

  const char* getTaskName() {
    return "Portal";
  }

  /*int getTaskCore() {
    return 1;
  }*/

  int getTaskPriority() {
    return 0;
  }

  void setup() {
    this->dnsServer->setTTL(0);
    this->dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    #ifdef ARDUINO_ARCH_ESP8266
    this->webServer->enableETag(true);
    #endif

    // index page
    /*auto indexPage = (new DynamicPage("/", &LittleFS, "/index.html"))
      ->setTemplateFunction([](const char* var) -> String {
        String result;

        if (strcmp(var, "ver") == 0) {
          result = PROJECT_VERSION;
        }

        return result;
      });
    this->webServer->addHandler(indexPage);*/
    this->webServer->addHandler(new StaticPage("/", &LittleFS, "/index.html", PORTAL_CACHE));

    // restart
    this->webServer->on("/restart.html", HTTP_GET, [this]() {
      if (this->isNeedAuth()) {
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
    auto networkPage = (new StaticPage("/network.html", &LittleFS, "/network.html", PORTAL_CACHE))
      ->setBeforeSendFunction([this]() {
        if (this->isNeedAuth() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->requestAuthentication(DIGEST_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(networkPage);

    // settings page
    auto settingsPage = (new StaticPage("/settings.html", &LittleFS, "/settings.html", PORTAL_CACHE))
      ->setBeforeSendFunction([this]() {
        if (this->isNeedAuth() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->requestAuthentication(DIGEST_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(settingsPage);

    // upgrade page
    auto upgradePage = (new StaticPage("/upgrade.html", &LittleFS, "/upgrade.html", PORTAL_CACHE))
      ->setBeforeSendFunction([this]() {
        if (this->isNeedAuth() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->requestAuthentication(DIGEST_AUTH);
          return false;
        }

        return true;
      });
    this->webServer->addHandler(upgradePage);

    // OTA
    auto upgradeHandler = (new UpgradeHandler("/api/upgrade"))->setCanUploadFunction([this](const String& uri) {
      if (this->isNeedAuth() && !this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
        this->webServer->sendHeader("Connection", "close");
        this->webServer->send(401);
        return false;
      }

      return true;
    })->setBeforeUpgradeFunction([](UpgradeHandler::UpgradeType type) -> bool {
      return true;
    })->setAfterUpgradeFunction([this](const UpgradeHandler::UpgradeResult& fwResult, const UpgradeHandler::UpgradeResult& fsResult) {
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
      if (this->isNeedAuth()) {
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
      if (this->isNeedAuth()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      String plain = this->webServer->arg(0);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/backup/restore %d bytes: %s"), plain.length(), plain.c_str());

      if (plain.length() < 5) {
        this->webServer->send(406);
        return;

      } else if (plain.length() > 2048) {
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
        tNetwork->setStaCredentials(networkSettings.sta.ssid, networkSettings.sta.password, networkSettings.sta.channel);
        tNetwork->setUseDhcp(networkSettings.useDhcp);
        tNetwork->setStaticConfig(
          networkSettings.staticConfig.ip,
          networkSettings.staticConfig.gateway,
          networkSettings.staticConfig.subnet,
          networkSettings.staticConfig.dns
        );
        tNetwork->reconnect();
        changed = true;
      }

      doc.clear();
      doc.shrinkToFit();

      this->webServer->send(changed ? 201 : 200);
    });

    // network
    this->webServer->on("/api/network/settings", HTTP_GET, [this]() {
      if (this->isNeedAuth()) {
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
      if (this->isNeedAuth()) {
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

      if (changed) {
        this->webServer->send(201);

        fsNetworkSettings.update();
        tNetwork->setStaCredentials(networkSettings.sta.ssid, networkSettings.sta.password, networkSettings.sta.channel);
        tNetwork->setUseDhcp(networkSettings.useDhcp);
        tNetwork->setStaticConfig(
          networkSettings.staticConfig.ip,
          networkSettings.staticConfig.gateway,
          networkSettings.staticConfig.subnet,
          networkSettings.staticConfig.dns
        );
        tNetwork->reconnect();

      } else {
        this->webServer->send(200);
      }
    });

    this->webServer->on("/api/network/status", HTTP_GET, [this]() {
      bool isConnected = tNetwork->isConnected();

      JsonDocument doc;
      doc["hostname"] = networkSettings.hostname;
      doc["mac"] = tNetwork->getStaMac();
      doc["isConnected"] = isConnected;
      doc["ssid"] = tNetwork->getStaSsid();
      doc["signalQuality"] = isConnected ? NetworkTask::rssiToSignalQuality(tNetwork->getRssi()) : 0;
      doc["channel"] = isConnected ? tNetwork->getStaChannel() : 0;
      doc["ip"] = isConnected ? tNetwork->getStaIp().toString() : "";
      doc["subnet"] = isConnected ? tNetwork->getStaSubnet().toString() : "";
      doc["gateway"] = isConnected ? tNetwork->getStaGateway().toString() : "";
      doc["dns"] = isConnected ? tNetwork->getStaDns().toString() : "";
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, "application/json", doc);
    });

    this->webServer->on("/api/network/scan", HTTP_GET, [this]() {
      if (this->isNeedAuth()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          this->webServer->send(401);
          return;
        }
      }

      auto apCount = WiFi.scanComplete();
      if (apCount <= 0) {
        WiFi.scanNetworks(true, true);
        
        if (apCount == WIFI_SCAN_RUNNING || apCount == WIFI_SCAN_FAILED) {
          this->webServer->send(202);

        } else if (apCount == 0) {
          this->webServer->send(200, "application/json", "[]");

        } else {
          this->webServer->send(500);
        }

        
        return;
      }
      
      JsonDocument doc;
      for (short int i = 0; i < apCount; i++) {
        String ssid = WiFi.SSID(i);
        doc[i]["ssid"] = ssid;
        doc[i]["signalQuality"] = NetworkTask::rssiToSignalQuality(WiFi.RSSI(i));
        doc[i]["channel"] = WiFi.channel(i);
        doc[i]["hidden"] = !ssid.length();
        doc[i]["encryptionType"] = WiFi.encryptionType(i);
      }
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, "application/json", doc);

      WiFi.scanNetworks(true, true);
    });


    // settings
    this->webServer->on("/api/settings", HTTP_GET, [this]() {
      if (this->isNeedAuth()) {
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
      if (this->isNeedAuth()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      String plain = this->webServer->arg(0);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/settings %d bytes: %s"), plain.length(), plain.c_str());

      if (plain.length() < 5) {
        this->webServer->send(406);
        return;

      } else if (plain.length() > 2048) {
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

      if (changed) {
        fsSettings.update();
        this->webServer->send(201);

      } else {
        this->webServer->send(200);
      }
    });


    // vars
    this->webServer->on("/api/vars", HTTP_GET, [this]() {
      JsonDocument doc;
      varsToJson(vars, doc);

      doc["system"]["version"] = PROJECT_VERSION;
      doc["system"]["buildDate"] = __DATE__ " " __TIME__;
      doc["system"]["uptime"] = millis() / 1000ul;
      doc["system"]["freeHeap"] = getFreeHeap();
      doc["system"]["totalHeap"] = getTotalHeap();
      doc["system"]["maxFreeBlockHeap"] = getMaxFreeBlockHeap();
      doc["system"]["resetReason"] = getResetReason();
      doc["system"]["mqttConnected"] = tMqtt->isConnected();
      doc.shrinkToFit();

      this->bufferedWebServer->send(200, "application/json", doc);
    });

    this->webServer->on("/api/vars", HTTP_POST, [this]() {
      if (this->isNeedAuth()) {
        if (!this->webServer->authenticate(settings.portal.login, settings.portal.password)) {
          return this->webServer->send(401);
        }
      }

      String plain = this->webServer->arg(0);
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Request /api/vars %d bytes: %s"), plain.length(), plain.c_str());

      if (plain.length() < 5) {
        this->webServer->send(406);
        return;

      } else if (plain.length() > 1024) {
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

      if (changed) {
        this->webServer->send(201);

      } else {
        this->webServer->send(200);
      }
    });


    // not found
    this->webServer->onNotFound([this]() {
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Page not found, uri: %s"), this->webServer->uri().c_str());

      const String uri = this->webServer->uri();
      if (uri.equals("/")) {
        this->webServer->send(200, "text/plain", F("The file system is not flashed!"));

      } else if (tNetwork->isApEnabled()) {
        this->onCaptivePortal();

      } else {
        this->webServer->send(404, "text/plain", F("Page not found"));
      }
    });

    this->webServer->serveStatic("/favicon.ico", LittleFS, "/static/favicon.ico", PORTAL_CACHE);
    this->webServer->serveStatic("/static", LittleFS, "/static", PORTAL_CACHE);
  }

  void loop() {
    // web server
    if (!this->stateWebServer()) {
      this->startWebServer();
      Log.straceln(FPSTR(L_PORTAL_WEBSERVER), F("Started"));

      #ifdef ARDUINO_ARCH_ESP8266
      ::esp_yield();
      #endif
    }

    // dns server
    if (!this->stateDnsServer() && this->stateWebServer() && tNetwork->isApEnabled() && tNetwork->hasApClients() && millis() - this->dnsServerChangeState >= this->changeStateInterval) {
      this->startDnsServer();
      Log.straceln(FPSTR(L_PORTAL_DNSSERVER), F("Started: AP up"));
      
      #ifdef ARDUINO_ARCH_ESP8266
      ::esp_yield();
      #endif

    } else if (this->stateDnsServer() && (!tNetwork->isApEnabled() || !this->stateWebServer()) && millis() - this->dnsServerChangeState >= this->changeStateInterval) {
      this->stopDnsServer();
      Log.straceln(FPSTR(L_PORTAL_DNSSERVER), F("Stopped: AP down"));

      #ifdef ARDUINO_ARCH_ESP8266
      ::esp_yield();
      #endif
    }

    if (this->stateDnsServer()) {
      this->dnsServer->processNextRequest();
      #ifdef ARDUINO_ARCH_ESP8266
      ::esp_yield();
      #endif
    }

    if (this->stateWebServer()) {
      this->webServer->handleClient();
    }
  }

  bool isNeedAuth() {
    return !tNetwork->isApEnabled() && settings.portal.useAuth && strlen(settings.portal.password);
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
      String portalUrl = "http://" + tNetwork->getApIp().toString() + '/';

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

    this->webServer->handleClient();
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

    this->dnsServer->start(53, "*", tNetwork->getApIp());
    this->dnsServerEnabled = true;
    this->dnsServerChangeState = millis();
  }

  void stopDnsServer() {
    if (!this->stateDnsServer()) {
      return;
    }

    this->dnsServer->processNextRequest();
    this->dnsServer->stop();
    this->dnsServerEnabled = false;
    this->dnsServerChangeState = millis();
  }
};