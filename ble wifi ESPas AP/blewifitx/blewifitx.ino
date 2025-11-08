#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs
#define SERVICE_UUID         "61b5ce72-86ce-4912-a966-61571ab22dcf"
#define CHARACTERISTIC_UUID  "26cb1570-3458-4abf-895a-8a0a283907d3"

// WiFi Configuration
const char* wifi_ssid = "ESP32C6_Coordinator";
const char* wifi_password = "12345678";
WiFiServer wifiServer(80);

// BLE Configuration
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool bleConnected = false;

// LED
#define LED_PIN 8

// BLE Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleConnected = true;
        Serial.println(" BLE Client Connected!");
        digitalWrite(LED_PIN, HIGH);
    }

    void onDisconnect(BLEServer* pServer) {
        bleConnected = false;
        Serial.println(" BLE Client Disconnected");
        digitalWrite(LED_PIN, LOW);
        pServer->startAdvertising();
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue().c_str();
        if (value.length() > 0) {
            Serial.print("BLE Received: ");
            Serial.println(value);
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== COORDINATOR TEST ===");
    
    // LED Setup
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // WiFi AP Setup
    Serial.println(" Starting WiFi AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifi_ssid, wifi_password);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print(" WiFi AP Started! IP: ");
    Serial.println(IP);
    Serial.println("   SSID: " + String(wifi_ssid));
    Serial.println("   Password: " + String(wifi_password));
    
    wifiServer.begin();
    Serial.println(" WiFi Server listening on port 80");
    
    // BLE Setup
    Serial.println("\n Starting BLE...");
    BLEDevice::init("ESP32_Coordinator");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    pCharacteristic->setValue("Ready");
    
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    
    Serial.println(" BLE Advertising as 'ESP32_Coordinator'");
    Serial.println("\n=== COORDINATOR READY ===\n");
}

void loop() {
    static unsigned long lastSend = 0;
    static int counter = 0;
    
    // Check for WiFi clients
    WiFiClient client = wifiServer.available();
    if (client) {
        Serial.println(" WiFi Client connected!");
        
        while (client.connected()) {
            if (client.available()) {
                String received = client.readStringUntil('\n');
                received.trim();
                Serial.print(" WiFi Received: ");
                Serial.println(received);
                
                // Echo back
                client.println("Coordinator received: " + received);
            }
            delay(10);
        }
        Serial.println(" WiFi Client disconnected");
    }
    
    // Send periodic messages every 5 seconds
    if (millis() - lastSend > 5000) {
        lastSend = millis();
        counter++;
        
        String message = "ABC" + String(counter);
        
        // Send via WiFi to any connected clients
        WiFiClient clients[5];
        int clientCount = 0;
        for (int i = 0; i < WiFi.softAPgetStationNum() && i < 5; i++) {
            clients[i] = wifiServer.available();
            if (clients[i]) clientCount++;
        }
        
        // Send via BLE
        if (bleConnected && pCharacteristic != NULL) {
            pCharacteristic->setValue(message.c_str());
            pCharacteristic->notify();
            Serial.println("📤 BLE Sent: " + message);
        }
        
        Serial.println("---");
        Serial.println("Status:");
        Serial.println("  WiFi Clients: " + String(WiFi.softAPgetStationNum()));
        Serial.println("  BLE Connected: " + String(bleConnected ? "YES" : "NO"));
        Serial.println("  Counter: " + String(counter));
    }
    
    delay(100);
}