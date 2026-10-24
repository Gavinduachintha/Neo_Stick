// =====================================================
// PACKET MONITOR — frames per second, per channel.
// Counts only. Addresses and contents are not stored.
// =====================================================

#include <esp_wifi.h>

volatile uint16_t pktHits[14];
uint16_t pktRate[14];
uint16_t pktHist[40];
uint8_t pktHistN = 0;
bool pktOn = false;
uint8_t pktFocus = 0;          // 0 = hop every channel, 1..13 = stay
uint8_t pktHop = 1;
unsigned long pktHopAt = 0;
unsigned long pktDrawAt = 0;

void IRAM_ATTR onSniff(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type == WIFI_PKT_MISC) return;
    const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
    uint8_t ch = pkt->rx_ctrl.channel;
    if (ch < 1 || ch > 13) return;
    if (pktHits[ch] < 65000) pktHits[ch]++;
}

void snapshotPackets() {
    noInterrupts();
    for (int i = 0; i < 14; i++) {
        pktRate[i] = pktHits[i];
        pktHits[i] = 0;
    }
    interrupts();
}

void drawPacketPlot(bool full) {
    if (full) {
        const char* sel = "BACK";
        drawShell("AIR", pktFocus == 0 ? "LOCK" : "NEXT", sel, 0, 0);
    }
    tft.fillRect(0, 16, 240, 102, BG);
    tft.setTextSize(1);

    uint16_t total = 0;
    int hot = 1;
    for (int c = 1; c <= 13; c++) {
        total += pktRate[c];
        if (pktRate[c] > pktRate[hot]) hot = c;
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "%u FPS", total);
    tft.setTextColor(GREEN);
    tft.setCursor(16, 18);
    tft.print(buf);

    if (pktFocus == 0) snprintf(buf, sizeof(buf), "HOP  HOT %d", hot);
    else snprintf(buf, sizeof(buf), "LOCKED CH %u", pktFocus);
    tft.setTextColor(GRAY);
    tft.setCursor(224 - (int)strlen(buf) * 6, 18);
    tft.print(buf);

    // Sparkline of total frames/s
    uint16_t peak = 1;
    int histCount = pktHistN < 40 ? pktHistN : 40;
    for (int i = 0; i < histCount; i++) {
        if (pktHist[i] > peak) peak = pktHist[i];
    }
    int base = 46;
    tft.drawFastHLine(16, base, 208, DARK_GRAY);
    for (int i = 0; i < histCount; i++) {
        int h = (pktHist[i] * 14) / peak;
        if (pktHist[i] > 0 && h < 1) h = 1;
        int x = 16 + (40 - histCount + i) * 5;
        if (h > 0) tft.drawFastVLine(x, base - h, h, DIM_GREEN);
    }

    // Per-channel bars. While hopping, each bar is what that channel
    // collected during its slice of the last second.
    const int barBase = 104;
    const int maxH = 36;
    const int x0 = 24;
    const int pitch = 15;
    const int barW = 11;
    uint16_t barPeak = 1;
    for (int c = 1; c <= 13; c++) {
        if (pktRate[c] > barPeak) barPeak = pktRate[c];
    }
    tft.drawFastHLine(x0 - 4, barBase, 13 * pitch + 4, DARK_GRAY);

    for (int c = 1; c <= 13; c++) {
        int x = x0 + (c - 1) * pitch;
        int n = pktRate[c];
        int h = (n == 0) ? 0 : (n * maxH) / barPeak;
        if (n > 0 && h < 2) h = 2;
        bool locked = (pktFocus == c);
        uint16_t color = locked ? GREEN : (c == hot && n > 0 ? AMBER : DIM_GREEN);
        if (h > 0) tft.fillRect(x, barBase - h, barW, h, color);
        else tft.drawFastHLine(x, barBase - 1, barW, DARK_GRAY);

        snprintf(buf, sizeof(buf), "%d", c);
        tft.setTextColor(locked ? GREEN : DIM_GREEN);
        tft.setCursor(x + (barW - (int)strlen(buf) * 6) / 2, barBase + 3);
        tft.print(buf);
    }
    pktDrawAt = millis();
}

void startPacketMonitor() {
    stopPortal();
    for (int i = 0; i < 14; i++) pktHits[i] = 0;
    memset(pktRate, 0, sizeof(pktRate));
    memset(pktHist, 0, sizeof(pktHist));
    pktHistN = 0;
    pktFocus = 0;
    pktHop = 1;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    wifiConnected = false;

    wifi_promiscuous_filter_t filt = {};
    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL;
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(&onSniff);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    pktOn = true;
    pktHopAt = millis();
    screen = SCREEN_WIFI_PKTS;
    drawPacketPlot(true);
    Serial.println("Packet monitor on (counts only)");
}

void stopPacketMonitor() {
    if (!pktOn) return;
    esp_wifi_set_promiscuous(false);
    pktOn = false;
    wifiConnected = (WiFi.status() == WL_CONNECTED);
}

void packetNext() {
    pktFocus = (pktFocus + 1) % 14;
    if (pktFocus >= 1) esp_wifi_set_channel(pktFocus, WIFI_SECOND_CHAN_NONE);
    drawPacketPlot(true);
}

void packetTick() {
    if (!pktOn) return;
    unsigned long now = millis();

    if (pktFocus == 0 && now - pktHopAt >= 70) {
        pktHopAt = now;
        pktHop++;
        if (pktHop > 13) pktHop = 1;
        esp_wifi_set_channel(pktHop, WIFI_SECOND_CHAN_NONE);
    }

    if (now - pktDrawAt >= 1000) {
        snapshotPackets();
        uint16_t total = 0;
        for (int c = 1; c <= 13; c++) total += pktRate[c];
        if (pktHistN < 40) {
            pktHist[pktHistN++] = total;
        } else {
            memmove(pktHist, pktHist + 1, sizeof(pktHist) - sizeof(pktHist[0]));
            pktHist[39] = total;
        }
        drawPacketPlot(false);
        touchActivity();
    }
}
