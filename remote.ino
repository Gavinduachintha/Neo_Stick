// =====================================================
// BLE REMOTE — keyboard for slides, media keys for volume
// =====================================================

#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <HIDTypes.h>

bool remoteConnected = false;
bool remoteAdvertising = false;
bool remoteModeVolume = false;
bool remoteShown = false;
bool remotePausedBeacon = false;
int remoteMenuIndex = 0;
unsigned long remoteRepeatAt = 0;

BLEHIDDevice* remoteHid = nullptr;
BLECharacteristic* remoteKeys = nullptr;
BLECharacteristic* remoteMedia = nullptr;

const char* remoteMenuNames[] = {"SLIDES", "VOLUME", "BACK"};
const int remoteMenuCount = 3;

static uint8_t remoteReportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00,
    0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x02, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x02, 0x09, 0xE9, 0x09, 0xEA, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x06, 0x81, 0x01, 0xC0
};

class RemoteCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override {
        (void)server;
        remoteConnected = true;
    }
    void onDisconnect(BLEServer *server) override {
        (void)server;
        remoteConnected = false;
        if (remoteAdvertising) BLEDevice::startAdvertising();
    }
};

RemoteCallbacks remoteCallbacks;

void ensureRemote() {
    ensureBle();
    if (remoteHid != nullptr) return;

    BLESecurity::setCapability(ESP_IO_CAP_NONE);
    BLESecurity::setAuthenticationMode(ESP_LE_AUTH_BOND);

    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(&remoteCallbacks);
    remoteHid = new BLEHIDDevice(server);
    remoteHid->manufacturer(DEVICE_NAME);
    remoteHid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    remoteHid->hidInfo(0x00, 0x01);
    remoteHid->reportMap(remoteReportMap, sizeof(remoteReportMap));
    remoteKeys = remoteHid->inputReport(1);
    remoteMedia = remoteHid->inputReport(2);
    remoteHid->startServices();
}

void startRemoteAdvertising() {
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->stop();
    advertising->addServiceUUID(remoteHid->hidService()->getUUID());
    advertising->setAppearance(HID_KEYBOARD);
    advertising->setScanResponse(true);
    advertising->setName(DEVICE_NAME);
    remoteAdvertising = true;
    advertising->start();
}

void drawRemoteMenu(bool full) {
    uint16_t accents[] = {CYAN, AMBER, GRAY};
    const char* metas[] = {nullptr, nullptr, nullptr};
    if (full) drawShell("REMOTE", "NEXT", "OPEN", remoteMenuIndex, remoteMenuCount);
    else drawFooterCount(remoteMenuIndex, remoteMenuCount);
    drawMenuList(remoteMenuNames, accents, metas, remoteMenuCount, remoteMenuIndex, 20);
}

void enterRemoteMenu() {
    remotePausedBeacon = bleBeacon;
    if (bleBeacon) setBeacon(false);
    ensureRemote();
    startRemoteAdvertising();
    screen = SCREEN_BLE_REMOTE_MENU;
    remoteMenuIndex = 0;
    remoteShown = remoteConnected;
    drawRemoteMenu(true);
}

void stopRemote() {
    remoteAdvertising = false;
    BLEDevice::stopAdvertising();
    if (remotePausedBeacon) {
        remotePausedBeacon = false;
        setBeacon(true);
    }
}

void drawRemote() {
    const char* btn = remoteModeVolume ? "VOL+" : "NEXT";
    const char* sel = remoteModeVolume ? "VOL-" : "PREV";
    drawShell("REMOTE", btn, sel, 0, 0);

    tft.setTextSize(2);
    tft.setTextColor(remoteModeVolume ? AMBER : CYAN);
    const char* mode = remoteModeVolume ? "VOLUME" : "SLIDES";
    tft.setCursor((240 - (int)strlen(mode) * 12) / 2, 28);
    tft.print(mode);

    tft.setTextSize(1);
    const char* link = remoteConnected ? "PHONE LINKED" : "PAIR  NEOSTICK";
    tft.setTextColor(remoteConnected ? GREEN : AMBER);
    tft.setCursor((240 - (int)strlen(link) * 6) / 2, 62);
    tft.print(link);

    tft.setTextColor(GRAY);
    tft.setCursor(84, 84);
    tft.print("PWR EXITS");
    remoteShown = remoteConnected;
}

void enterRemote(bool volume) {
    remoteModeVolume = volume;
    screen = SCREEN_BLE_REMOTE;
    drawRemote();
}

void exitRemoteScreen() {
    screen = SCREEN_BLE_REMOTE_MENU;
    drawRemoteMenu(true);
}

void remoteMenuNext() {
    remoteMenuIndex = (remoteMenuIndex + 1) % remoteMenuCount;
    drawRemoteMenu(false);
}

void remoteMenuSelect() {
    if (remoteMenuIndex == 0) enterRemote(false);
    else if (remoteMenuIndex == 1) enterRemote(true);
    else {
        stopRemote();
        screen = SCREEN_BLE_MENU;
        drawBleMiniMenu(true);
    }
}

void remoteTapKey(uint8_t keycode) {
    if (!remoteConnected || remoteKeys == nullptr) return;
    uint8_t report[8] = {0, 0, keycode, 0, 0, 0, 0, 0};
    remoteKeys->setValue(report, sizeof(report));
    remoteKeys->notify();
    delay(20);
    memset(report, 0, sizeof(report));
    remoteKeys->setValue(report, sizeof(report));
    remoteKeys->notify();
}

void remoteTapMedia(uint8_t bits) {
    if (!remoteConnected || remoteMedia == nullptr) return;
    uint8_t report = bits;
    remoteMedia->setValue(&report, 1);
    remoteMedia->notify();
    delay(20);
    report = 0;
    remoteMedia->setValue(&report, 1);
    remoteMedia->notify();
}

void remotePress(bool up) {
    if (remoteModeVolume) remoteTapMedia(up ? 0x01 : 0x02);
    else remoteTapKey(up ? 0x4F : 0x50);
    remoteRepeatAt = millis();
}

void remoteTick() {
    if (remoteConnected != remoteShown && screen == SCREEN_BLE_REMOTE) drawRemote();
    if (screen != SCREEN_BLE_REMOTE || !remoteModeVolume || !remoteConnected) return;
    if (millis() - remoteRepeatAt < 180) return;
    if (digitalRead(BTN_PIN) == LOW && millis() - lastButtonPress > 400) {
        remoteTapMedia(0x01);
        remoteRepeatAt = millis();
    } else if (selDown && !selLongFired && millis() - selDownAt > 400) {
        remoteTapMedia(0x02);
        remoteRepeatAt = millis();
    }
}
