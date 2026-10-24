// =====================================================
// POWER — backlight, idle clock, deep sleep
// =====================================================

void touchActivity() {
    lastActivity = millis();
}

void initBacklight() {
#if BL_PIN >= 0
    ledcAttach(BL_PIN, 5000, 8);
    applyBrightness();
#else
    (void)brightnessPct;
#endif
}

void applyBrightness() {
#if BL_PIN >= 0
    uint8_t duty = (uint16_t)brightnessPct * 255 / 100;
    if (duty < 8) duty = 8;
    ledcWrite(BL_PIN, duty);
#endif
}

// Screensaver first (lock screen clock), then deep sleep if that is armed.
// Torch and a running timer are left alone.
void idleTick() {
    if (screen == SCREEN_FLASH || timerBusy()) return;

    unsigned long idle = millis() - lastActivity;
    unsigned long saver = saverTimeoutMs();
    unsigned long sleepFor = sleepTimeoutMs();

    if (!locked && saver > 0 && idle >= saver) {
        lockScreen();
        return;
    }
    if (sleepFor > 0 && idle >= sleepFor && (locked || saver == 0)) {
        enterDeepSleep();
    }
}

void enterDeepSleep() {
    tft.fillScreen(BG);
    tft.setTextSize(1);
    tft.setTextColor(GREEN);
    tft.setCursor(102, 52);
    tft.print("SLEEP");
    tft.setTextColor(GRAY);
    tft.setCursor(66, 70);
    tft.print("PRESS BTN TO WAKE");
    delay(350);
    tft.enableDisplay(false);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    if (BLEDevice::getInitialized()) {
        BLEDevice::deinit(false);
    }

    gpio_num_t wake = (gpio_num_t)BTN_PIN;
    rtc_gpio_init(wake);
    rtc_gpio_set_direction(wake, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(wake);
    rtc_gpio_pulldown_dis(wake);
    esp_sleep_enable_ext0_wakeup(wake, 0);
    esp_deep_sleep_start();
}
