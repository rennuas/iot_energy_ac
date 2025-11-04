#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ==============================
// WiFi & MQTT CONFIG
// ==============================
const char* ssid       = "";
const char* password   = "*";

const char* mqttServer = ""; // perbaiki typo sebelumnya "locahost"
const uint16_t mqttPort = 8883;
const char* mqttUser   = "";
const char* mqttPass   = "";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// ==============================
// RELAY PINS
// ==============================
// Urutan pin: 15, 2, 4, 5, 18, 19, 21, 22, 23, 13, 12, 14, 27, 26, 25, 33, 32, 35, 34
const uint8_t relayPins[8] = { 
  2, 4, 5, 13, 14, 15, 18, 19
};

// ==============================
// Helpers
// ==============================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // Ambil payload sebagai string
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.println(msg);

  // Parsing JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.print("JSON Parsing failed: ");
    Serial.println(error.c_str());
    client.publish("iot_energy/status/ac", "{\"commandId\":0,\"status\":\"failed\"}");
    return;
  }

  String commandId = doc["commandId"];
  bool success = true;
  
  if(strcmp(topic, "iot_energy/trigger/ac") == 0){
    int turnOn   = doc["cmd"]["setTurnOn"];
    int turnOff  = doc["cmd"]["setTurnOff"];
    
    if (turnOn != 0) {
      if(turnOn-1 == 2 || turnOn-1 == 4){
        digitalWrite(relayPins[turnOn - 1], HIGH);
        digitalWrite(relayPins[3], HIGH);
      }else{
        digitalWrite(relayPins[turnOn - 1], HIGH);
      }
    }

    if (turnOff != 0) {
      if(turnOff-1 == 2){
        bool relayAlive = digitalRead(relayPins[4]) == HIGH;
        digitalWrite(relayPins[turnOff - 1], LOW);
        if(!relayAlive) digitalWrite(relayPins[3], LOW);
      }else if (turnOff-1 == 4 ){
        bool relayAlive = digitalRead(relayPins[2]) == HIGH;
        digitalWrite(relayPins[turnOff - 1], LOW);
        if(!relayAlive) digitalWrite(relayPins[3], LOW);
      }else{
        digitalWrite(relayPins[turnOff - 1], LOW);
      }
    }
  }

  if(strcmp(topic, "iot_energy/trigger/conveyor/ac") == 0){

    int turnOn   = doc["cmd"]["setTurnOn"];
    int turnOff   = doc["cmd"]["setTurnOff"];

    if(turnOn != 0){
        digitalWrite(relayPins[turnOn + 1], HIGH);
    }

    if(turnOff != 0){
       digitalWrite(relayPins[turnOn + 1], LOW);
    }
  }

  if(strcmp(topic, "iot_energy/trigger/ac/shutdown") == 0){
    for (size_t i = 0; i < 7; i++)
    {
      digitalWrite(relayPins[i], LOW);
    }
  }

  // Publish status
  StaticJsonDocument<200> response;
  response["commandId"] = commandId;
  response["status"] = success ? "success" : "failed";

  char buffer[200];
  serializeJson(response, buffer);

  client.publish("iot_energy/status/ac", buffer);

  Serial.println(success ? "AC ON success" : "AC failed");
}

// ==============================
// WiFi & MQTT Connect
// ==============================
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("WiFi connect");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());

  espClient.setInsecure();

}

void ensureMqtt() {
  if (client.connected()) return;

  Serial.printf("MQTT connect -> %s:%u ...\n", mqttServer, mqttPort);

  String clientId = "ESP32_AC_" + String((uint32_t)ESP.getEfuseMac(), HEX);

  client.connect(clientId.c_str(), mqttUser, mqttPass);

  if (!client.connected()) {
    Serial.println("[ERR] MQTT connect gagal");
    return;
  }

  client.subscribe("iot_energy/trigger/ac");
  client.subscribe("iot_energy/trigger/ac/shutdown");
  client.subscribe("iot_energy/trigger/conveyor/ac");

  Serial.println("MQTT connected & subscribed.");
}

// ==============================
// Setup & Loop
// ==============================
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("\n=== ESP32 MQTT AC Controller (JSON) ===");

  ensureWifi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);

  for (size_t i = 0; i < 8; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  
}

void loop() {
  ensureWifi();
  ensureMqtt();
  client.loop();
}
