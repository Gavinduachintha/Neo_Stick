// =====================================================
// SETTINGS — persisted in NVS namespace "cfg"
// =====================================================

const char* settingsNames[] = {"BRIGHTNESS", "SAVER", "SLEEP", "BOOT", "ROTATE", "BACK"};
const int settingsCount = sizeof(settingsNames) / sizeof(settingsNames[0]);
int settingsIndex = 0;

const char* saverLabels[] = {"OFF", "15S", "30S", "1M"};
const unsigned long saverTimes[] = {0, 15000, 30000, 60000};
const int saverOptionCount = 4;

const char* sleepLabels[] = {"OFF", "30S", "1M", "2M", "5M"};
const unsigned long sleepTimes[] = {0, 30000, 60000, 120000, 300000};
const int sleepOptionCount = 5;

const uint8_t brightnessSteps[] = {10, 25, 50, 75, 100};

unsigned long saverTimeoutMs() {
    if (saverIndex >= saverOptionCount) return 30000;
    return saverTimes[saverIndex];
}

unsigned long sleepTimeoutMs() {
    if (sleepIndex >= sleepOptionCount) return 0;
    return sleepTimes[sleepIndex];
}

const char* sleepSettingLabel() {
    if (sleepIndex >= sleepOptionCount) return "OFF";
    return sleepLabels[sleepIndex];
}

const char* settingsBlurb() {
    static char buf[16];
    snprintf(buf, sizeof(buf), "SLEEP %s", sleepSettingLabel());
    return buf;
}

void saveSettings() {
    Preferences p;
    p.begin("cfg", false);
    p.putUChar("bl", brightnessPct);
    p.putUChar("sl", sleepIndex);
    p.putUChar("sv", saverIndex);
    p.putChar("bt", (char)bootTool);
    p.putUChar("rt", rotFlip);
    p.end();
}

void loadSettings() {
    Preferences p;
    p.begin("cfg", false);
    brightnessPct = p.getUChar("bl", 100);
    sleepIndex = p.getUChar("sl", 0);
    saverIndex = p.getUChar("sv", 2);
    bootTool = (int8_t)p.getChar("bt", -1);
    rotFlip = p.getUChar("rt", 0);
    p.end();

    if (brightnessPct < 10 || brightnessPct > 100) brightnessPct = 100;
    if (sleepIndex >= sleepOptionCount) sleepIndex = 0;
    if (saverIndex >= saverOptionCount) saverIndex = 2;
    if (bootTool < -1 || bootTool >= menuItemCount) bootTool = -1;
}

void drawSettings(bool full) {
    const char* metas[6];
    uint16_t accents[] = {WHITE, AMBER, AMBER, GREEN, DIM_GREEN, GRAY};

#if BL_PIN >= 0
    char blBuf[8];
    snprintf(blBuf, sizeof(blBuf), "%u%%", brightnessPct);
    metas[0] = blBuf;
#else
    metas[0] = "N/A";
#endif
    metas[1] = saverLabels[saverIndex];
    metas[2] = sleepLabels[sleepIndex];
    metas[3] = (bootTool < 0) ? "HOME" : menuItems[bootTool].name;
    metas[4] = rotFlip ? "180" : "0";
    metas[5] = nullptr;

    if (full) {
        const char* sel = (settingsIndex == settingsCount - 1) ? "BACK" : "SET";
        drawShell("SETUP", "NEXT", sel, settingsIndex, settingsCount);
    } else {
        drawFooterCount(settingsIndex, settingsCount);
    }
    drawMenuList(settingsNames, accents, metas, settingsCount, settingsIndex, 15);
}

void enterSettings() {
    screen = SCREEN_SETTINGS;
    settingsIndex = 0;
    drawSettings(true);
}

void settingsNext() {
    settingsIndex = (settingsIndex + 1) % settingsCount;
    drawSettings(true);
}

void cycleBrightness() {
#if BL_PIN < 0
    drawDialog("SETUP", AMBER, "NO BACKLIGHT", "PIN NOT WIRED", "SET BL_PIN IN CONFIG", -1);
    delay(1400);
    drawSettings(true);
    return;
#else
    int next = 0;
    for (int i = 0; i < (int)(sizeof(brightnessSteps) / sizeof(brightnessSteps[0])); i++) {
        if (brightnessPct < brightnessSteps[i]) {
            next = i;
            break;
        }
        next = 0;
    }
    brightnessPct = brightnessSteps[next];
    applyBrightness();
    saveSettings();
    drawSettings(false);
#endif
}

void settingsSelect() {
    switch (settingsIndex) {
        case 0:
            cycleBrightness();
            break;
        case 1:
            saverIndex = (saverIndex + 1) % saverOptionCount;
            saveSettings();
            drawSettings(false);
            break;
        case 2:
            sleepIndex = (sleepIndex + 1) % sleepOptionCount;
            saveSettings();
            drawSettings(false);
            break;
        case 3:
            bootTool++;
            if (bootTool >= menuItemCount) bootTool = -1;
            saveSettings();
            drawSettings(false);
            break;
        case 4:
            rotFlip ^= 1;
            saveSettings();
            tft.setRotation(rotFlip ? 3 : 1);
            drawSettings(true);
            break;
        default:
            screen = SCREEN_MENU;
            drawMainMenu(true);
            break;
    }
}
