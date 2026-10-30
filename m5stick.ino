#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "config.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <driver/rtc_io.h>
#include <SD.h>
#include "vendors.h"

// =====================================================
// TFT PINS — XIAO ESP32-S3
// =====================================================
#define TFT_SCLK 7
#define TFT_MOSI 9
#define TFT_RST  4
#define TFT_DC   5
#define TFT_CS   6

// SD card shares SCLK/MOSI with the display
#define SD_MISO  8
#define SD_CS    44

// Button pin (adjust to your actual button pin)
#define BTN_PIN  1
#define SELECT_PIN 2
#define POWER_PIN 3

// Saved networks (persisted in NVS namespace "wifi", edited from the setup AP)
const int SAVED_MAX = 4;
struct SavedNet {
    char ssid[33];
    char pass[65];
};
SavedNet savedNets[SAVED_MAX];
int savedCount = 0;
char activeSsid[33] = "";   // network we are joining / joined

SPIClass mySPI(FSPI);

Adafruit_ST7789 tft(
    &mySPI,
    TFT_CS,
    TFT_DC,
    TFT_RST
);

// =====================================================
// MENU SYSTEM
// =====================================================

struct MenuItem {
    const char* name;
    const char* hint;   // one-liner shown on the home focus card (<= 16 chars)
    uint16_t color;
};

MenuItem menuItems[] = {
    {"FLASHER",  "WHITE TORCH",   CYAN},
    {"WIFI",     "2.4 GHZ LINK",  GREEN},
    {"BLE",      "SCAN / BEACON", AMBER},
    {"TIMER",    "LAP / COUNTDOWN", CYAN},
    {"SD CARD",  "FILE BROWSER",  AMBER},
    {"SETTINGS", "DEVICE SETUP",  WHITE},
    {"SYSTEM",   "DIAGNOSTICS",   DIM_GREEN},
    {"ABOUT",    "BUILD INFO",    GRAY}
};

const int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);
int currentMenuItem = 0;
bool buttonPressed = false;
unsigned long lastButtonPress = 0;
unsigned long lastSelectPress = 0;
const unsigned long debounceDelay = 200;
bool wifiConnected = false;
bool powerPressed = false;
unsigned long lastPowerPress = 0;
unsigned long powerDebounceDelay = 200;

// Hold SEL this long to deep-sleep. Short press still selects.
bool selDown = false;
bool selLongFired = false;
unsigned long selDownAt = 0;
const unsigned long longPressMs = 800;

// Idle clock (lock screen) and auto-sleep, persisted from Settings.
unsigned long lastActivity = 0;
uint8_t brightnessPct = 100;
uint8_t sleepIndex = 0;
uint8_t saverIndex = 2;     // default 30s screensaver
int8_t bootTool = -1;       // -1 = home
uint8_t rotFlip = 0;

enum Screen : uint8_t {
    SCREEN_MENU,
    SCREEN_FLASH,
    SCREEN_ABOUT,
    SCREEN_WIFI_MENU,
    SCREEN_WIFI_SCAN,
    SCREEN_WIFI_DETAIL,
    SCREEN_WIFI_CHAN,
    SCREEN_WIFI_STATUS,
    SCREEN_WIFI_PORTAL,
    SCREEN_WIFI_PKTS,
    SCREEN_BLE_MENU,
    SCREEN_BLE_SCAN,
    SCREEN_BLE_REMOTE_MENU,
    SCREEN_BLE_REMOTE,
    SCREEN_WIFI_ACT,
    SCREEN_WALKER,
    SCREEN_TIMER_MENU,
    SCREEN_TIMER_SETUP,
    SCREEN_TIMER_RUN,
    SCREEN_SETTINGS,
    SCREEN_SYSTEM,
    SCREEN_SD,
    SCREEN_SD_VIEW
};

Screen screen = SCREEN_MENU;

// Lock screen state (drawn in lock_screen.ino)
bool locked = false;
Screen screenBeforeLock = SCREEN_MENU;
unsigned long lastLockTick = 0;
int lockShownMinute = -1;

const char* wifiMenuNames[] = {"CONNECT WIFI", "LINK INFO", "SHOW WIFI", "CHANNELS", "SETUP", "PACKETS", "BACK"};
const int wifiMenuCount = sizeof(wifiMenuNames) / sizeof(wifiMenuNames[0]);
int wifiMenuIndex = 0;

const char* bleMenuNames[] = {"SCAN DEVICES", "BEACON", "REMOTE", "BACK"};
const int bleMenuCount = sizeof(bleMenuNames) / sizeof(bleMenuNames[0]);
int bleMenuIndex = 0;
bool bleBeacon = false;
bool sdMounted = false;

const int SCAN_MAX = 12;
const int SCAN_PAGE = 5;

struct ScanHit {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;    // 0 for BLE
    uint8_t enc;        // wifi_auth_mode_t, 0 (open) for BLE
    uint8_t bssid[6];
};

ScanHit scanHits[SCAN_MAX];
int scanCount = 0;
int scanPage = 0;
int scanCursor = 0;          // WiFi list: absolute row index; == scanCount means the BACK row
bool scanFailed = false;
uint8_t chanCount[14];       // APs seen per channel 1..13 (all APs, not just the top SCAN_MAX)
int chanTotal = 0;

// Dialog geometry shared by the status cards
const int DLG_X = 20;
const int DLG_Y = 28;
const int DLG_W = 200;

// =====================================================
// CHROME — status bar, footer, lists, dialogs
// =====================================================

void drawWifiIcon(int x, int y, uint16_t color) {
    tft.fillCircle(x, y + 6, 1, color);
    tft.drawLine(x - 2, y + 3, x - 3, y + 2, color);
    tft.drawLine(x + 2, y + 3, x + 3, y + 2, color);
    tft.drawLine(x - 4, y + 1, x - 5, y, color);
    tft.drawLine(x + 4, y + 1, x + 5, y, color);
}

// Small "B" badge: lit when the beacon is advertising.
void drawBleIcon(int x, int y, uint16_t color) {
    tft.setTextSize(1);
    tft.setTextColor(color);
    tft.setCursor(x, y);
    tft.print("B");
}

// Header indicators only (used by full shell draws and status refreshes).
void drawStatusIcons() {
    drawBleIcon(208, 4, bleBeacon ? AMBER : DARK_GRAY);
    drawWifiIcon(226, 2, wifiConnected ? GREEN : DARK_GRAY);
}

// Repaints just the centre "n/N" counter in the footer.
void drawFooterCount(int index, int count) {
    tft.fillRect(84, 120, 72, 14, BG);
    if (count <= 0) return;

    char countBuf[8];
    snprintf(countBuf, sizeof(countBuf), "%d/%d", index + 1, count);
    int w = strlen(countBuf) * 6;
    tft.setTextSize(1);
    tft.setTextColor(GREEN);
    tft.setCursor((240 - w) / 2, 123);
    tft.print(countBuf);
}

// btnAction or selAction may be null. Both null means a blocking wait.
void drawShell(const char* page, const char* btnAction, const char* selAction, int index, int count) {
    tft.fillScreen(BG);
    tft.setTextSize(1);

    // Breadcrumb: NEOSTICK / PAGE
    tft.setTextColor(GREEN);
    tft.setCursor(6, 4);
    tft.print(DEVICE_NAME);
    tft.setTextColor(DARK_GRAY);
    tft.print(" / ");
    tft.setTextColor(GRAY);
    tft.print(page);

    drawStatusIcons();
    tft.drawFastHLine(0, 14, 240, DARK_GRAY);
    tft.drawFastHLine(0, 118, 240, DARK_GRAY);

    if (btnAction == nullptr && selAction == nullptr) {
        tft.setTextColor(AMBER);
        tft.setCursor(87, 123);
        tft.print("PLEASE WAIT");
        return;
    }

    if (btnAction != nullptr) {
        tft.setTextColor(DIM_GREEN);
        tft.setCursor(6, 123);
        tft.print("BTN");
        tft.setTextColor(WHITE);
        tft.print(" ");
        tft.print(btnAction);
    }

    drawFooterCount(index, count);

    if (selAction != nullptr) {
        int selW = (4 + strlen(selAction)) * 6;
        int x = 234 - selW;
        tft.setTextColor(DIM_GREEN);
        tft.setCursor(x, 123);
        tft.print("SEL");
        tft.setTextColor(WHITE);
        tft.print(" ");
        tft.print(selAction);
    }
}

void drawMenuList(const char** labels, const uint16_t* accents, const char** metas, int count, int selected, int rowH) {
    int panelH = count * rowH + 8;
    int panelY = 16 + (102 - panelH) / 2;
    int panelX = 8;
    int panelW = 224;

    tft.fillRoundRect(panelX, panelY, panelW, panelH, 4, PANEL);
    tft.drawRoundRect(panelX, panelY, panelW, panelH, 4, PANEL_HI);

    tft.setTextSize(1);
    for (int i = 0; i < count; i++) {
        int y = panelY + 4 + i * rowH;
        bool on = (i == selected);
        int textY = y + (rowH - 10) / 2;

        if (on) {
            tft.fillRect(panelX + 3, y, panelW - 6, rowH - 2, PANEL_HI);
            tft.fillRect(panelX + 3, y, 2, rowH - 2, accents[i]);
        }

        tft.setTextColor(on ? WHITE : GRAY);
        tft.setCursor(panelX + 12, textY);
        tft.print(labels[i]);

        if (metas != nullptr && metas[i] != nullptr) {
            bool live = (strcmp(metas[i], "ON") == 0);
            int mw = strlen(metas[i]) * 6;
            tft.setTextColor(live ? GREEN : GRAY);
            tft.setCursor(panelX + panelW - 12 - mw, textY);
            tft.print(metas[i]);
        }
    }
}

void paintDialogBar(int progress, uint16_t color) {
    int bw = DLG_W - 24;
    int bx = DLG_X + 12;
    int by = DLG_Y + 60;
    int inner = bw - 2;
    int fill = inner * progress / 100;
    if (fill < 0) fill = 0;
    if (fill > inner) fill = inner;

    tft.drawRect(bx, by, bw, 8, PANEL_HI);
    tft.fillRect(bx + 1, by + 1, inner, 6, PANEL);
    if (fill > 0) {
        tft.fillRect(bx + 1, by + 1, fill, 6, color);
    }
}

void drawDialog(const char* page, uint16_t accent, const char* title, const char* lineA, const char* lineB, int progress) {
    drawShell(page, nullptr, nullptr, 0, 0);

    tft.fillRoundRect(DLG_X, DLG_Y, DLG_W, 78, 4, PANEL);
    tft.drawRoundRect(DLG_X, DLG_Y, DLG_W, 78, 4, accent);

    tft.setTextSize(1);
    tft.setTextColor(accent);
    tft.setCursor(DLG_X + 12, DLG_Y + 8);
    tft.print(title);

    tft.drawFastHLine(DLG_X + 12, DLG_Y + 22, DLG_W - 24, DARK_GRAY);

    tft.setTextColor(WHITE);
    tft.setCursor(DLG_X + 12, DLG_Y + 30);
    tft.print(lineA);

    tft.setTextColor(GRAY);
    tft.setCursor(DLG_X + 12, DLG_Y + 44);
    tft.print(lineB);

    if (progress >= 0) {
        paintDialogBar(progress, accent);
    }
}

void drawCenteredNote(const char* title, const char* detail, uint16_t color) {
    tft.setTextSize(1);
    tft.setTextColor(color);
    tft.setCursor((240 - (int)strlen(title) * 6) / 2, 52);
    tft.print(title);
    tft.setTextColor(GRAY);
    tft.setCursor((240 - (int)strlen(detail) * 6) / 2, 68);
    tft.print(detail);
}

// Home geometry: compact tool list on the left, large focus card on the right.
const int HOME_LIST_X = 6;
const int HOME_LIST_Y = 18;
const int HOME_LIST_W = 104;
const int HOME_ROW_H  = 12;

const int HOME_CARD_X = 116;
const int HOME_CARD_Y = 18;
const int HOME_CARD_W = 118;
const int HOME_CARD_H = 96;

void drawHomeList() {
    tft.fillRect(HOME_LIST_X, HOME_LIST_Y, HOME_LIST_W, HOME_ROW_H * menuItemCount, BG);
    tft.setTextSize(1);

    for (int i = 0; i < menuItemCount; i++) {
        int y = HOME_LIST_Y + i * HOME_ROW_H;
        bool on = (i == currentMenuItem);

        if (on) {
            tft.fillRoundRect(HOME_LIST_X, y, HOME_LIST_W, HOME_ROW_H - 2, 3, PANEL_HI);
            tft.fillRect(HOME_LIST_X + 2, y + 2, 2, HOME_ROW_H - 6, menuItems[i].color);
        }

        tft.setTextColor(on ? WHITE : GRAY);
        tft.setCursor(HOME_LIST_X + 10, y + (HOME_ROW_H - 10) / 2 + 1);
        tft.print(menuItems[i].name);
    }
}

void drawHomeCard() {
    MenuItem &it = menuItems[currentMenuItem];
    int x = HOME_CARD_X;
    int y = HOME_CARD_Y;

    tft.fillRoundRect(x, y, HOME_CARD_W, HOME_CARD_H, 4, PANEL);
    tft.drawRoundRect(x, y, HOME_CARD_W, HOME_CARD_H, 4, it.color);

    // Big tool name (8 chars * 12px fits inside the card)
    tft.setTextSize(2);
    tft.setTextColor(it.color);
    tft.setCursor(x + 10, y + 8);
    tft.print(it.name);

    tft.drawFastHLine(x + 10, y + 30, HOME_CARD_W - 20, DARK_GRAY);

    tft.setTextSize(1);
    tft.setTextColor(GRAY);
    tft.setCursor(x + 10, y + 38);
    tft.print(it.hint);

    // Live status for the selected tool
    const char* status = nullptr;
    uint16_t statusColor = GRAY;
    switch (currentMenuItem) {
        case 1:
            status = wifiConnected ? "LINK ON" : "LINK OFF";
            statusColor = wifiConnected ? GREEN : GRAY;
            break;
        case 2:
            status = bleBeacon ? "BEACON ON" : "BEACON OFF";
            statusColor = bleBeacon ? GREEN : GRAY;
            break;
        case 3:
            status = timerBusy() ? "RUNNING" : "READY";
            statusColor = timerBusy() ? GREEN : GRAY;
            break;
        case 4:
            status = sdMounted ? "CARD IN" : "NO CARD";
            statusColor = sdMounted ? GREEN : GRAY;
            break;
        case 5:
            status = settingsBlurb();
            statusColor = (sleepIndex == 0) ? GRAY : AMBER;
            break;
        case 6:
            status = "LIVE";
            statusColor = DIM_GREEN;
            break;
        case 7:
            status = FW_VERSION;
            statusColor = DIM_GREEN;
            break;
    }
    if (status != nullptr) {
        tft.setTextColor(statusColor);
        tft.setCursor(x + 10, y + 52);
        tft.print(status);
    }

    // Position pips
    for (int i = 0; i < menuItemCount; i++) {
        uint16_t c = (i == currentMenuItem) ? it.color : DARK_GRAY;
        tft.fillRect(x + 10 + i * 8, y + HOME_CARD_H - 10, 5, 3, c);
    }
}

// full=false repaints only the list, card and footer counter (no flash on NEXT).
void drawMainMenu(bool full) {
    if (full) {
        drawShell("HOME", "NEXT", "OPEN", currentMenuItem, menuItemCount);
    } else {
        drawFooterCount(currentMenuItem, menuItemCount);
    }
    drawHomeList();
    drawHomeCard();
}

// =====================================================
// SCREENS
// =====================================================

void litFlash() {
    if (screen != SCREEN_FLASH) {
        screen = SCREEN_FLASH;
        tft.fillScreen(WHITE);
        tft.fillRect(0, 122, 240, 13, BG);
        tft.setTextSize(1);
        tft.setTextColor(GREEN);
        tft.setCursor(6, 124);
        tft.print(DEVICE_NAME);
        tft.setTextColor(DARK_GRAY);
        tft.print(" / TORCH");
        tft.setTextColor(DIM_GREEN);
        tft.setCursor(186, 124);
        tft.print("SEL");
        tft.setTextColor(WHITE);
        tft.print(" OFF");
        Serial.println("Flashlight ON");
    } else {
        screen = SCREEN_MENU;
        drawMainMenu(true);
        Serial.println("Flashlight OFF");
    }
}



void about() {
    screen = SCREEN_ABOUT;
    drawShell("ABOUT", nullptr, "BACK", 0, 0);

    int x = 16;
    int y = 24;
    int w = 208;
    int h = 86;

    tft.fillRoundRect(x, y, w, h, 4, PANEL);
    tft.drawRoundRect(x, y, w, h, 4, GREEN);
    tft.setTextSize(1);

    tft.setTextColor(GREEN);
    tft.setCursor(x + 12, y + 10);
    tft.print("POCKET OS");
    tft.setTextColor(DIM_GREEN);
    tft.setCursor(x + w - 12 - (int)strlen(FW_VERSION) * 6, y + 10);
    tft.print(FW_VERSION);

    tft.drawFastHLine(x + 12, y + 24, w - 24, DARK_GRAY);

    tft.setTextColor(GRAY);
    tft.setCursor(x + 12, y + 34);
    tft.print("BUILD");
    tft.setTextColor(WHITE);
    tft.setCursor(x + 66, y + 34);
    tft.print(DEVICE_NAME);

    tft.setTextColor(GRAY);
    tft.setCursor(x + 12, y + 50);
    tft.print("RADIO");
    tft.setTextColor(WHITE);
    tft.setCursor(x + 66, y + 50);
    tft.print("ESP32-S3");

    tft.setTextColor(GRAY);
    tft.setCursor(x + 12, y + 66);
    tft.print("MAKER");
    tft.setTextColor(WHITE);
    tft.setCursor(x + 66, y + 66);
    tft.print("Gavindu Achintha");

    Serial.println("About screen displayed");
}

void showSoon(const char* name) {
    drawDialog("HOME", AMBER, name, "NOT ON THIS BUILD", "NOT READY YET", -1);
    delay(900);
    screen = SCREEN_MENU;
    drawMainMenu(true);
}


void execBleMenuItem() {
    switch (bleMenuIndex) {
        case 0:
            showNearbyBle();
            break;
        case 1:
            setBeacon(!bleBeacon);
            drawDialog(
                "BLE",
                bleBeacon ? GREEN : AMBER,
                bleBeacon ? "BEACON ON" : "BEACON OFF",
                DEVICE_NAME,
                bleBeacon ? "VISIBLE TO PHONES" : "NOT ADVERTISING",
                -1
            );
            delay(1200);
            drawBleMiniMenu(true);
            break;
        case 2:
            enterRemoteMenu();
            break;
        default:
            screen = SCREEN_MENU;
            drawMainMenu(true);
            break;
    }
}

void execWifiMenuItem() {
    switch (wifiMenuIndex) {
        case 0:
            if (WiFi.status() == WL_CONNECTED) {
                wifiConnected = true;
                showWifiStatus();
            } else {
                wifiConnected = false;
                connectWIFI();
            }
            break;
        case 1:
            showWifiStatus();
            break;
        case 2:
            showNearbyWifi();
            break;
        case 3:
            showChannels();
            break;
        case 4:
            startPortal();
            break;
        case 5:
            startPacketMonitor();
            break;
        default:
            leaveWifiMenu();
            break;
    }
}

//-----------------------------------------------------------------

void executeMenuItem(int index) {
    switch (index) {
        case 0:
            litFlash();
            break;
        case 1:
            enterWifiMenu();
            break;
        case 2:
            enterBleMenu();
            break;
        case 3:
            enterTimerMenu();
            break;
        case 4:
            enterSd();
            break;
        case 5:
            enterSettings();
            break;
        case 6:
            enterSystem();
            break;
        case 7:
            about();
            break;
    }
}



// =====================================================
// SETUP
// =====================================================

void setup() {
    Serial.begin(115200);

    pinMode(BTN_PIN, INPUT_PULLUP);
    pinMode(SELECT_PIN, INPUT_PULLUP);
    pinMode(POWER_PIN, INPUT_PULLUP);

    // Keep the card deselected so it ignores display traffic
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    mySPI.begin(
        TFT_SCLK,
        SD_MISO,
        TFT_MOSI,
        -1
    );

    tft.init(135, 240);
    tft.setTextWrap(false);
    loadSettings();
    tft.setRotation(rotFlip ? 3 : 1);
    initBacklight();
    tft.fillScreen(BG);

    loadSavedNets();
    sdBegin();

    if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
        tft.setTextSize(2);
        tft.setTextColor(GREEN);
        tft.setCursor(96, 56);
        tft.print("WAKE");
        delay(250);
    } else {
        drawBootLogo();
        bootAnimation();
    }
    drawMainMenu(true);
    touchActivity();
    if (bootTool >= 0 && bootTool < menuItemCount) {
        currentMenuItem = bootTool;
        executeMenuItem(bootTool);
    }
}

// =====================================================
// INPUT
// =====================================================

void handleNext() {
    if (screen == SCREEN_MENU) {
        currentMenuItem++;
        if (currentMenuItem >= menuItemCount) currentMenuItem = 0;
        drawMainMenu(false);
    } else if (screen == SCREEN_WIFI_MENU) {
        wifiMenuIndex = (wifiMenuIndex + 1) % wifiMenuCount;
        drawWifiMiniMenu(false);
    } else if (screen == SCREEN_BLE_MENU) {
        bleMenuIndex = (bleMenuIndex + 1) % bleMenuCount;
        drawBleMiniMenu(false);
    } else if (screen == SCREEN_WIFI_SCAN) {
        scanCursor = (scanCursor + 1) % (scanCount + 1);
        scanPage = scanCursor / SCAN_PAGE;
        drawWifiResults();
    } else if (screen == SCREEN_WIFI_DETAIL) {
        openWifiActions();
    } else if (screen == SCREEN_WALKER) {
        walkerKick();
    } else if (screen == SCREEN_WIFI_ACT) {
        wifiActNext();
    } else if (screen == SCREEN_WIFI_CHAN) {
        showChannels();
    } else if (screen == SCREEN_WIFI_STATUS) {
        showWifiStatus();
    } else if (screen == SCREEN_WIFI_PKTS) {
        packetNext();
    } else if (screen == SCREEN_BLE_SCAN) {
        scanCursor = (scanCursor + 1) % (scanCount + 1);
        scanPage = scanCursor / SCAN_PAGE;
        drawBleResults();
    } else if (screen == SCREEN_BLE_REMOTE_MENU) {
        remoteMenuNext();
    } else if (screen == SCREEN_BLE_REMOTE) {
        remotePress(true);
    } else if (screen == SCREEN_TIMER_MENU) {
        timerMenuNext();
    } else if (screen == SCREEN_TIMER_SETUP) {
        timerSetupNext();
    } else if (screen == SCREEN_TIMER_RUN) {
        timerRunNext();
    } else if (screen == SCREEN_SETTINGS) {
        settingsNext();
    } else if (screen == SCREEN_SYSTEM) {
        drawSystemRows();
    } else if (screen == SCREEN_SD) {
        sdNext();
    }
}

void handleSelect() {
    if (screen == SCREEN_ABOUT) {
        screen = SCREEN_MENU;
        drawMainMenu(true);
    } else if (screen == SCREEN_FLASH) {
        litFlash();
    } else if (screen == SCREEN_WIFI_MENU) {
        execWifiMenuItem();
    } else if (screen == SCREEN_WIFI_SCAN) {
        if (scanCursor >= scanCount) {
            screen = SCREEN_WIFI_MENU;
            drawWifiMiniMenu(true);
        } else {
            openWifiActions();
        }
    } else if (screen == SCREEN_WIFI_ACT) {
        wifiActSelect();
    } else if (screen == SCREEN_WALKER) {
        leaveWalker();
    } else if (screen == SCREEN_WIFI_DETAIL) {
        joinFromDetail();
    } else if (screen == SCREEN_WIFI_CHAN || screen == SCREEN_WIFI_STATUS) {
        screen = SCREEN_WIFI_MENU;
        drawWifiMiniMenu(true);
    } else if (screen == SCREEN_WIFI_PORTAL) {
        stopPortal();
        screen = SCREEN_WIFI_MENU;
        drawWifiMiniMenu(true);
    } else if (screen == SCREEN_WIFI_PKTS) {
        stopPacketMonitor();
        screen = SCREEN_WIFI_MENU;
        drawWifiMiniMenu(true);
    } else if (screen == SCREEN_BLE_MENU) {
        execBleMenuItem();
    } else if (screen == SCREEN_BLE_SCAN) {
        if (scanCursor >= scanCount || scanFailed) {
            screen = SCREEN_BLE_MENU;
            drawBleMiniMenu(true);
        } else {
            startWalker(true);
        }
    } else if (screen == SCREEN_BLE_REMOTE_MENU) {
        remoteMenuSelect();
    } else if (screen == SCREEN_BLE_REMOTE) {
        remotePress(false);
    } else if (screen == SCREEN_TIMER_MENU) {
        timerMenuSelect();
    } else if (screen == SCREEN_TIMER_SETUP) {
        timerSetupSelect();
    } else if (screen == SCREEN_TIMER_RUN) {
        timerRunSelect();
    } else if (screen == SCREEN_SETTINGS) {
        settingsSelect();
    } else if (screen == SCREEN_SYSTEM) {
        screen = SCREEN_MENU;
        drawMainMenu(true);
    } else if (screen == SCREEN_SD) {
        sdSelect();
    } else if (screen == SCREEN_SD_VIEW) {
        sdViewSelect();
    } else {
        executeMenuItem(currentMenuItem);
    }
}

void screenTick() {
    if (screen == SCREEN_TIMER_RUN) timerTick();
    else if (screen == SCREEN_WIFI_STATUS) wifiStatusTick();
    else if (screen == SCREEN_WIFI_PORTAL) portalTick();
    else if (screen == SCREEN_WIFI_PKTS) packetTick();
    else if (screen == SCREEN_SYSTEM) systemTick();
    else if (screen == SCREEN_WALKER) walkerTick();
    else if (screen == SCREEN_SD_VIEW) sdImageTick();
    else if (screen == SCREEN_BLE_REMOTE || screen == SCREEN_BLE_REMOTE_MENU) remoteTick();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
    bool buttonState = (digitalRead(BTN_PIN) == LOW);
    bool selectState = (digitalRead(SELECT_PIN) == LOW);
    bool onOffState = (digitalRead(POWER_PIN) == LOW);

    // Hold SEL to sleep. Works on every screen, including the lock screen.
    if (selectState) {
        if (!selDown) {
            selDown = true;
            selDownAt = millis();
            selLongFired = false;
        } else if (!selLongFired && millis() - selDownAt >= longPressMs) {
            selLongFired = true;
            enterDeepSleep();
        }
    }
    bool selShort = false;
    if (!selectState && selDown) {
        selDown = false;
        selShort = !selLongFired && (millis() - lastSelectPress > debounceDelay);
    }

    if (locked) {
        bool nextEdge = buttonState && !buttonPressed && (millis() - lastButtonPress > debounceDelay);
        bool powerEdge = onOffState && !powerPressed && (millis() - lastPowerPress > powerDebounceDelay);
        if (nextEdge || selShort || powerEdge) {
            buttonPressed = buttonState;
            powerPressed = onOffState;
            lastButtonPress = lastSelectPress = lastPowerPress = millis();
            touchActivity();
            unlockScreen();
        } else {
            lockScreenTick();
            if (!timerBusy()) idleTick();
        }
        if (!buttonState) buttonPressed = false;
        if (!onOffState) powerPressed = false;
        delay(10);
        return;
    }

    if (buttonState && !buttonPressed && (millis() - lastButtonPress > debounceDelay)) {
        buttonPressed = true;
        lastButtonPress = millis();
        touchActivity();
        handleNext();
    }

    if (selShort) {
        lastSelectPress = millis();
        touchActivity();
        handleSelect();
    }

    if (onOffState && !powerPressed && (millis() - lastPowerPress > powerDebounceDelay)) {
        powerPressed = true;
        lastPowerPress = millis();
        touchActivity();
        if (screen == SCREEN_BLE_REMOTE) {
            exitRemoteScreen();
        } else if (screen == SCREEN_BLE_REMOTE_MENU) {
            stopRemote();
            screen = SCREEN_BLE_MENU;
            drawBleMiniMenu(true);
        } else {
            lockScreen();
        }
    }

    if (!buttonState) buttonPressed = false;
    if (!onOffState) powerPressed = false;

    screenTick();
    idleTick();
    delay(10);
}
