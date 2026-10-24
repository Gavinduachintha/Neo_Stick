// =====================================================
// SYSTEM — live diagnostics
// =====================================================

unsigned long lastSystemTick = 0;

const char* resetName() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "POWER ON";
        case ESP_RST_SW:        return "SOFTWARE";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT WDT";
        case ESP_RST_TASK_WDT:  return "TASK WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEP SLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

void formatStaMac(char* out, size_t n) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void sysRow(int x, int y, const char* label, const char* value, uint16_t color) {
    tft.setTextColor(GRAY);
    tft.setCursor(x, y);
    tft.print(label);
    tft.fillRect(x + 66, y, 120, 8, PANEL);
    tft.setTextColor(color);
    tft.setCursor(x + 66, y);
    tft.print(value);
}

void drawSystemRows() {
    int x = 16;
    int y = 20;
    int w = 208;
    int h = 94;

    tft.fillRoundRect(x, y, w, h, 4, PANEL);
    tft.drawRoundRect(x, y, w, h, 4, DIM_GREEN);
    tft.setTextSize(1);

    char buf[20];
    int row = y + 8;

    snprintf(buf, sizeof(buf), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
    sysRow(x + 10, row, "HEAP", buf, WHITE);
    row += 12;

    unsigned long s = millis() / 1000;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (s / 3600) % 100, (s / 60) % 60, s % 60);
    sysRow(x + 10, row, "UPTIME", buf, GREEN);
    row += 12;

    snprintf(buf, sizeof(buf), "%u MHZ", (unsigned)ESP.getCpuFreqMHz());
    sysRow(x + 10, row, "CPU", buf, WHITE);
    row += 12;

    snprintf(buf, sizeof(buf), "%.1f C", temperatureRead());
    sysRow(x + 10, row, "TEMP", buf, AMBER);
    row += 12;

    snprintf(buf, sizeof(buf), "%u MB", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
    sysRow(x + 10, row, "FLASH", buf, WHITE);
    row += 12;

    sysRow(x + 10, row, "RESET", resetName(), WHITE);
    row += 12;

    formatStaMac(buf, sizeof(buf));
    sysRow(x + 10, row, "MAC", buf, DIM_GREEN);

    lastSystemTick = millis();
}

void enterSystem() {
    screen = SCREEN_SYSTEM;
    drawShell("SYSTEM", "REFRESH", "BACK", 0, 0);
    drawSystemRows();
}

void systemTick() {
    if (millis() - lastSystemTick < 1000) return;
    drawSystemRows();
}
