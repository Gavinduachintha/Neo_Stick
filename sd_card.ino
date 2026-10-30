// =====================================================
// SD — card info, file browser, text preview
// Shares the display SPI bus; CS on GPIO44 (D7), MISO on GPIO8 (D9).
// =====================================================

const int SD_MAX = 24;
const int SD_PAGE = 5;
const int SD_ROW_Y = 34;
const int SD_ROW_H = 14;

struct SdEntry {
    char name[33];
    uint32_t size;
    bool dir;
};

SdEntry sdEntries[SD_MAX];
int sdCount = 0;
int sdCursor = 0;
char sdPath[96] = "/";

bool sdBegin() {
    SD.end();
    sdMounted = SD.begin(SD_CS, mySPI, 10000000);
    if (sdMounted && SD.cardType() == CARD_NONE) {
        SD.end();
        sdMounted = false;
    }
    return sdMounted;
}

void formatBytes(uint64_t b, char* out, size_t n) {
    if (b >= 1024ULL * 1024 * 1024) snprintf(out, n, "%.1fG", b / 1073741824.0);
    else if (b >= 1024ULL * 1024) snprintf(out, n, "%.1fM", b / 1048576.0);
    else if (b >= 1024) snprintf(out, n, "%.1fK", b / 1024.0);
    else snprintf(out, n, "%uB", (unsigned)b);
}

const char* sdTypeName() {
    switch (SD.cardType()) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SD";
        case CARD_SDHC: return "SDHC";
        default:        return "?";
    }
}

bool sdAtRoot() {
    return strcmp(sdPath, "/") == 0;
}

void sdLoadDir() {
    sdCount = 0;
    File dir = SD.open(sdPath);
    if (!dir || !dir.isDirectory()) return;

    File f = dir.openNextFile();
    while (f && sdCount < SD_MAX) {
        const char* nm = f.name();
        const char* slash = strrchr(nm, '/');
        if (slash) nm = slash + 1;
        if (nm[0] != '.') {
            SdEntry &e = sdEntries[sdCount++];
            strncpy(e.name, nm, sizeof(e.name) - 1);
            e.name[sizeof(e.name) - 1] = '\0';
            e.dir = f.isDirectory();
            e.size = e.dir ? 0 : (uint32_t)f.size();
        }
        f.close();
        f = dir.openNextFile();
    }
    if (f) f.close();
    dir.close();
}

// Rows: entries, then BACK. BACK climbs one folder, or leaves at the root.
int sdRowCount() {
    return sdCount + 1;
}

void drawSdNoCard() {
    drawShell("SD", "RETRY", "BACK", 0, 0);
    tft.setTextSize(2);
    tft.setTextColor(AMBER);
    tft.setCursor(66, 44);
    tft.print("NO CARD");
    tft.setTextSize(1);
    tft.setTextColor(GRAY);
    tft.setCursor(30, 72);
    tft.print("CHECK WIRING / FAT32 FORMAT");
}

void drawSdHeader() {
    tft.fillRect(0, 16, 240, 16, BG);
    tft.setTextSize(1);

    char total[10], used[10];
    formatBytes(SD.totalBytes(), total, sizeof(total));
    formatBytes(SD.usedBytes(), used, sizeof(used));

    tft.setTextColor(GREEN);
    tft.setCursor(6, 20);
    tft.print(sdTypeName());
    tft.setTextColor(GRAY);
    tft.print(" ");
    tft.print(used);
    tft.print("/");
    tft.print(total);

    // Path, trimmed from the left so the current folder stays visible.
    const char* p = sdPath;
    int maxChars = 18;
    int len = (int)strlen(p);
    tft.setTextColor(DIM_GREEN);
    if (len > maxChars) {
        p += len - maxChars + 2;
        tft.setCursor(234 - maxChars * 6, 20);
        tft.print("..");
    } else {
        tft.setCursor(234 - len * 6, 20);
    }
    tft.print(p);
}

void drawSdRows() {
    tft.fillRect(0, SD_ROW_Y, 240, SD_PAGE * SD_ROW_H, BG);
    tft.setTextSize(1);

    int rows = sdRowCount();
    int first = (sdCursor / SD_PAGE) * SD_PAGE;
    for (int i = 0; i < SD_PAGE && first + i < rows; i++) {
        int idx = first + i;
        int y = SD_ROW_Y + i * SD_ROW_H;
        bool on = (idx == sdCursor);

        if (on) {
            tft.fillRoundRect(6, y, 228, SD_ROW_H - 2, 3, PANEL_HI);
            tft.fillRect(8, y + 2, 2, SD_ROW_H - 6, GREEN);
        }

        if (idx == sdCount) {
            tft.setTextColor(on ? WHITE : GRAY);
            tft.setCursor(16, y + 3);
            tft.print(sdAtRoot() ? "BACK" : "UP ..");
            continue;
        }

        SdEntry &e = sdEntries[idx];
        tft.setTextColor(e.dir ? AMBER : (on ? WHITE : GRAY));
        tft.setCursor(16, y + 3);
        char shown[25];
        strncpy(shown, e.name, 24);
        shown[24] = '\0';
        tft.print(shown);
        if (e.dir) tft.print("/");

        char sz[10];
        if (e.dir) strcpy(sz, "DIR");
        else formatBytes(e.size, sz, sizeof(sz));
        tft.setTextColor(e.dir ? AMBER : DIM_GREEN);
        tft.setCursor(228 - (int)strlen(sz) * 6, y + 3);
        tft.print(sz);
    }

    drawFooterCount(sdCursor, rows);
}

void drawSdBrowser() {
    screen = SCREEN_SD;
    drawShell("SD", "NEXT", "OPEN", sdCursor, sdRowCount());
    drawSdHeader();
    if (sdCount == 0) {
        tft.setTextColor(GRAY);
        tft.setCursor(16, SD_ROW_Y + SD_PAGE * SD_ROW_H - 12);
        tft.print("EMPTY FOLDER");
    }
    drawSdRows();
}

void enterSd() {
    screen = SCREEN_SD;
    if (!sdMounted && !sdBegin()) {
        drawSdNoCard();
        return;
    }
    strcpy(sdPath, "/");
    sdCursor = 0;
    sdLoadDir();
    drawSdBrowser();
}

void sdNext() {
    if (!sdMounted) {
        enterSd();
        return;
    }
    sdCursor = (sdCursor + 1) % sdRowCount();
    drawSdRows();
}

void sdChildPath(const char* name, char* out, size_t n) {
    if (sdAtRoot()) snprintf(out, n, "/%s", name);
    else snprintf(out, n, "%s/%s", sdPath, name);
}

void sdLeave() {
    screen = SCREEN_MENU;
    drawMainMenu(true);
}

void sdSelect() {
    if (!sdMounted) {
        sdLeave();
        return;
    }

    if (sdCursor == sdCount) {
        if (sdAtRoot()) {
            sdLeave();
            return;
        }
        char* slash = strrchr(sdPath, '/');
        if (slash == sdPath) sdPath[1] = '\0';
        else *slash = '\0';
        sdCursor = 0;
        sdLoadDir();
        drawSdBrowser();
        return;
    }

    SdEntry &e = sdEntries[sdCursor];
    char full[sizeof(sdPath)];
    sdChildPath(e.name, full, sizeof(full));

    if (e.dir) {
        strncpy(sdPath, full, sizeof(sdPath) - 1);
        sdPath[sizeof(sdPath) - 1] = '\0';
        sdCursor = 0;
        sdLoadDir();
        drawSdBrowser();
    } else if (sdExtIs(e.name, ".bmp") || sdExtIs(e.name, ".gif")) {
        openSdImage(full);
    } else {
        drawSdView(full);
    }
}

bool sdExtIs(const char* name, const char* ext) {
    int n = (int)strlen(name);
    int e = (int)strlen(ext);
    if (n < e) return false;
    for (int i = 0; i < e; i++) {
        if (tolower((unsigned char)name[n - e + i]) != ext[i]) return false;
    }
    return true;
}

void openSdImage(const char* path) {
    gifStop();
    screen = SCREEN_SD_VIEW;
    tft.fillScreen(BG);
    bool ok = false;
    if (sdExtIs(path, ".bmp")) ok = drawBmpFile(path, 240, 135, false);
    else ok = gifStart(path);
    if (!ok) {
        tft.setTextSize(1);
        tft.setTextColor(AMBER);
        tft.setCursor(48, 60);
        tft.print("CANNOT SHOW IMAGE");
    }
}

// Shows the first lines of a file as text. Non-printable bytes become dots.
void drawSdView(const char* path) {
    screen = SCREEN_SD_VIEW;
    drawShell("SD / VIEW", nullptr, "BACK", 0, 0);

    File f = SD.open(path);
    tft.setTextSize(1);
    if (!f) {
        tft.setTextColor(AMBER);
        tft.setCursor(6, 22);
        tft.print("CANNOT OPEN");
        return;
    }

    const int cols = 38;
    const int lines = 10;
    char line[cols + 1];
    int y = 20;
    for (int l = 0; l < lines && f.available(); l++) {
        int n = 0;
        while (f.available() && n < cols) {
            int c = f.read();
            if (c == '\n') break;
            if (c == '\r') continue;
            line[n++] = (c >= 32 && c < 127) ? (char)c : '.';
        }
        line[n] = '\0';
        tft.setTextColor(l % 2 ? GRAY : WHITE);
        tft.setCursor(6, y);
        tft.print(line);
        y += 10;
    }
    f.close();
}

void sdViewSelect() {
    gifStop();
    drawSdBrowser();
}

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Draws a 24- or 32-bit BMP into dw×dh.
// cover crops to fill the rectangle. Otherwise the whole image is letterboxed.
bool drawBmpFile(const char* path, int dw, int dh, bool cover) {
    if (!sdMounted && !sdBegin()) return false;
    File f = SD.open(path);
    if (!f) return false;

    uint8_t hdr[54];
    if (f.read(hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        f.close();
        return false;
    }

    uint32_t off = rd32(hdr + 10);
    uint32_t dib = rd32(hdr + 14);
    int32_t w = (int32_t)rd32(hdr + 18);
    int32_t h = (int32_t)rd32(hdr + 22);
    uint16_t bpp = rd16(hdr + 28);
    uint32_t comp = rd32(hdr + 30);
    bool bottomUp = h > 0;
    if (h < 0) h = -h;

    bool ok = dib >= 40 && w > 0 && h > 0 && w <= 2048 && h <= 2048 &&
              (bpp == 24 || bpp == 32) && (comp == 0 || comp == 3) &&
              dw > 0 && dh > 0;
    int px = bpp / 8;
    int stride = ok ? ((w * px + 3) & ~3) : 0;
    uint8_t* row = ok ? (uint8_t*)malloc(stride) : nullptr;
    if (!row) {
        f.close();
        return false;
    }

    if (dw > 240) dw = 240;
    bool wider = (int64_t)w * dh > (int64_t)h * dw;
    int outW = dw, outH = dh, ox = 0, oy = 0;
    int visW = w, visH = h, x0 = 0, y0 = 0;
    if (cover) {
        visW = wider ? (int)((int64_t)h * dw / dh) : w;
        visH = wider ? h : (int)((int64_t)w * dh / dw);
        if (visW < 1) visW = 1;
        if (visH < 1) visH = 1;
        if (visW > w) visW = w;
        if (visH > h) visH = h;
        x0 = (w - visW) / 2;
        y0 = (h - visH) / 2;
    } else {
        tft.fillScreen(BG);
        if (wider) outH = (int)((int64_t)h * dw / w);
        else outW = (int)((int64_t)w * dh / h);
        if (outW < 1) outW = 1;
        if (outH < 1) outH = 1;
        if (outW > dw) outW = dw;
        if (outH > dh) outH = dh;
        ox = (dw - outW) / 2;
        oy = (dh - outH) / 2;
    }

    uint16_t line[240];
    int lastSy = -1;
    bool failed = false;

    for (int y = 0; y < outH && !failed; y++) {
        int sy = y0 + (int)((int64_t)y * visH / outH);
        if (sy >= h) sy = h - 1;
        if (sy != lastSy) {
            int fileRow = bottomUp ? (h - 1 - sy) : sy;
            if (!f.seek(off + (uint32_t)fileRow * (uint32_t)stride) ||
                f.read(row, stride) != stride) {
                failed = true;
                break;
            }
            lastSy = sy;
        }
        for (int x = 0; x < outW; x++) {
            int sx = x0 + (int)((int64_t)x * visW / outW);
            if (sx >= w) sx = w - 1;
            const uint8_t* p = row + sx * px;
            uint8_t b = p[0], g = p[1], r = p[2];
            line[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        tft.drawRGBBitmap(ox, oy + y, line, outW, 1);
    }

    free(row);
    f.close();
    return !failed;
}
