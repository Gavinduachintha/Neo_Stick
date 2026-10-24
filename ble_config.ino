//-----------------------------------------------------------------
// BLE
//-----------------------------------------------------------------

void ensureBle() {
    if (!BLEDevice::getInitialized()) {
        BLEDevice::init(DEVICE_NAME);
    }
}

void setBeacon(bool on) {
    ensureBle();
    if (!on) {
        BLEDevice::stopAdvertising();
        bleBeacon = false;
        return;
    }

    BLEAdvertisementData data;
    data.setFlags(0x06);
    data.setName(DEVICE_NAME);

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->setScanResponse(true);
    advertising->setAdvertisementData(data);
    BLEDevice::startAdvertising();
    bleBeacon = true;
}

void drawBleMiniMenu(bool full) {
    uint16_t accents[] = {AMBER, GREEN, GRAY};
    const char* metas[] = {nullptr, bleBeacon ? "ON" : "OFF", nullptr};

    if (full) {
        drawShell("BLE", "NEXT", "OPEN", bleMenuIndex, bleMenuCount);
    } else {
        drawFooterCount(bleMenuIndex, bleMenuCount);
    }
    drawMenuList(bleMenuNames, accents, metas, bleMenuCount, bleMenuIndex, 20);
}

void enterBleMenu() {
    screen = SCREEN_BLE_MENU;
    bleMenuIndex = 0;
    drawBleMiniMenu(true);
}

void drawBleResults() {
    drawScanResults("BLE", "DEVICE", "NO DEVICES", "NONE IN RANGE", false);
}

void showNearbyBle() {
    screen = SCREEN_BLE_SCAN;
    scanCount = 0;
    scanPage = 0;
    scanFailed = false;
    drawDialog("BLE", AMBER, "SCANNING", "NEARBY DEVICES", "LISTENING", -1);

    bool resumeBeacon = bleBeacon;
    ensureBle();
    if (resumeBeacon) {
        BLEDevice::stopAdvertising();
    }

    BLEScan *scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->clearResults();

    BLEScanResults *found = scan->start(4, false);
    if (found == nullptr) {
        scanFailed = true;
    } else {
        int n = found->getCount();
        for (int i = 0; i < n; i++) {
            BLEAdvertisedDevice dev = found->getDevice(i);
            String label = dev.haveName() ? dev.getName() : dev.getAddress().toString();
            rememberHit(label.c_str(), dev.getRSSI(), 0, 0, nullptr);
        }
        sortHits();
        scan->clearResults();
    }

    if (resumeBeacon) {
        setBeacon(true);
    }

    drawBleResults();
    Serial.print("Nearby BLE devices: ");
    Serial.println(scanCount);
}
