#include <Arduino.h>
#include <NimBLEDevice.h>

#define BUTTON_PIN 17
#define BT_DEVICE_NAME "NimBLE-Arduino"

static NimBLEServer *pServer = nullptr;
static NimBLEAdvertising *pAdvertising = nullptr;
static NimBLEService *pFeedService = nullptr;
static std::list<NimBLEUUID> advertisingUUIDs;

class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
    {
        Serial.print("Client connected addr: ");
        Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    }

    void onDisconnect(NimBLEServer *pServer)
    {
        Serial.println("Client disconnected - start advertising");

        if (pAdvertising != nullptr && !pAdvertising->isAdvertising()) {
            pAdvertising->start();
        }
    }

    // uint32_t onPassKeyRequest(){
    //     Serial.println("Server Passkey Request");
    //     return 123456;
    // }

    // bool onConfirmPIN(uint32_t pass_key){
    //     Serial.print("The passkey YES/NO number: ");Serial.println(pass_key);
    //     /** Return false if passkeys don't match. */
    //     return true;
    // }

    // void onAuthenticationComplete(ble_gap_conn_desc* desc){
    //     /** Check that encryption was successful, if not we disconnect the client */
    //     if(!desc->sec_state.encrypted) {
    //         NimBLEDevice::getServer()->disconnect(desc->conn_handle);
    //         Serial.println("Encrypt connection failed - disconnecting client");
    //         return;
    //     }
    //     Serial.println("Starting BLE work!");
    // }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
    void onRead(NimBLECharacteristic *pCharacteristic)
    {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onRead(), value: ");
        Serial.println(pCharacteristic->getValue().c_str());
    }

    void onWrite(NimBLECharacteristic *pCharacteristic)
    {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onWrite(), value: ");
        Serial.println(pCharacteristic->getValue().c_str());
    }
    
    void onNotify(NimBLECharacteristic *pCharacteristic)
    {
        Serial.println("Sending notification to clients");
    }

    void onStatus(NimBLECharacteristic *pCharacteristic, Status status, int code)
    {
        String str = ("Notification/Indication status code: ");
        str += status;
        str += ", return code: ";
        str += code;
        str += ", ";
        str += NimBLEUtils::returnCodeToString(code);
        Serial.println(str);
    }

    void onSubscribe(NimBLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc, uint16_t subValue)
    {
        String str = "Client ID: ";
        str += desc->conn_handle;
        str += " Address: ";
        str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
        if (subValue == 0)
        {
            str += " Unsubscribed to ";
        }
        else if (subValue == 1)
        {
            str += " Subscribed to notfications for ";
        }
        else if (subValue == 2)
        {
            str += " Subscribed to indications for ";
        }
        else if (subValue == 3)
        {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID()).c_str();

        Serial.println(str);
    }
};

class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
    void onWrite(NimBLEDescriptor *pDescriptor)
    {
        std::string dscVal((char *)pDescriptor->getValue(), pDescriptor->getLength());
        Serial.print("Descriptor witten value:");
        Serial.println(dscVal.c_str());
    }

    void onRead(NimBLEDescriptor *pDescriptor)
    {
        Serial.print(pDescriptor->getUUID().toString().c_str());
        Serial.println(" Descriptor read");
    }
};

/** Define callback instances globally to use for multiple Charateristics \ Descriptors */
static DescriptorCallbacks dscCallbacks;
static CharacteristicCallbacks chrCallbacks;

bool tryAddAdvertisingUuid(NimBLEUUID uuid) {
    if (std::find(advertisingUUIDs.begin(), advertisingUUIDs.end(), uuid) == advertisingUUIDs.end()) {
        advertisingUUIDs.push_back(uuid);
        pAdvertising->addServiceUUID(uuid);
        return true;
    }
    return false;
}

void tryStartService()
{
    if (pServer == nullptr || pFeedService != nullptr) return;

    Serial.println("\n=================\nButton pressed, starting additional service\n");

    pFeedService = pServer->createService("FEED");
    NimBLECharacteristic *pBeefCharacteristic = pFeedService->createCharacteristic(
        "BEEF",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY);
    pBeefCharacteristic->setValue("YUM");
    pBeefCharacteristic->setCallbacks(&chrCallbacks);
    pFeedService->start();

    if (pAdvertising != nullptr)
    {
        tryAddAdvertisingUuid(pFeedService->getUUID());
        
        if (!pAdvertising->isAdvertising())
        {
            pAdvertising->start();
        }
    }

    Serial.println("\nService started and advertising restarted\n=================\n");
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting NimBLE Server");
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    NimBLEDevice::init(BT_DEVICE_NAME);
    NimBLEDevice::setSecurityAuth(false, false, true);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName(BT_DEVICE_NAME);
    pServer->advertiseOnDisconnect(false);

    NimBLEService *pDeadService = pServer->createService("DEAD");
    NimBLECharacteristic *pBeefCharacteristic = pDeadService->createCharacteristic(
        "BEEF",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        /** Require a secure connection for read and write access */
        NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
        NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
    );

    pBeefCharacteristic->setValue("Burger");
    pBeefCharacteristic->setCallbacks(&chrCallbacks);

    NimBLE2904 *pBeef2904 = (NimBLE2904 *)pBeefCharacteristic->createDescriptor("2904");
    pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
    pBeef2904->setCallbacks(&dscCallbacks);

    NimBLEService *pBaadService = pServer->createService("BAAD");
    NimBLECharacteristic *pFoodCharacteristic = pBaadService->createCharacteristic(
        "F00D",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY);

    pFoodCharacteristic->setValue("Fries");
    pFoodCharacteristic->setCallbacks(&chrCallbacks);

    NimBLEDescriptor *pC01Ddsc = pFoodCharacteristic->createDescriptor(
        "C01D",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
        20);
    pC01Ddsc->setValue("Send it back!");
    pC01Ddsc->setCallbacks(&dscCallbacks);

    pDeadService->start();
    pBaadService->start();

    // button pressed during startup, so start service now
    if (digitalRead(BUTTON_PIN) == LOW)
    {
        tryStartService();
    }

    tryAddAdvertisingUuid(pDeadService->getUUID());
    tryAddAdvertisingUuid(pBaadService->getUUID());
    pAdvertising->setScanResponse(true);
    if (!pAdvertising->isAdvertising())
    {
        pAdvertising->start();
    }

    Serial.println("Advertising Started");
}

void loop()
{
    delay(500);

    if (pFeedService == nullptr && digitalRead(BUTTON_PIN) == LOW)
    {
        tryStartService();
    }
}
