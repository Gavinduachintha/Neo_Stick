// =====================================================
// TIMER — stopwatch and countdown
// =====================================================

const char* timerMenuNames[] = {"STOPWATCH", "COUNTDOWN", "BACK"};
const int timerMenuCount = sizeof(timerMenuNames) / sizeof(timerMenuNames[0]);
const int countChoices[] = {1, 5, 10, 15, 30, 60};
const int countChoiceCount = sizeof(countChoices) / sizeof(countChoices[0]);

int timerMenuIndex = 0;
int countChoice = 1;
bool timerCountdown = false;
bool timerRunning = false;
bool timerDone = false;
unsigned long timerAccum = 0;
unsigned long timerMark = 0;
unsigned long timerLastDraw = 0;

bool timerBusy() {
    return timerRunning;
}

unsigned long timerDurationMs() {
    return (unsigned long)countChoices[countChoice] * 60000UL;
}

// Fills showMs with what the face should display. Stops a finished countdown.
unsigned long timerShowMs() {
    unsigned long elapsed = timerAccum;
    if (timerRunning) elapsed += millis() - timerMark;

    if (!timerCountdown) return elapsed;

    unsigned long cap = timerDurationMs();
    if (elapsed >= cap) {
        timerRunning = false;
        timerAccum = cap;
        timerDone = true;
        return 0;
    }
    return cap - elapsed;
}

void drawTimerMenu(bool full) {
    uint16_t accents[] = {CYAN, AMBER, GRAY};
    const char* metas[] = {nullptr, nullptr, nullptr};
    if (full) {
        drawShell("TIMER", "NEXT", "OPEN", timerMenuIndex, timerMenuCount);
    } else {
        drawFooterCount(timerMenuIndex, timerMenuCount);
    }
    drawMenuList(timerMenuNames, accents, metas, timerMenuCount, timerMenuIndex, 20);
}

void enterTimerMenu() {
    screen = SCREEN_TIMER_MENU;
    timerMenuIndex = 0;
    drawTimerMenu(true);
}

void timerMenuNext() {
    timerMenuIndex = (timerMenuIndex + 1) % timerMenuCount;
    drawTimerMenu(false);
}

void startTimerFace(bool countdown) {
    timerCountdown = countdown;
    timerRunning = false;
    timerDone = false;
    timerAccum = 0;
    timerMark = millis();
    screen = SCREEN_TIMER_RUN;
    drawTimerFace(true);
}

void timerMenuSelect() {
    if (timerMenuIndex == 0) startTimerFace(false);
    else if (timerMenuIndex == 1) drawTimerSetup();
    else {
        screen = SCREEN_MENU;
        drawMainMenu(true);
    }
}

void drawTimerSetup() {
    screen = SCREEN_TIMER_SETUP;
    drawShell("TIMER", "MINS", "START", 0, 0);

    tft.setTextSize(1);
    tft.setTextColor(GRAY);
    tft.setCursor(84, 28);
    tft.print("COUNTDOWN");

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", countChoices[countChoice]);
    tft.fillRect(0, 44, 240, 36, BG);
    tft.setTextSize(4);
    tft.setTextColor(AMBER);
    int w = (int)strlen(buf) * 24;
    tft.setCursor((240 - w) / 2, 46);
    tft.print(buf);

    tft.setTextSize(1);
    tft.setTextColor(DIM_GREEN);
    tft.setCursor(96, 90);
    tft.print("MINUTES");
}

void timerSetupNext() {
    countChoice = (countChoice + 1) % countChoiceCount;
    drawTimerSetup();
}

void timerSetupSelect() {
    startTimerFace(true);
}

void drawTimerFace(bool full) {
    unsigned long show = timerShowMs();
    bool pausedWithTime = !timerRunning && !timerDone && timerAccum > 0;

    if (full) {
        const char* btn = "START";
        const char* sel = "BACK";
        if (timerRunning) btn = "STOP";
        else if (timerDone) btn = "RESET";
        else if (pausedWithTime) {
            btn = "START";
            sel = "RESET";
        }
        drawShell("TIMER", btn, sel, 0, 0);

        tft.setTextSize(1);
        tft.setTextColor(timerDone ? AMBER : GRAY);
        const char* cap = "STOPWATCH";
        if (timerDone) cap = "TIME UP";
        else if (timerCountdown) cap = "COUNTDOWN";
        tft.setCursor((240 - (int)strlen(cap) * 6) / 2, 22);
        tft.print(cap);
    }

    unsigned long tenths = (show / 100) % 10;
    unsigned secs = (show / 1000) % 60;
    unsigned mins = (show / 60000UL) % 100;
    char big[6];
    snprintf(big, sizeof(big), "%02u:%02u", mins, secs);

    int x = 42;
    tft.fillRect(x, 42, 160, 34, BG);
    tft.setTextSize(4);
    tft.setTextColor(timerDone ? AMBER : (timerRunning ? GREEN : WHITE));
    tft.setCursor(x, 42);
    tft.print(big);

    tft.fillRect(x + 126, 58, 14, 16, BG);
    tft.setTextSize(2);
    tft.setTextColor(DIM_GREEN);
    tft.setCursor(x + 126, 58);
    tft.print((int)tenths);

    timerLastDraw = millis();
}

void timerRunNext() {
    if (timerDone) {
        timerDone = false;
        timerAccum = 0;
        timerRunning = false;
        drawTimerFace(true);
        return;
    }
    if (timerRunning) {
        timerAccum += millis() - timerMark;
        timerRunning = false;
    } else {
        timerMark = millis();
        timerRunning = true;
    }
    drawTimerFace(true);
}

void timerRunSelect() {
    bool pausedWithTime = !timerRunning && !timerDone && timerAccum > 0;
    if (pausedWithTime) {
        timerAccum = 0;
        drawTimerFace(true);
        return;
    }
    timerRunning = false;
    screen = SCREEN_TIMER_MENU;
    drawTimerMenu(true);
}

void timerTick() {
    if (!timerRunning && !timerDone) return;
    if (millis() - timerLastDraw < 100) return;
    bool wasDone = timerDone;
    drawTimerFace(false);
    if (timerDone && !wasDone) drawTimerFace(true);
}
