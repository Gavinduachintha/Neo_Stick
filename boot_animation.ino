// =====================================================
// BOOT SCREEN (240 × 135)
// =====================================================

void drawBootChips(int step) {
    const char* names[] = {"BOOT", "DISP", "UI"};
    int x = 36;

    tft.fillRect(0, 110, 240, 14, BG);
    tft.setTextSize(1);
    for (int i = 0; i < 3; i++) {
        tft.setCursor(x, 116);
        tft.setTextColor(GRAY);
        tft.print("[");
        tft.setTextColor(i <= step ? GREEN : DARK_GRAY);
        tft.print(names[i]);
        tft.setTextColor(GRAY);
        tft.print("]");
        x += 62;
    }
}

void drawBootLogo() {
    tft.fillScreen(BG);
    tft.setTextSize(1);
    tft.setTextColor(DIM_GREEN);
    tft.setCursor(6, 6);
    tft.print("XIAO ESP32-S3");
    tft.setCursor(234 - (int)strlen(FW_VERSION) * 6, 6);
    tft.print(FW_VERSION);
    tft.drawFastHLine(0, 18, 240, DARK_GRAY);

    tft.setTextSize(2);
    tft.setTextColor(GREEN);
    tft.setCursor((240 - (int)strlen(DEVICE_NAME) * 12) / 2, 32);
    tft.print(DEVICE_NAME);

    tft.setTextSize(1);
    tft.setTextColor(GRAY);
    tft.setCursor(93, 56);
    tft.print("POCKET OS");

    tft.drawRoundRect(20, 74, 200, 10, 2, PANEL_HI);
    tft.setTextColor(GRAY);
    tft.setCursor(20, 92);
    tft.print("WAKE");
    drawBootChips(-1);
}

void bootAnimation() {
    const char* messages[] = {
        "DISPLAY",
        "CONTROLS",
        "MENUS",
        "READY"
    };

    for (int i = 0; i < 4; i++) {
        int width = (i + 1) * 49;
        tft.fillRect(22, 76, width, 6, GREEN);

        tft.fillRect(20, 92, 200, 10, BG);
        tft.setTextSize(1);
        tft.setCursor(20, 92);
        tft.setTextColor(GRAY);
        tft.print(messages[i]);

        drawBootChips(i > 2 ? 2 : i);
        delay(280);
    }

    delay(180);
}