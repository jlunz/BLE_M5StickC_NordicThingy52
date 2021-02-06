/** NimBLE_Server Demo:
 *
 *  Demonstrates many of the available features of the NimBLE client library.
 *
 *  Created: on March 24 2020
 *      Author: H2zero
 *
*/

#include <M5StickC.h>
#include <NimBLEDevice.h>

void scanEndedCB(NimBLEScanResults results);

static NimBLEAdvertisedDevice* advDevice;

static bool doConnect = false;
static uint32_t scanTime = 0; /** 0 = scan forever */

static uint32_t reconnect_count = 0;
static uint32_t notify_count = 0;


/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        Serial.println("Connected");
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
        pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
        reconnect_count++;
        M5.Lcd.drawNumber(reconnect_count, 50, 0, 2);

        Serial.print(pClient->getPeerAddress().toString().c_str());
        Serial.println(" Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    };

    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if(params->itvl_min < 24) { /** 1.25ms units */
            return false;
        } else if(params->itvl_max > 40) { /** 1.25ms units */
            return false;
        } else if(params->latency > 2) { /** Number of intervals allowed to skip */
            return false;
        } else if(params->supervision_timeout > 100) { /** 10ms units */
            return false;
        }

        return true;
    };

    /********************* Security handled here **********************
    ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest(){
        Serial.println("Client Passkey Request");
        /** return the passkey to send to the server */
        return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key){
        Serial.print("The passkey YES/NO number: ");
        Serial.println(pass_key);
    /** Return false if passkeys don't match. */
        return true;
    };

    /** Pairing process complete, we can check the results in ble_gap_conn_desc */
    void onAuthenticationComplete(ble_gap_conn_desc* desc){
        if(!desc->sec_state.encrypted) {
            Serial.println("Encrypt connection failed - disconnecting");
            /** Find the client with the connection handle provided in desc */
            NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
            return;
        }
    };
};


/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.print("Advertised Device found: ");
        Serial.println(advertisedDevice->toString().c_str());
        if(advertisedDevice->isAdvertisingService(NimBLEUUID("ef680100-9b35-4933-9b10-52ffa9740042")))
        {
            Serial.println("Found Our Service");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            advDevice = advertisedDevice;
            /** Ready to connect now */
            doConnect = true;
        }
    };
};


/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    digitalWrite(GPIO_NUM_10, LOW);
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    str += ", Value = " + std::string((char*)pData, length);
    Serial.println(str.c_str());
    if(std::string(pRemoteCharacteristic->getUUID()) == "ef680201-9b35-4933-9b10-52ffa9740042")
    {
        Serial.println("---");
        for (size_t i = 0; i < length; ++i)
        {
            Serial.print(pData[i]);
        }
        Serial.println("---");

        int8_t integer = pData[0];
        uint8_t decimal = pData[1];

        Serial.print(integer);
        Serial.println(decimal);

        char buff[100];
        snprintf(buff, sizeof(buff), "Temp: %i.%i°C", integer, decimal);
        std::string temp = buff;
        M5.Lcd.drawString(temp.c_str(), 0, 29, 2);
    }
    if(std::string(pRemoteCharacteristic->getUUID()) == "ef680204-9b35-4933-9b10-52ffa9740042")
    {
        Serial.println("---");
        for (size_t i = 0; i < length; ++i)
        {
            Serial.print(pData[i]);
        }
        Serial.println("---");

        uint16_t eCO2_ppm = (pData[1] << 8) | pData[0];
        uint16_t TVOC_ppb = (pData[3] << 8) | pData[2];

        char buff[100];
        snprintf(buff, sizeof(buff), "CO2: %4u", eCO2_ppm);
        std::string co2 = buff;

        M5.Lcd.drawString(co2.c_str(), 0, 49, 2);
        if(eCO2_ppm <= 699)
            M5.Lcd.fillCircle(40, 100, 20, TFT_GREEN);
        if(eCO2_ppm > 699 && eCO2_ppm < 1099)
            M5.Lcd.fillCircle(40, 100, 20, TFT_GREENYELLOW);
        if(eCO2_ppm > 1099 && eCO2_ppm < 1500)
            M5.Lcd.fillCircle(40, 100, 20, TFT_ORANGE);


    }
    notify_count++;
    M5.Lcd.drawNumber(notify_count, 0, 140, 2);
    digitalWrite(GPIO_NUM_10, HIGH);
}

/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results){
    Serial.println("Scan Ended");
}


/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks clientCB;


/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if(NimBLEDevice::getClientListSize()) {
        /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if(pClient){
            if(!pClient->connect(advDevice, false)) {
                Serial.println("Reconnect failed");
                return false;
            }
            Serial.println("Reconnected client");
        }
        /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
        else {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if(!pClient) {
        if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.println("Max clients reached - no more connections available");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        Serial.println("New client created");

        pClient->setClientCallbacks(&clientCB, false);
        /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
         */
        pClient->setConnectionParams(12,12,0,51);
        /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
        pClient->setConnectTimeout(5);


        if (!pClient->connect(advDevice)) {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect, deleted client");
            return false;
        }
    }

    if(!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            Serial.println("Failed to connect");
            return false;
        }
    }

    Serial.print("Connected to: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());
    Serial.print("RSSI: ");
    Serial.println(pClient->getRssi());

    /** Now we can read/write/subscribe the charateristics of the services we are interested in */
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;

    pSvc = pClient->getService("ef680200-9b35-4933-9b10-52ffa9740042");
    if(pSvc) {     /** make sure it's not null */
        pChr = pSvc->getCharacteristic("ef680201-9b35-4933-9b10-52ffa9740042");

        if(pChr) {     /** make sure it's not null */
            if(pChr->canRead()) {
                Serial.print(pChr->getUUID().toString().c_str());
                Serial.print(" Value: ");
                Serial.println(pChr->readValue().c_str());
            }

            /** registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe().
             *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
             *  Unsubscribe parameter defaults are: response=false.
             */
            if(pChr->canNotify()) {
                //if(!pChr->registerForNotify(notifyCB)) {
                if(!pChr->subscribe(true, notifyCB, true)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
        }
        else{
            Serial.println("Temperature characteristic not found.");
        }
    } else {
        Serial.println("Environment service not found.");
        return false;
    }

    pChr = pSvc->getCharacteristic("ef680204-9b35-4933-9b10-52ffa9740042");
    if(pChr) {     /** make sure it's not null */
        if(pChr->canRead()) {
            Serial.print(pChr->getUUID().toString().c_str());
            Serial.print(" Value: ");
            Serial.println(pChr->readValue().c_str());
        }

        /** registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe().
         *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
         *  Unsubscribe parameter defaults are: response=false.
         */
        if(pChr->canNotify()) {
            //if(!pChr->registerForNotify(notifyCB)) {
            if(!pChr->subscribe(true, notifyCB, true)) {
                /** Disconnect if subscribe failed */
                pClient->disconnect();
                return false;
            }
        }
    }
    else{
        Serial.println("CO2 characteristic not found.");
    }


    Serial.println("Done with this device!");
    return true;
}

void setup (){
    Serial.begin(115200);
    pinMode(GPIO_NUM_10, OUTPUT);
    digitalWrite(GPIO_NUM_10, HIGH);
    M5.begin();
    // M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Axp.ScreenBreath(10);
    M5.Lcd.drawString("BLE", 0, 0, 2);


    Serial.println("Starting NimBLE Client");
    /** Initialize NimBLE, no device name spcified as we are not advertising */
    NimBLEDevice::init("");

    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_KEYBOARD_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

    /** 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, secure connections.
     *
     *  These are the default values, only shown here for demonstration.
     */
    //NimBLEDevice::setSecurityAuth(false, false, true);
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

    /** Optional: set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

    /** Optional: set any devices you don't want to get advertisments from */
    // NimBLEDevice::addIgnored(NimBLEAddress ("aa:bb:cc:dd:ee:ff"));

    /** create new scan */
    NimBLEScan* pScan = NimBLEDevice::getScan();

    /** create a callback that gets called when advertisers are found */
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(45);
    pScan->setWindow(15);

    /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);
    /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
    pScan->start(scanTime, scanEndedCB);
}

bool backlight_toogle = true;
void loop (){
    if(doConnect){
        /** Found a device we want to connect to, do it now */
        if(connectToServer()) {
            Serial.println("Success! we should now be getting notifications.");
        }
        doConnect = false;
    }
    if(M5.BtnA.wasPressed())
    {
        Serial.println("Button press");
        backlight_toogle = !backlight_toogle;
        M5.Axp.SetLDO2(backlight_toogle);
    }
    M5.update();
}
