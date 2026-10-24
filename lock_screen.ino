// =====================================================
// LOCK SCREEN — clock + link status, any key to unlock
// =====================================================

#include <time.h>

// State lives in m5stick.ino (locked, screenBeforeLock, lastLockTick, lockShownMinute)

// Clock geometry: "HH:MM" at size 4 (120x32) + seconds at size 2 (24x16)
const int LOCK_CLOCK_W = 120 + 8 + 24;
const int LOCK_CLOCK_X = (240 - LOCK_CLOCK_W) / 2;
const int LOCK_CLOCK_Y = 26;

// Status panel geometry
const int LOCK_PANEL_X = 8;
const int LOCK_PANEL_Y = 78;
const int LOCK_PANEL_W = 224;
const int LOCK_PANEL_H = 36;

void syncClock() {
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
}

bool clockNow(struct tm &t) {
    // 0 ms timeout: returns false until NTP has set a sane year
    return getLocalTime(&t, 0);
}

void upperCase(char* s) {
    for (; *s; s++) *s = toupper((unsigned char)*s);
}

void printCentered(const char* s, int y, uint16_t color) {
    tft.setTextColor(color);
    tft.setCursor((240 - (int)strlen(s) * 6) / 2, y);
    tft.print(s);
}

// Redraws HH:MM and the date only when the minute changes; seconds every call.
void drawLockClock(bool force) {
    struct tm t;
    bool ok = clockNow(t);

    char hm[6] = "--:--";
    char ss[3] = "--";
    char date[24] = "NO TIME SYNC";
    int minute = -1;

    if (ok) {
        strftime(hm, sizeof(hm), "%H:%M", &t);
        strftime(ss, sizeof(ss), "%S", &t);
        strftime(date, sizeof(date), "%a %d %b %Y", &t);
        upperCase(date);
        minute = t.tm_min;
    } else if (!wifiConnected) {
        strcpy(date, "LINK WIFI TO SYNC CLOCK");
    }

    if (force || minute != lockShownMinute) {
        lockShownMinute = minute;

        tft.fillRect(LOCK_CLOCK_X, LOCK_CLOCK_Y, 120, 32, BG);
        tft.setTextSize(4);
        tft.setTextColor(ok ? WHITE : GRAY);
        tft.setCursor(LOCK_CLOCK_X, LOCK_CLOCK_Y);
        tft.print(hm);

        tft.fillRect(0, LOCK_CLOCK_Y + 38, 240, 10, BG);
        tft.setTextSize(1);
        printCentered(date, LOCK_CLOCK_Y + 38, ok ? GRAY : AMBER);
    }

    int sx = LOCK_CLOCK_X + 128;
    int sy = LOCK_CLOCK_Y + 16;
    tft.fillRect(sx, sy, 24, 16, BG);
    tft.setTextSize(2);
    tft.setTextColor(ok ? GREEN : DARK_GRAY);
    tft.setCursor(sx, sy);
    tft.print(ss);
}

void drawLockUptime() {
    unsigned long s = millis() / 1000;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (s / 3600) % 100, (s / 60) % 60, s % 60);

    int x = LOCK_PANEL_X + LOCK_PANEL_W - 10 - 8 * 6;
    int y = LOCK_PANEL_Y + 21;
    tft.fillRect(x - 18, y, 18 + 8 * 6, 8, PANEL);
    tft.setTextSize(1);
    tft.setTextColor(DIM_GREEN);
    tft.setCursor(x - 18, y);
    tft.print("UP ");
    tft.setTextColor(GRAY);
    tft.print(buf);
}

void drawLockPanel() {
    int x = LOCK_PANEL_X;
    int y = LOCK_PANEL_Y;

    tft.fillRoundRect(x, y, LOCK_PANEL_W, LOCK_PANEL_H, 4, PANEL);
    tft.drawRoundRect(x, y, LOCK_PANEL_W, LOCK_PANEL_H, 4, PANEL_HI);
    tft.setTextSize(1);

    // Row 1 — WiFi
    int r1 = y + 7;
    bool linked = wifiConnected && (WiFi.status() == WL_CONNECTED);
    drawWifiIcon(x + 15, r1 - 2, linked ? GREEN : DARK_GRAY);
    tft.setCursor(x + 26, r1);
    if (linked) {
        char ssid[17];
        String live = WiFi.SSID();
        strncpy(ssid, live.c_str(), 16);
        ssid[16] = '\0';
        tft.setTextColor(WHITE);
        tft.print(ssid);

        char ipBuf[16];
        IPAddress ip = WiFi.localIP();
        snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        int ipX = x + LOCK_PANEL_W - 10 - (int)strlen(ipBuf) * 6;
        tft.setTextColor(GRAY);
        tft.setCursor(ipX, r1);
        tft.print(ipBuf);

        drawSignalBars(ipX - 22, r1 - 1, WiFi.RSSI());
    } else {
        tft.setTextColor(GRAY);
        tft.print("WIFI OFFLINE");
    }

    // Row 2 — BLE + uptime
    int r2 = y + 21;
    drawBleIcon(x + 12, r2, bleBeacon ? AMBER : DARK_GRAY);
    tft.setTextColor(bleBeacon ? GREEN : GRAY);
    tft.setCursor(x + 26, r2);
    tft.print(bleBeacon ? "BEACON ON" : "BEACON OFF");

    drawLockUptime();
}

void drawLockScreen() {
    tft.fillScreen(BG);
    tft.setTextSize(1);

    tft.setTextColor(DIM_GREEN);
    tft.setCursor(6, 4);
    tft.print(DEVICE_NAME);
    tft.setTextColor(DARK_GRAY);
    tft.print(" / LOCKED");
    drawStatusIcons();
    tft.drawFastHLine(0, 14, 240, DARK_GRAY);

    drawLockClock(true);
    drawLockPanel();

    tft.drawFastHLine(0, 118, 240, DARK_GRAY);
    tft.setTextSize(1);
    printCentered("PRESS ANY KEY TO UNLOCK", 123, DIM_GREEN);
}

void lockScreen() {
    if (locked) return;
    locked = true;
    screenBeforeLock = screen;
    lockShownMinute = -1;
    lastLockTick = millis();
    drawLockScreen();
    Serial.println("Locked");
}

// Repaint whatever screen was showing before the lock.
void redrawScreen(Screen s) {
    switch (s) {
        case SCREEN_FLASH:
            screen = SCREEN_MENU;   // litFlash() toggles on; force the "on" branch
            litFlash();
            break;
        case SCREEN_ABOUT:
            about();
            break;
        case SCREEN_WIFI_MENU:
            drawWifiMiniMenu(true);
            break;
        case SCREEN_WIFI_SCAN:
            drawWifiResults();
            break;
        case SCREEN_WIFI_DETAIL:
            showWifiDetail();
            break;
        case SCREEN_WIFI_CHAN:
            drawChannelChart();
            break;
        case SCREEN_WIFI_STATUS:
            showWifiStatus();
            break;
        case SCREEN_TIMER_MENU:
            drawTimerMenu(true);
            break;
        case SCREEN_TIMER_SETUP:
            drawTimerSetup();
            break;
        case SCREEN_TIMER_RUN:
            drawTimerFace(true);
            break;
        case SCREEN_SETTINGS:
            drawSettings(true);
            break;
        case SCREEN_SYSTEM:
            enterSystem();
            break;
        case SCREEN_BLE_MENU:
            drawBleMiniMenu(true);
            break;
        case SCREEN_BLE_SCAN:
            drawBleResults();
            break;
        default:
            drawMainMenu(true);
            break;
    }
}

void unlockScreen() {
    locked = false;
    screen = screenBeforeLock;
    redrawScreen(screen);
    Serial.println("Unlocked");
}

// Called every loop while locked; refreshes seconds and uptime once a second.
void lockScreenTick() {
    if (millis() - lastLockTick >= 1000) {
        lastLockTick = millis();
        drawLockClock(false);
        drawLockUptime();
    }
}
