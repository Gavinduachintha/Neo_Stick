// =====================================================
// SIGNAL WALKER — one target, a bar that grows as you approach
// =====================================================

bool walkerBle = false;
bool walkerSeen = false;
char walkerName[33] = "";
uint8_t walkerAddr[6] = {0, 0, 0, 0, 0, 0};
uint8_t walkerChannel = 0;
int walkerRssi = -100;
unsigned long walkerDue = 0;
bool walkerBusy = false;
Screen walkerReturn = SCREEN_WIFI_ACT;
bool walkerPausedBeacon = false;

bool walkerAddrBlank() {
    for (int i = 0; i < 6; i++) if (walkerAddr[i] != 0) return false;
    return true;
}

bool walkerAddrMatch(const uint8_t* addr) {
    if (addr == nullptr || walkerAddrBlank()) return false;
    return memcmp(addr, walkerAddr, 6) == 0;
}

void noteWalkerRssi(int rssi, bool seen) {
    if (!seen) {
        walkerSeen = false;
        if (walkerRssi > -100) walkerRssi -= 6;
        return;
    }
    walkerSeen = true;
    if (walkerRssi < -90) walkerRssi = rssi;
    else walkerRssi = (walkerRssi * 3 + rssi) / 4;
}

void probeWifiWalker() {
    stopPacketMonitor();
    WiFi.mode(WIFI_STA);
    uint8_t channel = walkerChannel >= 1 && walkerChannel <= 13 ? walkerChannel : 0;
    int n = WiFi.scanNetworks(false, true, false, 120, channel, nullptr, walkerAddrBlank() ? nullptr : walkerAddr);
    int best = -127;
    bool seen = false;
    for (int i = 0; i < n; i++) {
        bool match = walkerAddrMatch(WiFi.BSSID(i));
        if (!match && walkerAddrBlank()) match = (WiFi.SSID(i) == walkerName);
        if (!match) continue;
        seen = true;
        if (WiFi.RSSI(i) > best) best = WiFi.RSSI(i);
        walkerChannel = (uint8_t)WiFi.channel(i);
    }
    WiFi.scanDelete();
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    noteWalkerRssi(best, seen);
}

void probeBleWalker() {
    ensureBle();
    if (bleBeacon) BLEDevice::stopAdvertising();
    BLEScan *scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(80);
    scan->setWindow(60);
    scan->clearResults();
    BLEScanResults *found = scan->start(1, false);
    int best = -127;
    bool seen = false;
    if (found != nullptr) {
        for (int i = 0; i < found->getCount(); i++) {
            BLEAdvertisedDevice dev = found->getDevice(i);
            BLEAddress foundAddr = dev.getAddress();
            bool match = walkerAddrMatch(foundAddr.getNative());
            if (!match && walkerAddrBlank()) {
                String name = dev.haveName() ? dev.getName() : foundAddr.toString();
                match = (name == walkerName);
            }
            if (!match) continue;
            seen = true;
            if (dev.getRSSI() > best) best = dev.getRSSI();
        }
        scan->clearResults();
    }
    noteWalkerRssi(best, seen);
}

int walkerFill() {
    int level = walkerRssi + 95;
    if (level < 0) level = 0;
    if (level > 55) level = 55;
    return level * 200 / 55;
}

void drawWalker(bool full) {
    if (full) {
        drawShell("WALK", "SCAN", "BACK", 0, 0);
        tft.setTextSize(1);
        tft.setTextColor(walkerBle ? AMBER : GREEN);
        tft.setCursor(16, 22);
        tft.print(walkerBle ? "BLE" : "WIFI");

        char label[19];
        strncpy(label, walkerName[0] ? walkerName : "(hidden)", 18);
        label[18] = '\0';
        tft.setTextColor(WHITE);
        tft.setCursor(52, 22);
        tft.print(label);
    }

    tft.fillRect(16, 40, 208, 28, BG);
    tft.drawRoundRect(16, 40, 208, 28, 4, PANEL_HI);
    int fill = walkerSeen ? walkerFill() : 0;
    if (fill > 0) {
        uint16_t color = walkerRssi >= -55 ? GREEN : (walkerRssi >= -75 ? AMBER : GRAY);
        tft.fillRoundRect(20, 44, fill, 20, 3, color);
    }

    tft.fillRect(16, 76, 208, 28, BG);
    tft.setTextSize(2);
    if (!walkerSeen) {
        tft.setTextColor(AMBER);
        tft.setCursor(48, 80);
        tft.print("NO SIGNAL");
    } else {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d DBM", walkerRssi);
        tft.setTextColor(walkerRssi >= -60 ? GREEN : WHITE);
        tft.setCursor((240 - (int)strlen(buf) * 12) / 2, 80);
        tft.print(buf);
    }
}

void startWalker(bool ble) {
    if (scanCursor < 0 || scanCursor >= scanCount) return;
    ScanHit &hit = scanHits[scanCursor];
    walkerBle = ble;
    walkerReturn = ble ? SCREEN_BLE_SCAN : SCREEN_WIFI_ACT;
    strncpy(walkerName, hit.ssid, 32);
    walkerName[32] = '\0';
    memcpy(walkerAddr, hit.bssid, 6);
    walkerChannel = hit.channel;
    walkerRssi = hit.rssi;
    walkerSeen = true;
    walkerPausedBeacon = ble && bleBeacon;
    if (walkerPausedBeacon) setBeacon(false);
    screen = SCREEN_WALKER;
    walkerDue = millis();
    drawWalker(true);
}

void stopWalker() {
    if (walkerPausedBeacon) {
        walkerPausedBeacon = false;
        setBeacon(true);
    }
}

void leaveWalker() {
    stopWalker();
    screen = walkerReturn;
    if (walkerReturn == SCREEN_BLE_SCAN) drawBleResults();
    else openWifiActions();
}

void walkerKick() {
    walkerDue = 0;
    walkerTick();
}

void walkerTick() {
    if (walkerBusy || millis() < walkerDue) return;
    walkerBusy = true;
    if (walkerBle) probeBleWalker();
    else probeWifiWalker();
    walkerBusy = false;
    drawWalker(false);
    walkerDue = millis() + 250;
    touchActivity();
}
