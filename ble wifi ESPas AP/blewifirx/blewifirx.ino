#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>

// BLE UUIDs
#define SERVICE_UUID         "61b5ce72-86ce-4912-a966-61571ab22dcf"
#define CHARACTERISTIC_UUID  "26cb1570-3458-4abf-895a-8a0a283907d3"

// WiFi Configuration
const char* wifi_ssid = "ESP32C6_Coordinator";
const char* wifi_password = "12345678";

// WiFi Client
WiFiClient wifiClient;

// BLE Configuration
static BLEClient* pBLEClient = NULL;
static BLERemoteCharacteristic* pRemoteCharacteristic = NULL;
static bool bleConnected = false;
static bool doConnect = false;
static BLEAdvertisedDevice* myDevice = NULL;

// LED & Button
#define LED_PIN 2
#define BUTTON_PIN 9

// BLE Notify Callback
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                          uint8_t* pData, size_t length, bool isNotify) {
    String received = String((char*)pData).substring(0, length);
    Serial.print(" BLE Received: ");
    Serial.println(received);
    
    // Blink LED on receive
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
}

// BLE Callbacks
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        bleConnected = true;
        Serial.println(" BLE Connected to Coordinator!");
        digitalWrite(LED_PIN, HIGH);
    }

    void onDisconnect(BLEClient* pclient) {
        bleConnected = false;
        Serial.println("BLE Disconnected");
        digitalWrite(LED_PIN, LOW);
    }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveServiceUUID() && 
            advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            Serial.println("Found Coordinator BLE!");
        }
    }
};

bool connectToBLEServer() {
    if (myDevice == NULL) return false;
    
    Serial.println(" Connecting to BLE server...");
    pBLEClient = BLEDevice::createClient();
    pBLEClient->setClientCallbacks(new MyClientCallback());
    
    if (!pBLEClient->connect(myDevice)) {
        Serial.println("BLE connection failed");
        return false;
    }
    
    BLERemoteService* pRemoteService = pBLEClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("Service not found");
        pBLEClient->disconnect();
        return false;
    }
    
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Characteristic not found");
        pBLEClient->disconnect();
        return false;
    }
    
    if(pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }
    
    Serial.println(" BLE fully connected!");
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== END DEVICE TEST ===");
    
    // LED & Button Setup
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_PIN, LOW);
    
    // WiFi Setup
    Serial.println("Connecting to WiFi AP...");
    WiFi.begin(wifi_ssid, wifi_password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n WiFi Connected!");
        Serial.print("   IP: ");
        Serial.println(WiFi.localIP());
        
        if (wifiClient.connect(WiFi.gatewayIP(), 80)) {
            Serial.println(" Connected to Coordinator WiFi Server!");
            wifiClient.println("EndDevice_Connected");
        }
    } else {
        Serial.println("\n WiFi connection failed");
    }
    
    // BLE Setup
    Serial.println("\n Starting BLE scan...");
    BLEDevice::init("ESP32_EndDevice");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
    
    Serial.println("\n=== END DEVICE READY ===\n");
}

void loop() {
    static unsigned long lastSend = 0;
    static int counter = 0;
    static bool lastButtonState = HIGH;
    
    // Handle BLE connection
    if (doConnect && !bleConnected) {
        if (connectToBLEServer()) {
            Serial.println(" BLE setup complete");
        }
        doConnect = false;
    }
    
    if (!bleConnected && !doConnect) {
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->start(5, false);
    }
    
    // Check WiFi messages
    if (wifiClient.connected() && wifiClient.available()) {
        String received = wifiClient.readStringUntil('\n');
        received.trim();
        Serial.print(" WiFi Received: ");
        Serial.println(received);
    }
    
    // Reconnect WiFi if disconnected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi reconnecting...");
        WiFi.reconnect();
        delay(5000);
    }
    
    if (!wifiClient.connected()) {
        wifiClient.connect(WiFi.gatewayIP(), 80);
        delay(1000);
    }
    
    // Button press sends message
    bool currentButtonState = digitalRead(BUTTON_PIN);
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        counter++;
        String message = "Button" + String(counter);
        
        // Send via WiFi
        if (wifiClient.connected()) {
            wifiClient.println(message);
            Serial.println(" WiFi Sent: " + message);
            //Serial.println("hello wifi");
        }
        
        // Send via BLE
        if (bleConnected && pRemoteCharacteristic != NULL) {
            pRemoteCharacteristic->writeValue(message.c_str(), message.length());
            Serial.println(" BLE Sent: " + message);
            //Serial.println("hello ble");
        }
        
        delay(300); // Debounce
    }
    lastButtonState = currentButtonState;
    
    // Periodic status
    if (millis() - lastSend > 10000) {
        lastSend = millis();
        Serial.println("---");
        Serial.println("Status:");
        Serial.println("  WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
        Serial.println("  BLE: " + String(bleConnected ? "Connected" : "Disconnected"));
        Serial.println("  Counter: " + String(counter));
    }
    
    delay(100);
}

    