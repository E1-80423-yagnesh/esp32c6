#include <WiFi.h>
#include <WiFiUdp.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

const char *ssid = "moto";
const char *password = "12345678";

const int udpPort = 4210;
WiFiUDP udp;
String message = "Hello";
unsigned long lastSend = 0;

// BLE
BLEServer *pServer;
BLECharacteristic *pCharacteristic;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) { Serial.println("BLE connected!"); }
  void onDisconnect(BLEServer *pServer) {
    Serial.println("BLE disconnected, restarting advertising...");
    pServer->startAdvertising();
  }
};

void setupWiFi() {
  Serial.println("Starting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n Connected to WiFi!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    udp.begin(udpPort);
  } else {
    Serial.println("\n WiFi connection failed. Check SSID/password or 2.4 GHz.");
  }
}

void setupBLE() {
  Serial.println("Starting BLE...");
  BLEDevice::init("ESP32C6_TX");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->setValue("Ready");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->startAdvertising();
  Serial.println("BLE advertising started...");
}

void setup() {
  Serial.begin(115200);
  setupWiFi();   //  Wi-Fi first
  setupBLE();    // BLE second
}

void loop() {
  if (millis() - lastSend >= 3000) {
    lastSend = millis();

    // Wi-Fi broadcast
    udp.beginPacket(WiFi.broadcastIP(), udpPort);
    udp.print(message);
    udp.endPacket();
    Serial.println("WiFi Sent: " + message);

    // BLE send
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
    Serial.println("BLE Sent: " + message);
  }

  delay(100);
}
