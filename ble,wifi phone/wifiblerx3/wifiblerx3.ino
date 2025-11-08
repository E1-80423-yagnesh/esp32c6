#include <WiFi.h>
#include <WiFiUdp.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

const char *ssid = "moto";
const char *password = "12345678";

const int udpPort = 4210;
WiFiUDP udp;

// BLE
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLERemoteCharacteristic *pRemoteCharacteristic = nullptr;
BLEClient *pClient = nullptr;
bool bleConnected = false;

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to WiFi!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    udp.begin(udpPort);
  } else {
    Serial.println("\n❌ Failed to connect to WiFi.");
  }
}

void bleNotifyCallback(BLERemoteCharacteristic* pRemoteCharacteristic,
                       uint8_t* pData, size_t length, bool isNotify) {
  String msg;
  for (size_t i = 0; i < length; i++) msg += (char)pData[i];
  Serial.print("BLE Received: ");
  Serial.println(msg);
}

void connectToBLE() {
  Serial.println("Starting BLE scan...");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  BLEScanResults* results = pScan->start(5, false);


for (int i = 0; i < results->getCount(); i++) {
  BLEAdvertisedDevice device = results->getDevice(i);
    if (device.haveServiceUUID() && device.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial.print("Found TX: ");
      Serial.println(device.getName().c_str());

      pClient = BLEDevice::createClient();
      pClient->connect(&device);

      BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
      if (pRemoteService) {
        pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
        if (pRemoteCharacteristic) {
          pRemoteCharacteristic->registerForNotify(bleNotifyCallback);
          Serial.println("✅ Subscribed to BLE notifications!");
          bleConnected = true;
        }
      }
      break;
    }
  }
  if (!bleConnected) Serial.println("❌ No TX device found.");
}

void setupBLE() {
  BLEDevice::init("ESP32C6_RX");
  connectToBLE();
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
  setupBLE();
}

void loop() {
  // Wi-Fi receive
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buffer[255];
    int len = udp.read(buffer, 255);
    if (len > 0) buffer[len] = '\0';
    Serial.print("WiFi Received: ");
    Serial.println(buffer);
  }

  // Reconnect BLE if disconnected
  if (pClient && !pClient->isConnected()) {
    bleConnected = false;
    Serial.println("BLE disconnected — retrying...");
    delay(1000);
    connectToBLE();
  }

  delay(100);
}
