//-----------------------------------------------------------------
// SAVED NETWORKS (NVS)
//-----------------------------------------------------------------

void loadSavedNets() {
    Preferences p;
    p.begin("wifi", true);
    savedCount = p.getInt("n", 0);
    if (savedCount > SAVED_MAX) savedCount = SAVED_MAX;
    for (int i = 0; i < savedCount; i++) {
        char k[8];
        snprintf(k, sizeof(k), "s%d", i);
        p.getString(k, savedNets[i].ssid, sizeof(savedNets[i].ssid));
        snprintf(k, sizeof(k), "p%d", i);
        p.getString(k, savedNets[i].pass, sizeof(savedNets[i].pass));
    }
    p.end();
    Serial.print("Saved networks: ");
    Serial.println(savedCount);
}

void storeSavedNets() {
    Preferences p;
    p.begin("wifi", false);
    p.putInt("n", savedCount);
    for (int i = 0; i < savedCount; i++) {
        char k[8];
        snprintf(k, sizeof(k), "s%d", i);
        p.putString(k, savedNets[i].ssid);
        snprintf(k, sizeof(k), "p%d", i);
        p.putString(k, savedNets[i].pass);
    }
    p.end();
}

int savedIndexOf(const char* ssid) {
    for (int i = 0; i < savedCount; i++) {
        if (strcmp(savedNets[i].ssid, ssid) == 0) return i;
    }
    return -1;
}

bool isSavedSsid(const char* ssid) {
    return savedIndexOf(ssid) >= 0;
}

// Adds or updates. When full, the oldest entry (index 0) is dropped.
void addSavedNet(const char* ssid, const char* pass) {
    int idx = savedIndexOf(ssid);
    if (idx < 0) {
        if (savedCount >= SAVED_MAX) {
            for (int i = 1; i < SAVED_MAX; i++) savedNets[i - 1] = savedNets[i];
            savedCount = SAVED_MAX - 1;
        }
        idx = savedCount++;
    }
    strncpy(savedNets[idx].ssid, ssid, 32);
    savedNets[idx].ssid[32] = '\0';
    strncpy(savedNets[idx].pass, pass, 64);
    savedNets[idx].pass[64] = '\0';
    storeSavedNets();
}

void removeSavedNet(int idx) {
    if (idx < 0 || idx >= savedCount) return;
    for (int i = idx; i < savedCount - 1; i++) savedNets[i] = savedNets[i + 1];
    savedCount--;
    memset(&savedNets[savedCount], 0, sizeof(SavedNet));
    storeSavedNets();
}

//-----------------------------------------------------------------
// CONNECT
//-----------------------------------------------------------------

void showWifiConnecting(int attempt, int maxAttempts) {
    int progress = (maxAttempts == 0) ? 0 : (attempt * 100) / maxAttempts;
    drawDialog("WIFI", AMBER, "CONNECTING", activeSsid, "JOINING NETWORK", progress);
}

void showWifiConnected() {
    char ipBuf[16];
    IPAddress ip = WiFi.localIP();
    snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    String s = WiFi.SSID();
    drawDialog("WIFI", GREEN, "LINKED", s.c_str(), ipBuf, -1);
}

void showWifiFailed() {
    drawDialog("WIFI", AMBER, "NO LINK", "COULD NOT JOIN", activeSsid, -1);
}

// Blocking join with the progress dialog. Returns true on success.
bool joinNetwork(const char* ssid, const char* pass) {
    const int maxAttempts = 20;
    strncpy(activeSsid, ssid, 32);
    activeSsid[32] = '\0';

    showWifiConnecting(0, maxAttempts);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    Serial.print("Connecting to ");
    Serial.println(ssid);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        delay(1000);
        attempts++;
        paintDialogBar((attempts * 100) / maxAttempts, AMBER);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.print("Connected! IP: ");
        Serial.println(WiFi.localIP());
        syncClock();
        touchActivity();
        return true;
    }

    wifiConnected = false;
    Serial.println("Connection failed!");
    showWifiFailed();
    delay(2200);
    return false;
}

// Joins the strongest saved network currently in range.
void connectWIFI() {
    if (savedCount == 0) {
        drawDialog("WIFI", AMBER, "NO NETWORKS", "NOTHING SAVED YET", "JOIN AN OPEN NET FIRST", -1);
        delay(1800);
        screen = SCREEN_WIFI_MENU;
        drawWifiMiniMenu(true);
        return;
    }

    drawDialog("WIFI", AMBER, "SEARCHING", "LOOKING FOR SAVED", "NETWORKS IN RANGE", -1);
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(false, true);

    int best = -1;
    int bestRssi = -127;
    for (int i = 0; i < n; i++) {
        String name = WiFi.SSID(i);
        int idx = savedIndexOf(name.c_str());
        if (idx >= 0 && WiFi.RSSI(i) > bestRssi) {
            bestRssi = WiFi.RSSI(i);
            best = idx;
        }
    }
    WiFi.scanDelete();

    if (best < 0) {
        best = savedCount - 1;   // most recently added; may be hidden SSID
    }

    if (joinNetwork(savedNets[best].ssid, savedNets[best].pass)) {
        showWifiStatus();
    } else {
        screen = SCREEN_WIFI_MENU;
        drawWifiMiniMenu(true);
    }
}

//-----------------------------------------------------------------
// MINI MENU
//-----------------------------------------------------------------

void drawWifiMiniMenu(bool full) {
    uint16_t accents[] = {GREEN, GREEN, GREEN, AMBER, CYAN, AMBER, GRAY};
    const char* metas[] = {wifiConnected ? "ON" : "OFF", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    if (full) {
        drawShell("WIFI", "NEXT", "OPEN", wifiMenuIndex, wifiMenuCount);
    } else {
        drawFooterCount(wifiMenuIndex, wifiMenuCount);
    }
    drawMenuList(wifiMenuNames, accents, metas, wifiMenuCount, wifiMenuIndex, 13);
}

void enterWifiMenu() {
    screen = SCREEN_WIFI_MENU;
    wifiMenuIndex = 0;
    drawWifiMiniMenu(true);
}

void leaveWifiMenu() {
    screen = SCREEN_MENU;
    drawMainMenu(true);
}

//-----------------------------------------------------------------
// SIGNAL / SECURITY HELPERS
//-----------------------------------------------------------------

int rssiLevel(int rssi) {
    if (rssi >= -50) return 4;
    if (rssi >= -60) return 3;
    if (rssi >= -70) return 2;
    if (rssi >= -80) return 1;
    return 0;
}

uint16_t rssiColor(int rssi) {
    if (rssi >= -50) return GREEN;
    if (rssi >= -70) return AMBER;
    return GRAY;
}

void drawSignalBars(int x, int y, int rssi) {
    int level = rssiLevel(rssi);
    uint16_t color = rssiColor(rssi);
    for (int i = 0; i < 4; i++) {
        int h = 2 + i * 2;
        uint16_t c = (i < level) ? color : DARK_GRAY;
        tft.fillRect(x + i * 4, y + (8 - h), 3, h, c);
    }
}

// Short tag for the list column (<= 4 chars)
const char* encShort(uint8_t enc) {
    switch (enc) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA2";
        case WIFI_AUTH_WPA3_PSK:
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA3";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
        default:                        return "SEC";
    }
}

// Longer label for the detail card
const char* encLong(uint8_t enc) {
    switch (enc) {
        case WIFI_AUTH_OPEN:            return "OPEN - NO PASSWORD";
        case WIFI_AUTH_WEP:             return "WEP (WEAK)";
        case WIFI_AUTH_WPA_PSK:         return "WPA PERSONAL";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2 PERSONAL";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2 MIXED";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3 PERSONAL";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3 MIXED";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 ENTERPRISE";
        default:                        return "SECURED";
    }
}

bool hitIsOpen(const ScanHit &h) {
    return h.enc == WIFI_AUTH_OPEN;
}

//-----------------------------------------------------------------
// SCAN BUFFER (shared with BLE)
//-----------------------------------------------------------------

void fillHit(ScanHit &h, const char* name, int rssi, uint8_t channel, uint8_t enc, const uint8_t* bssid) {
    strncpy(h.ssid, name, 32);
    h.ssid[32] = '\0';
    h.rssi = (int8_t)rssi;
    h.channel = channel;
    h.enc = enc;
    if (bssid != nullptr) {
        memcpy(h.bssid, bssid, 6);
    } else {
        memset(h.bssid, 0, 6);
    }
}

void rememberHit(const char* name, int rssi, uint8_t channel, uint8_t enc, const uint8_t* bssid) {
    if (scanCount < SCAN_MAX) {
        fillHit(scanHits[scanCount], name, rssi, channel, enc, bssid);
        scanCount++;
        return;
    }

    int weakest = 0;
    for (int i = 1; i < SCAN_MAX; i++) {
        if (scanHits[i].rssi < scanHits[weakest].rssi) {
            weakest = i;
        }
    }
    if (rssi > scanHits[weakest].rssi) {
        fillHit(scanHits[weakest], name, rssi, channel, enc, bssid);
    }
}

void sortHits() {
    for (int i = 0; i < scanCount; i++) {
        for (int j = i + 1; j < scanCount; j++) {
            if (scanHits[j].rssi > scanHits[i].rssi) {
                ScanHit tmp = scanHits[i];
                scanHits[i] = scanHits[j];
                scanHits[j] = tmp;
            }
        }
    }
}

//-----------------------------------------------------------------
// SCAN RESULT LIST
//-----------------------------------------------------------------

// Both lists use a row cursor and a trailing BACK row.
// wifiMode adds the channel and security columns. BLE uses SEL to walk a device.
void drawScanResults(const char* page, const char* column, const char* emptyTitle, const char* emptyDetail, bool wifiMode) {
    bool onBack = (scanCursor >= scanCount);
    const char* sel = onBack ? "BACK" : (wifiMode ? "OPEN" : "WALK");
    drawShell(page, "NEXT", sel, onBack ? scanCount : scanCursor, onBack ? 0 : scanCount);

    if (scanFailed) {
        drawCenteredNote("SCAN FAILED", emptyDetail, AMBER);
        return;
    }

    tft.setTextSize(1);
    tft.setTextColor(DIM_GREEN);
    tft.setCursor(16, 18);
    tft.print(column);
    if (wifiMode) {
        tft.setCursor(96, 18);
        tft.print("CH");
        tft.setCursor(118, 18);
        tft.print("SEC");
    }
    tft.setCursor(200, 18);
    tft.print("DBM");
    tft.drawFastHLine(16, 28, 208, DARK_GRAY);

    int start = scanPage * SCAN_PAGE;
    int rows = scanCount + 1;

    for (int i = 0; i < SCAN_PAGE && (start + i) < rows; i++) {
        int idx = start + i;
        int y = 34 + i * 16;
        bool on = (idx == scanCursor);

        if (idx == scanCount) {
            if (on) {
                tft.fillRoundRect(10, y - 3, 220, 14, 3, PANEL_HI);
                tft.fillRect(12, y - 1, 2, 10, GRAY);
            }
            tft.setTextColor(on ? WHITE : GRAY);
            tft.setCursor(18, y);
            tft.print(scanCount == 0 ? emptyTitle : "< BACK");
            continue;
        }

        ScanHit &hit = scanHits[idx];
        bool mine = wifiMode && isSavedSsid(hit.ssid);
        bool open = wifiMode && hitIsOpen(hit);
        const char* name = hit.ssid[0] ? hit.ssid : "(hidden)";

        if (on) {
            tft.fillRoundRect(10, y - 3, 220, 14, 3, PANEL_HI);
        } else if (open) {
            tft.fillRoundRect(10, y - 3, 220, 14, 3, PANEL);
        }
        if (mine || on) {
            tft.fillRect(12, y - 1, 2, 10, mine ? GREEN : (open ? AMBER : WHITE));
        }

        int maxChars = wifiMode ? 12 : 16;
        char label[17];
        strncpy(label, name, maxChars);
        label[maxChars] = '\0';

        tft.setTextColor(mine ? GREEN : WHITE);
        tft.setCursor(18, y);
        tft.print(label);

        if (wifiMode) {
            char chBuf[4];
            snprintf(chBuf, sizeof(chBuf), "%2u", hit.channel);
            tft.setTextColor(DIM_GREEN);
            tft.setCursor(96, y);
            tft.print(chBuf);

            tft.setTextColor(open ? AMBER : GRAY);
            tft.setCursor(118, y);
            tft.print(encShort(hit.enc));
        }

        drawSignalBars(156, y, hit.rssi);

        char rssiBuf[6];
        snprintf(rssiBuf, sizeof(rssiBuf), "%d", (int)hit.rssi);
        tft.setTextColor(rssiColor(hit.rssi));
        tft.setCursor(224 - (int)strlen(rssiBuf) * 6, y);
        tft.print(rssiBuf);
    }
}

void showScanning() {
    drawDialog("SCAN", AMBER, "SCANNING", "2.4 GHZ ONLY", "LISTENING FOR NETWORKS", -1);
}

void drawWifiResults() {
    drawScanResults("SCAN", "NETWORK", "NO NETWORKS", "2.4 GHZ ONLY", true);
}

// Runs a scan and fills scanHits + chanCount. Returns false on radio failure.
bool runWifiScan() {
    scanCount = 0;
    scanFailed = false;
    memset(chanCount, 0, sizeof(chanCount));
    chanTotal = 0;

    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(false, true);

    if (n < 0) {
        scanFailed = true;
    } else {
        for (int i = 0; i < n; i++) {
            String name = WiFi.SSID(i);
            uint8_t ch = (uint8_t)WiFi.channel(i);
            if (ch >= 1 && ch <= 13) {
                chanCount[ch]++;
                chanTotal++;
            }
            rememberHit(name.c_str(), WiFi.RSSI(i), ch, (uint8_t)WiFi.encryptionType(i), WiFi.BSSID(i));
        }
        sortHits();
    }
    WiFi.scanDelete();
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    return !scanFailed;
}

void showNearbyWifi() {
    screen = SCREEN_WIFI_SCAN;
    scanPage = 0;
    scanCursor = 0;
    showScanning();
    runWifiScan();
    drawWifiResults();
    Serial.print("Nearby networks: ");
    Serial.println(scanCount);
}

//-----------------------------------------------------------------
// NETWORK DETAIL
//-----------------------------------------------------------------

void detailRow(int x, int y, const char* label, const char* value, uint16_t color) {
    tft.setTextColor(GRAY);
    tft.setCursor(x, y);
    tft.print(label);
    tft.setTextColor(color);
    tft.setCursor(x + 60, y);
    tft.print(value);
}

bool detailJoinable() {
    if (scanCursor >= scanCount) return false;
    ScanHit &h = scanHits[scanCursor];
    return h.ssid[0] != '\0' && (hitIsOpen(h) || isSavedSsid(h.ssid));
}

void showWifiDetail() {
    screen = SCREEN_WIFI_DETAIL;
    ScanHit &h = scanHits[scanCursor];
    bool open = hitIsOpen(h);
    bool saved = isSavedSsid(h.ssid);
    uint16_t accent = saved ? GREEN : (open ? AMBER : GRAY);

    drawShell("INFO", "BACK", detailJoinable() ? "JOIN" : nullptr, 0, 0);

    int x = 16, y = 18, w = 208, h2 = 98;
    tft.fillRoundRect(x, y, w, h2, 4, PANEL);
    tft.drawRoundRect(x, y, w, h2, 4, accent);
    tft.setTextSize(1);

    // Title: SSID + saved/open tag
    char title[20];
    strncpy(title, h.ssid[0] ? h.ssid : "(hidden)", 19);
    title[19] = '\0';
    tft.setTextColor(accent);
    tft.setCursor(x + 10, y + 6);
    tft.print(title);

    const char* tag = saved ? "SAVED" : (open ? "OPEN" : nullptr);
    if (tag) {
        tft.setCursor(x + w - 10 - (int)strlen(tag) * 6, y + 6);
        tft.print(tag);
    }
    tft.drawFastHLine(x + 10, y + 18, w - 20, DARK_GRAY);

    char bssid[18];
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
             h.bssid[0], h.bssid[1], h.bssid[2], h.bssid[3], h.bssid[4], h.bssid[5]);
    detailRow(x + 10, y + 24, "BSSID", bssid, WHITE);
    detailRow(x + 10, y + 38, "VENDOR", lookupVendor(h.bssid), WHITE);

    char ch[16];
    snprintf(ch, sizeof(ch), "%u  (%u APS)", h.channel, chanCount[h.channel <= 13 ? h.channel : 0]);
    detailRow(x + 10, y + 52, "CHANNEL", ch, WHITE);
    detailRow(x + 10, y + 66, "SECURITY", encLong(h.enc), open ? AMBER : WHITE);

    char sig[12];
    snprintf(sig, sizeof(sig), "%d DBM", (int)h.rssi);
    detailRow(x + 10, y + 80, "SIGNAL", sig, rssiColor(h.rssi));
    drawSignalBars(x + 60 + (int)strlen(sig) * 6 + 8, y + 80, h.rssi);
}

// SEL on the detail screen: join saved / open networks, otherwise just go back.
void joinFromDetail() {
    if (!detailJoinable()) {
        screen = SCREEN_WIFI_SCAN;
        drawWifiResults();
        return;
    }

    ScanHit &h = scanHits[scanCursor];
    int idx = savedIndexOf(h.ssid);
    bool ok;
    if (idx >= 0) {
        ok = joinNetwork(savedNets[idx].ssid, savedNets[idx].pass);
    } else {
        ok = joinNetwork(h.ssid, "");
        if (ok) {
            addSavedNet(h.ssid, "");
            drawDialog("WIFI", GREEN, "SAVED", h.ssid, "WILL AUTO-JOIN NEXT TIME", -1);
            delay(1400);
        }
    }

    if (ok) {
        showWifiStatus();
    } else {
        screen = SCREEN_WIFI_MENU;
        drawWifiMiniMenu(true);
    }
}

//-----------------------------------------------------------------
// ROW ACTIONS — details, walk, join
//-----------------------------------------------------------------

const char* wifiActLabels[4];
int wifiActCount = 0;
int wifiActIndex = 0;

void drawWifiActions(bool full) {
    wifiActCount = 0;
    wifiActLabels[wifiActCount++] = "DETAILS";
    wifiActLabels[wifiActCount++] = "WALK";
    if (detailJoinable()) wifiActLabels[wifiActCount++] = "JOIN";
    wifiActLabels[wifiActCount++] = "BACK";
    if (wifiActIndex >= wifiActCount) wifiActIndex = 0;

    uint16_t accents[] = {GREEN, AMBER, GREEN, GRAY};
    const char* metas[] = {nullptr, nullptr, nullptr, nullptr};
    if (full) {
        drawShell("WIFI", "NEXT", "OPEN", wifiActIndex, wifiActCount);
    } else {
        drawFooterCount(wifiActIndex, wifiActCount);
    }
    drawMenuList(wifiActLabels, accents, metas, wifiActCount, wifiActIndex, 20);
}

void openWifiActions() {
    screen = SCREEN_WIFI_ACT;
    wifiActIndex = 0;
    drawWifiActions(true);
}

void wifiActNext() {
    wifiActIndex = (wifiActIndex + 1) % wifiActCount;
    drawWifiActions(false);
}

void wifiActSelect() {
    const char* item = wifiActLabels[wifiActIndex];
    if (strcmp(item, "DETAILS") == 0) showWifiDetail();
    else if (strcmp(item, "WALK") == 0) startWalker(false);
    else if (strcmp(item, "JOIN") == 0) joinFromDetail();
    else {
        screen = SCREEN_WIFI_SCAN;
        drawWifiResults();
    }
}

//-----------------------------------------------------------------
// CHANNEL ANALYSER
//-----------------------------------------------------------------

// Least-loaded of the non-overlapping channels 1 / 6 / 11.
int bestChannel() {
    int best = 1;
    const int picks[] = {1, 6, 11};
    for (int i = 1; i < 3; i++) {
        if (chanCount[picks[i]] < chanCount[best]) best = picks[i];
    }
    return best;
}

void drawChannelChart() {
    drawShell("CHANNELS", "RESCAN", "BACK", 0, 0);

    if (scanFailed) {
        drawCenteredNote("SCAN FAILED", "TRY AGAIN", AMBER);
        return;
    }

    tft.setTextSize(1);

    // Summary line
    char buf[24];
    snprintf(buf, sizeof(buf), "BEST CH %d", bestChannel());
    tft.setTextColor(GREEN);
    tft.setCursor(16, 18);
    tft.print(buf);

    snprintf(buf, sizeof(buf), "%d APS", chanTotal);
    tft.setTextColor(GRAY);
    tft.setCursor(224 - (int)strlen(buf) * 6, 18);
    tft.print(buf);

    // Chart geometry
    const int baseY = 104;
    const int maxH  = 56;
    const int x0    = 24;
    const int pitch = 15;
    const int barW  = 11;

    int peak = 1;
    for (int c = 1; c <= 13; c++) {
        if (chanCount[c] > peak) peak = chanCount[c];
    }

    int liveCh = wifiConnected ? WiFi.channel() : 0;

    tft.drawFastHLine(x0 - 4, baseY, 13 * pitch + 4, DARK_GRAY);

    for (int c = 1; c <= 13; c++) {
        int x = x0 + (c - 1) * pitch;
        int n = chanCount[c];
        int h = (n == 0) ? 0 : (n * maxH + peak - 1) / peak;
        if (n > 0 && h < 3) h = 3;

        uint16_t color = (n <= 2) ? GREEN : (n <= 5 ? AMBER : WHITE);
        if (h > 0) {
            tft.fillRect(x, baseY - h, barW, h, color);
            if (n > 5) tft.drawRect(x, baseY - h, barW, h, AMBER);

            snprintf(buf, sizeof(buf), "%d", n);
            tft.setTextColor(GRAY);
            tft.setCursor(x + (barW - (int)strlen(buf) * 6) / 2, baseY - h - 9);
            tft.print(buf);
        } else {
            tft.drawFastHLine(x, baseY - 1, barW, DARK_GRAY);
        }

        // Channel label; the joined channel gets the accent underline
        snprintf(buf, sizeof(buf), "%d", c);
        tft.setTextColor(c == liveCh ? GREEN : DIM_GREEN);
        tft.setCursor(x + (barW - (int)strlen(buf) * 6) / 2, baseY + 3);
        tft.print(buf);
        if (c == liveCh) {
            tft.fillRect(x, baseY + 12, barW, 2, GREEN);
        }
    }
}

void showChannels() {
    screen = SCREEN_WIFI_CHAN;
    drawDialog("CHANNELS", AMBER, "SCANNING", "COUNTING ACCESS POINTS", "PER CHANNEL 1-13", -1);
    runWifiScan();
    drawChannelChart();
}

//-----------------------------------------------------------------
// LINK STATUS
//-----------------------------------------------------------------

unsigned long lastStatusTick = 0;

void ipText(char* buf, size_t n, IPAddress ip) {
    snprintf(buf, n, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void showWifiStatus() {
    screen = SCREEN_WIFI_STATUS;
    lastStatusTick = millis();
    drawShell("LINK", "REFRESH", "BACK", 0, 0);

    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        drawCenteredNote("NO LINK", "CONNECT FIRST", AMBER);
        return;
    }
    wifiConnected = true;

    int x = 16;
    int y = 18;
    int w = 208;
    int h = 98;
    tft.fillRoundRect(x, y, w, h, 4, PANEL);
    tft.drawRoundRect(x, y, w, h, 4, GREEN);
    tft.setTextSize(1);

    char title[20];
    String ssid = WiFi.SSID();
    strncpy(title, ssid.c_str(), 19);
    title[19] = '\0';
    tft.setTextColor(GREEN);
    tft.setCursor(x + 10, y + 6);
    tft.print(title);
    tft.drawFastHLine(x + 10, y + 18, w - 20, DARK_GRAY);

    char buf[20];
    int row = y + 24;
    ipText(buf, sizeof(buf), WiFi.localIP());
    detailRow(x + 10, row, "IP", buf, WHITE);
    row += 12;
    ipText(buf, sizeof(buf), WiFi.gatewayIP());
    detailRow(x + 10, row, "GATEWAY", buf, WHITE);
    row += 12;
    ipText(buf, sizeof(buf), WiFi.dnsIP());
    detailRow(x + 10, row, "DNS", buf, WHITE);
    row += 12;

    int rssi = WiFi.RSSI();
    snprintf(buf, sizeof(buf), "%d DBM", rssi);
    detailRow(x + 10, row, "SIGNAL", buf, rssiColor(rssi));
    drawSignalBars(x + 70 + (int)strlen(buf) * 6 + 6, row, rssi);
    row += 12;

    snprintf(buf, sizeof(buf), "%u", (unsigned)WiFi.channel());
    detailRow(x + 10, row, "CHANNEL", buf, WHITE);
    row += 12;

    String mac = WiFi.macAddress();
    mac.toUpperCase();
    detailRow(x + 10, row, "MAC", mac.c_str(), DIM_GREEN);
}

void wifiStatusTick() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (millis() - lastStatusTick < 1000) return;
    lastStatusTick = millis();

    int x = 26;
    int y = 18 + 24 + 36;
    int rssi = WiFi.RSSI();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d DBM", rssi);
    tft.fillRect(x + 60, y, 120, 10, PANEL);
    tft.setTextSize(1);
    tft.setTextColor(rssiColor(rssi));
    tft.setCursor(x + 60, y);
    tft.print(buf);
    drawSignalBars(x + 60 + (int)strlen(buf) * 6 + 6, y, rssi);
}
