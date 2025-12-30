#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const int RELAYPIN = 5;
uint8_t relay_state = HIGH;
TaskHandle_t toggle_relay_handle = NULL;

const char* CONFIG_PATH = "/config.json";

JsonDocument json_doc;

struct Config {
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

const char* hostname = "gate";

bool readConfig(fs::FS &fs, const char* PATH) {
  Serial.printf("Reading config file in: %s\n", PATH);

  File config_file = fs.open(PATH, "r");

  if (!config_file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  DeserializationError error = deserializeJson(json_doc, config_file);

  if (error) {
    Serial.print("Error: failed to parse json ");
    Serial.println(error.f_str());
    return false;
  }

  if (!json_doc["is_filled"].as<bool>()){
    config.ap_ssid        = json_doc["ap_ssid"].as<String>();
    config.ap_pass        = json_doc["ap_pass"].as<String>();

    Serial.println("Config file is empty");
    return false;
  } else {
    config.is_filled      = json_doc["is_filled"].as<bool>();
    config.device_name    = json_doc["device_name"].as<String>();
    config.admin_username = json_doc["admin_username"].as<String>();
    config.admin_pass     = json_doc["admin_pass"].as<String>();
    config.wifi_ssid      = json_doc["wifi_ssid"].as<String>();
    config.wifi_pass      = json_doc["wifi_pass"].as<String>();
    config.ap_ssid        = json_doc["ap_ssid"].as<String>();
    config.ap_pass        = json_doc["ap_pass"].as<String>();

    Serial.println("Success: config file loaded");
    return true;
  }
}

bool saveConfig(fs::FS &fs, const char* PATH) {
  Serial.printf("Saving config file in: %s\n", PATH);

  File config_file = fs.open(PATH, "w");

  if (!config_file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  json_doc["is_filled"]      = config.is_filled;
  json_doc["device_name"]    = config.device_name;
  json_doc["admin_username"] = config.admin_username;
  json_doc["admin_pass"]     = config.admin_pass;
  json_doc["wifi_ssid"]      = config.wifi_ssid;
  json_doc["wifi_pass"]      = config.wifi_pass;
  json_doc["ap_ssid"]        = config.ap_ssid;
  json_doc["ap_pass"]        = config.ap_pass;

  if (serializeJson(json_doc, config_file) == 0) {
    Serial.println("Error: failed to serialize json");
    config_file.close();
    return false;
  }

  Serial.println("Success: config file saved");
  config_file.close();
  return true;
}

bool updateConfig(fs::FS &fs, const char* PATH, const char* KEY, const char* VALUE) {
  Serial.printf("Updating config file in %s\n", PATH);

  File config_file = fs.open(PATH, "w");

  if (!config_file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  json_doc[KEY] = VALUE;

  if (serializeJson(json_doc, config_file) == 0) {
    Serial.println("Error: failed to serialize json");
    config_file.close();
    return false;
  }

  Serial.println("Success: config file updated");
  config_file.close();
  return true;
}

bool resetConfig(fs::FS &fs, const char* PATH) {
  Serial.printf("Resetting config file in: %s\n", PATH);

  File config_file = fs.open(PATH, "w");

  if (!config_file) {
    Serial.println("Error: failed to open config file");
    return false;
  }

  json_doc["is_filled"]      = false;
  json_doc["device_name"]    = "";
  json_doc["admin_username"] = "";
  json_doc["admin_pass"]     = "";
  json_doc["wifi_ssid"]      = "";
  json_doc["wifi_pass"]      = "";
  json_doc["ap_ssid"]        = "ESP-32-GATE";
  json_doc["ap_pass"]        = "";

  if (serializeJson(json_doc, config_file) == 0) {
    Serial.println("Error: failed to serialize json");
    config_file.close();
    return false;
  }

  Serial.println("Success: config file reset");
  config_file.close();
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
  relay_state = LOW;
  digitalWrite(RELAYPIN, relay_state);
  ws.textAll("open");

  vTaskDelay(pdMS_TO_TICKS(5000));
    
  relay_state = HIGH;
  digitalWrite(RELAYPIN, relay_state);
  ws.textAll("close");

  toggle_relay_handle = NULL;
  vTaskDelete(NULL);
}

void handleWSEvent(uint8_t &relay_state) {
  if (toggle_relay_handle != NULL) {
    Serial.println("Error: same task is currently being executed, please wait");
    return;
  }
  if (relay_state == LOW) {
    Serial.println("Error: relay is in open state, please wait");
    return;
  }
  if (xTaskCreate(toogleRelayTask, "toogleRelayTask", 2048, NULL, 1, NULL) != pdPASS) {
    Serial.println("Error: xTaskCreate toogleRelayTask failed");
    toggle_relay_handle = NULL;
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
          handleWSEvent(relay_state);
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
  
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, relay_state);

  // WebSocket
  ws.onEvent(onWSEvent);
  server.addHandler(&ws);

  Serial.println("Starting....");

  if (!LittleFS.begin(true)) {
    Serial.println("Error: failed to mount LittleFS");
  }

  bool isFilled = readConfig(LittleFS, CONFIG_PATH);

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

      if (!saveConfig(LittleFS, CONFIG_PATH)) {
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

      if (!saveConfig(LittleFS, CONFIG_PATH)) {
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

      if (!saveConfig(LittleFS, CONFIG_PATH)) {
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

      if (!saveConfig(LittleFS, CONFIG_PATH)) {
        request->send(400, "text/plain", "Error: please check ESP log");
        return;
      }

      request->send(200, "text/plain", "Success");
    });

    server.on("/device", HTTP_POST, [](AsyncWebServerRequest *request) {
      config.device_name = request->getParam("device_name", true)->value();

      if (!saveConfig(LittleFS, CONFIG_PATH)) {
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
      if (!resetConfig(LittleFS, CONFIG_PATH)) {
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