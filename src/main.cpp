#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const int relayPin = 5;
uint8_t relayState = HIGH;
TaskHandle_t toggleRelayHandle = NULL;

const char* configPath = "/config.json";

JsonDocument jsonDoc;

class Config {
  public:
    bool is_filled;
    String device_name;
    String admin_username;
    String admin_pass;
    String wifi_ssid;
    String wifi_pass;
    String ap_ssid;
    String ap_pass;
};

Config config;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool readConfig(fs::FS &fs, const char* path) {
  Serial.printf("Reading config file in: %s\n", path);

  File file = fs.open(path, "r");

  if (!file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  DeserializationError error = deserializeJson(jsonDoc, file);

  if (error) {
    Serial.print("Error: failed to parse json ");
    Serial.println(error.f_str());
    return false;
  }

  if (!jsonDoc["is_filled"].as<bool>()){
    Serial.println("Config file is empty");

    config.ap_ssid        = jsonDoc["ap_ssid"].as<String>();
    config.ap_pass        = jsonDoc["ap_pass"].as<String>();

    return false;

  } else {
    config.is_filled      = jsonDoc["is_filled"].as<bool>();
    config.device_name    = jsonDoc["device_name"].as<String>();
    config.admin_username = jsonDoc["admin_username"].as<String>();
    config.admin_pass     = jsonDoc["admin_pass"].as<String>();
    config.wifi_ssid      = jsonDoc["wifi_ssid"].as<String>();
    config.wifi_pass      = jsonDoc["wifi_pass"].as<String>();
    config.ap_ssid        = jsonDoc["ap_ssid"].as<String>();
    config.ap_pass        = jsonDoc["ap_pass"].as<String>();

    Serial.println("Success: config file loaded");
    return true;
  }
}

bool saveConfig(fs::FS &fs, const char* path, const Config &data) {
  Serial.printf("Saving config file in: %s\n", path);

  File file = fs.open(path, "w");

  if (!file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  jsonDoc["is_filled"]      = data.is_filled;
  jsonDoc["device_name"]    = data.device_name;
  jsonDoc["admin_username"] = data.admin_username;
  jsonDoc["admin_pass"]     = data.admin_pass;
  jsonDoc["wifi_ssid"]      = data.wifi_ssid;
  jsonDoc["wifi_pass"]      = data.wifi_pass;
  jsonDoc["ap_ssid"]        = data.ap_ssid;
  jsonDoc["ap_pass"]        = data.ap_pass;

  if (serializeJson(jsonDoc, file) == 0) {
    Serial.println("Error: failed to serialize json");
    file.close();
    return false;
  }

  Serial.println("Success: config file saved");
  file.close();
  return true;
}

bool updateConfig(fs::FS &fs, const char* path, const char* key, const char* value) {
  Serial.printf("Updating config file in %s\n", path);

  File file = fs.open(path, "w");

  if (!file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  jsonDoc[key] = value;

  if (serializeJson(jsonDoc, file) == 0) {
    Serial.println("Error: failed to serialize json");
    file.close();
    return false;
  }

  Serial.println("Success: config file updated");
  file.close();
  return true;
}

bool resetConfig(fs::FS &fs, const char* path) {
  Serial.printf("Resetting config file in: %s\n", path);

  File file = fs.open(path, "w");

  if (!file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  jsonDoc["is_filled"]      = false;
  jsonDoc["device_name"]    = "";
  jsonDoc["admin_username"] = "";
  jsonDoc["admin_pass"]     = "";
  jsonDoc["wifi_ssid"]      = "";
  jsonDoc["wifi_pass"]      = "";
  jsonDoc["ap_ssid"]        = "ESP-32-GATE";
  jsonDoc["ap_pass"]        = "";

  if (serializeJson(jsonDoc, file) == 0) {
    Serial.println("Error: failed to serialize json");
    file.close();
    return false;
  }

  Serial.println("Success: config file reset");
  file.close();
  return true;
}

void initNetwork() {
  WiFi.mode(WIFI_AP_STA);

  if (config.wifi_ssid != "" && config.wifi_pass != "") {
    WiFi.begin(config.wifi_ssid, config.wifi_pass);
    Serial.print("Connected to : ");
    Serial.println(config.wifi_ssid);
  }

  WiFi.softAP(config.ap_ssid, config.ap_pass);
  Serial.print("Device access point : ");
  Serial.println(WiFi.softAPIP());
}

void rebootTask(void *param) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP.restart();
  vTaskDelete(NULL);
}

void initNetworkTask(void *param) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  initNetwork();
  vTaskDelete(NULL);
}

void toogleRelayTask(void *param) {
  relayState = LOW;
  digitalWrite(relayPin, relayState);
  ws.textAll("open");

  vTaskDelay(pdMS_TO_TICKS(5000));
    
  relayState = HIGH;
  digitalWrite(relayPin, relayState);
  ws.textAll("close");

  toggleRelayHandle = NULL;
  vTaskDelete(NULL);
}

void handleWSEvent(uint8_t &relayState) {
  if (toggleRelayHandle != NULL) {
    Serial.println("Error: same task is currently being executed, please wait");
    return;
  }
  if (relayState == LOW) {
    Serial.println("Error: relay is in open state, please wait");
    return;
  }
  if (xTaskCreate(toogleRelayTask, "toogleRelayTask", 2048, NULL, 1, NULL) != pdPASS) {
    Serial.println("Error: xTaskCreate toogleRelayTask failed");
    toggleRelayHandle = NULL;
    return;
  }
}

void onWSEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *args, uint8_t *data, size_t len) {
  switch(type) {
    case WS_EVT_CONNECT: {
      Serial.printf("New Connection: WebSocket client %s\n", client->remoteIP().toString().c_str());
      break;
    }
    case WS_EVT_DISCONNECT: {
      Serial.printf("Closed Connection: WebSocket client %s\n", client->remoteIP().toString().c_str());
      break;
    }
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)args;
      if (info->final && info->index == 0 && info->len && info->opcode == WS_TEXT) {
        data[len] = 0;
        if (strcmp((char*)data, "open") == 0) {
          handleWSEvent(relayState);
        }
      }
      break;
    }
    case WS_EVT_ERROR: {
      Serial.printf("Connection Error: WebSocket client %s\n", client->remoteIP().toString().c_str());
      break;
    }
  }
}

String processor(const String &var) {
  if (var == "DEVICE_NAME") {
    return config.device_name;
  }
  if (var == "WIFI_SSID") {
    return config.wifi_ssid;
  }
  if (var == "WIFI_PASS") {
    return config.wifi_pass;
  }
  if (var == "AP_SSID")  {
    return config.ap_ssid;
  }
  if (var == "AP_PASS") {
    return config.ap_pass;
  }

  return String();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayState);

  // WebSocket
  ws.onEvent(onWSEvent);
  server.addHandler(&ws);

  Serial.println("Starting....");

  if (!LittleFS.begin(true)) {
    Serial.println("Error: failed to mount LittleFS");
  }

  bool isFilled = readConfig(LittleFS, configPath);

  initNetwork();

  if (!isFilled) {
    // GET Request
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/initialconfig.html", "text/html", false);
    });

    // POST Request
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      config.is_filled       = true;
      config.device_name     = request->getParam("device_name", true)->value();
      config.admin_username  = request->getParam("admin_username", true)->value();
      config.admin_pass      = request->getParam("admin_pass", true)->value();
      config.wifi_ssid       = request->getParam("wifi_ssid", true)->value();
      config.wifi_pass       = request->getParam("wifi_pass", true)->value();
      config.ap_ssid         = request->getParam("ap_ssid", true)->value();
      config.ap_pass         = request->getParam("ap_pass", true)->value();

      if (!saveConfig(LittleFS, configPath, config)) {
        request->send(400, "text/plain", "Error: please check ESP log");
        return;
      }

      request->send(201, "text/plain", "Success");

      if (xTaskCreate(rebootTask, "rebootTask", 2048, NULL, 1, NULL) != pdPASS) {
        Serial.println("Error: xTaskCreate rebootTask failed");
      }
    });

  } else {
    // GET request
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/gate.html", "text/html", false);
    });

    server.on("/management", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/management.html", "text/html", false);
    });

    server.on("/accesspoint", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/accesspoint.html", "text/html", false);  
    });

    server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/wifi.html", "text/html", false);
    });

    server.on("/credential", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/credential.html", "text/html", false);
    });

    server.on("/device", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/device.html", "text/html", false);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/page/status.html", "text/html", false, processor);
    });

    // POST request
    server.on("/accesspoint", HTTP_POST, [](AsyncWebServerRequest *request) {
      config.ap_ssid  = request->getParam("ap_ssid", true)->value();
      config.ap_pass  = request->getParam("ap_pass", true)->value();

      if (!saveConfig(LittleFS, configPath, config)) {
        request->send(400, "text/plain", "Error: please check ESP log");
        return;
      }

      request->send(201, "text/plain", "Success");

      if (xTaskCreate(initNetworkTask, "initNetworkTask", 4056, NULL, 1, NULL) != pdPASS) {
        Serial.println("Error: xTaskCreate initNetworkTask failed");
      }
    }); 

    server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request){
      config.wifi_ssid = request->getParam("wifi_ssid", true)->value();
      config.wifi_pass = request->getParam("wifi_pass", true)->value();

      if (!saveConfig(LittleFS, configPath, config)) {
        request->send(400, "text/plain", "Error: please check ESP log");
        return;
      }

      request->send(201, "text/plain", "Success");

      if (xTaskCreate(initNetworkTask, "initNetworkTask", 4056, NULL, 1, NULL) != pdPASS) {
        Serial.println("Error: xTaskCreate initNetworkTask failed");
      }
    });

    server.on("/credential", HTTP_POST, [](AsyncWebServerRequest *request) {
      config.admin_username = request->getParam("admin_username", true)->value();
      config.admin_pass     = request->getParam("admin_pass", true)->value();

      if (!saveConfig(LittleFS, configPath, config)) {
        request->send(400, "text/plain", "Error: please check ESP log");
        return;
      }

      request->send(200, "text/plain", "Success");
    });

    server.on("/device", HTTP_POST, [](AsyncWebServerRequest *request) {
      config.device_name = request->getParam("device_name", true)->value();

      if (!saveConfig(LittleFS, configPath, config)) {
        request->send(400, "text/palin", "Error: please check ESP log");
        return;
      }

      request->send(200, "text/plain", "Success");
    });

    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "Success: Rebooting");

      Serial.println("Rebooting....");

      if (xTaskCreate(rebootTask, "rebootTask", 2048, NULL, 1, NULL) != pdPASS) {
        Serial.println("Error: xTaskCreate rebootTask failed");
      }
    });

    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
      if (!resetConfig(LittleFS, configPath)) {
        request->send(400, "text/plain", "Error: please check ESP log");
        return;
      }

      request->send(200, "text/plain", "Success: Rebooting");

      if (xTaskCreate(rebootTask, "rebootTask", 2048, NULL, 1, NULL) != pdPASS) {
        Serial.println("Error: xTaskCreate rebootTask failed");
      }
    });
  }

  server.serveStatic("/assets/css/style.css", LittleFS, "/assets/css/style.css")
    .setCacheControl("no-cache, no-store, must-revalidate")
    .setTryGzipFirst(false);
  server.serveStatic("/assets/js/", LittleFS, "/assets/js/")
    .setTryGzipFirst(false);

  server.begin();
}

void loop() {
  ws.cleanupClients();
}