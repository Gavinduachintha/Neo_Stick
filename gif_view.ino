// =====================================================
// GIF — play an animated GIF from the SD card
// Frames land in a 240×135 buffer, then go to the display.
// =====================================================

#include <esp_heap_caps.h>

static const int GIF_W = 240;
static const int GIF_H = 135;

File gifFile;
bool gifPlaying = false;
bool gifSingle = false;
uint32_t gifDataPos = 0;
uint32_t gifNextAt = 0;
int gifDelayMs = 100;
int gifLogicW = 0;
int gifLogicH = 0;
int gifBg = 0;
int gifTrans = -1;
int gifFrames = 0;
int gifOx = 0, gifOy = 0, gifOutW = GIF_W, gifOutH = GIF_H;
int gifDisposal = 1;

static uint16_t gifRd16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint16_t gifPal[256];
int gifPalCount = 0;
uint16_t* gifCanvas = nullptr;

struct GifDictEnt {
    int16_t prefix;
    uint8_t suffix;
};
GifDictEnt gifDict[4096];

uint8_t gifBlock[255];
int gifBlockLen = 0;
int gifBlockPos = 0;
uint32_t gifBitBuf = 0;
int gifBitCnt = 0;
bool gifDataEnded = false;

void gifStop() {
    gifPlaying = false;
    gifSingle = false;
    if (gifFile) gifFile.close();
    if (gifCanvas) {
        free(gifCanvas);
        gifCanvas = nullptr;
    }
}

static uint16_t gif565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static bool gifReadPal(int count) {
    if (count < 1 || count > 256) return false;
    uint8_t rgb[3];
    for (int i = 0; i < count; i++) {
        if (gifFile.read(rgb, 3) != 3) return false;
        gifPal[i] = gif565(rgb[0], rgb[1], rgb[2]);
    }
    gifPalCount = count;
    return true;
}

static void gifSkipBlocks() {
    for (;;) {
        int n = gifFile.read();
        if (n <= 0) return;
        if (!gifFile.seek(gifFile.position() + (uint32_t)n)) return;
    }
}

static void gifResetBits() {
    gifBlockLen = 0;
    gifBlockPos = 0;
    gifBitBuf = 0;
    gifBitCnt = 0;
    gifDataEnded = false;
}

static int gifReadCode(int bits) {
    while (gifBitCnt < bits) {
        if (gifBlockPos >= gifBlockLen) {
            int n = gifFile.read();
            if (n <= 0) {
                gifDataEnded = true;
                return -1;
            }
            if (gifFile.read(gifBlock, n) != n) return -1;
            gifBlockLen = n;
            gifBlockPos = 0;
        }
        gifBitBuf |= (uint32_t)gifBlock[gifBlockPos++] << gifBitCnt;
        gifBitCnt += 8;
    }
    int code = (int)(gifBitBuf & ((1u << bits) - 1));
    gifBitBuf >>= bits;
    gifBitCnt -= bits;
    return code;
}

static void gifFinishData() {
    gifBlockPos = gifBlockLen;
    if (gifDataEnded) return;
    gifSkipBlocks();
}

static void gifBlit() {
    if (!gifCanvas) return;
    for (int y = 0; y < GIF_H; y++) {
        tft.drawRGBBitmap(0, y, gifCanvas + y * GIF_W, GIF_W, 1);
    }
}

static void gifFillRect(int x, int y, int w, int h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GIF_W) w = GIF_W - x;
    if (y + h > GIF_H) h = GIF_H - y;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h; row++) {
        uint16_t* p = gifCanvas + row * GIF_W + x;
        for (int col = 0; col < w; col++) p[col] = color;
    }
}

static uint8_t gifFirst(int code, int clear) {
    int guard = 0;
    while (code >= clear && guard++ < 4096) code = gifDict[code].prefix;
    return (uint8_t)code;
}

// One image descriptor. Transparent pixels are left alone.
static bool gifDecodeImage() {
    uint8_t desc[9];
    if (gifFile.read(desc, 9) != 9) return false;
    int left = gifRd16(desc);
    int top = gifRd16(desc + 2);
    int fw = gifRd16(desc + 4);
    int fh = gifRd16(desc + 6);
    uint8_t packed = desc[8];
    if (fw <= 0 || fh <= 0 || fw > 1024 || fh > 1024) return false;

    uint16_t savedPal[256];
    int savedCount = gifPalCount;
    memcpy(savedPal, gifPal, sizeof(savedPal));
    if (packed & 0x80) {
        int n = 1 << ((packed & 7) + 1);
        if (!gifReadPal(n)) return false;
    }

    bool interlace = packed & 0x40;
    int minSize = gifFile.read();
    if (minSize < 2 || minSize > 11) return false;

    int clear = 1 << minSize;
    int eoi = clear + 1;
    int codeSize = minSize + 1;
    int nextCode = eoi + 1;
    int prev = -1;
    gifResetBits();

    static const int passStart[] = {0, 4, 2, 1};
    static const int passStep[] = {8, 8, 4, 2};
    int pass = 0;
    int rowInPass = 0;
    int sx = 0;
    int sy = 0;
    int emitted = 0;
    int total = fw * fh;
    static uint8_t stack[4096];
    auto plot = [&](uint8_t idx) {
        if (emitted >= total) return;
        int px = sx;
        int py = sy;
        emitted++;
        if (++sx >= fw) {
            sx = 0;
            if (!interlace) sy++;
            else {
                rowInPass++;
                sy = passStart[pass] + rowInPass * passStep[pass];
                while (sy >= fh && pass < 3) {
                    pass++;
                    rowInPass = 0;
                    sy = passStart[pass];
                }
            }
        }
        if (idx == gifTrans || py >= fh || py < 0 || idx >= gifPalCount) return;
        int x0 = gifOx + (int)((int64_t)(left + px) * gifOutW / gifLogicW);
        int x1 = gifOx + (int)((int64_t)(left + px + 1) * gifOutW / gifLogicW);
        int y0 = gifOy + (int)((int64_t)(top + py) * gifOutH / gifLogicH);
        int y1 = gifOy + (int)((int64_t)(top + py + 1) * gifOutH / gifLogicH);
        if (x1 > x0 && y1 > y0) gifFillRect(x0, y0, x1 - x0, y1 - y0, gifPal[idx]);
    };

    if (gifDisposal == 2) {
        int x0 = gifOx + (int)((int64_t)left * gifOutW / gifLogicW);
        int y0 = gifOy + (int)((int64_t)top * gifOutH / gifLogicH);
        int x1 = gifOx + (int)((int64_t)(left + fw) * gifOutW / gifLogicW);
        int y1 = gifOy + (int)((int64_t)(top + fh) * gifOutH / gifLogicH);
        uint16_t bg = (gifBg >= 0 && gifBg < savedCount) ? savedPal[gifBg] : BG;
        gifFillRect(x0, y0, x1 - x0, y1 - y0, bg);
    }
    gifDisposal = 1;

    for (;;) {
        int code = gifReadCode(codeSize);
        if (code < 0) break;
        if (code == clear) {
            codeSize = minSize + 1;
            nextCode = eoi + 1;
            prev = -1;
            continue;
        }
        if (code == eoi) break;

        if (prev < 0) {
            if (code < clear) plot((uint8_t)code);
            prev = code;
            continue;
        }

        int outCode = code;
        uint8_t first;
        int sp = 0;
        if (code > nextCode || code > 4095) break;
        if (code == nextCode) {
            first = gifFirst(prev, clear);
            code = prev;
            stack[sp++] = first;
        } else {
            first = gifFirst(code, clear);
        }
        int guard = 0;
        while (code >= clear && guard++ < 4096) {
            stack[sp++] = gifDict[code].suffix;
            code = gifDict[code].prefix;
            if (sp >= 4096) break;
        }
        if (sp < 4096) stack[sp++] = (uint8_t)code;
        while (sp > 0) plot(stack[--sp]);

        if (nextCode < 4096) {
            gifDict[nextCode].prefix = (int16_t)prev;
            gifDict[nextCode].suffix = first;
            nextCode++;
            if (nextCode == (1 << codeSize) && codeSize < 12) codeSize++;
        }
        prev = outCode;
    }

    gifFinishData();
    memcpy(gifPal, savedPal, sizeof(gifPal));
    gifPalCount = savedCount;
    gifFrames++;
    return true;
}

static bool gifStep() {
    for (;;) {
        int b = gifFile.read();
        if (b < 0 || b == 0x3B) {
            if (gifFrames <= 1) {
                gifSingle = true;
                return gifFrames == 1;
            }
            gifFrames = 0;
            if (!gifFile.seek(gifDataPos)) return false;
            continue;
        }
        if (b == 0x21) {
            int label = gifFile.read();
            if (label == 0xF9) {
                int n = gifFile.read();
                uint8_t g[8];
                if (n < 4 || n > 8 || gifFile.read(g, n) != n) return false;
                int disposal = (g[0] >> 2) & 7;
                gifDelayMs = gifRd16(g + 1) * 10;
                if (gifDelayMs < 20) gifDelayMs = 100;
                gifTrans = (g[0] & 1) ? g[3] : -1;
                gifDisposal = disposal;
                int term = gifFile.read();
                if (term > 0) gifSkipBlocks();
            } else {
                gifSkipBlocks();
            }
            continue;
        }
        if (b == 0x2C) {
            if (!gifDecodeImage()) return false;
            gifBlit();
            return true;
        }
        return false;
    }
}

bool gifStart(const char* path) {
    gifStop();
    if (!sdMounted && !sdBegin()) return false;
    gifFile = SD.open(path);
    if (!gifFile) return false;

    uint8_t hdr[13];
    if (gifFile.read(hdr, 13) != 13 || memcmp(hdr, "GIF", 3) != 0) {
        gifFile.close();
        return false;
    }
    gifLogicW = gifRd16(hdr + 6);
    gifLogicH = gifRd16(hdr + 8);
    if (gifLogicW <= 0 || gifLogicH <= 0 || gifLogicW > 1024 || gifLogicH > 1024) {
        gifFile.close();
        return false;
    }
    uint8_t packed = hdr[10];
    gifBg = hdr[11];
    gifPalCount = 0;
    if (packed & 0x80) {
        int n = 1 << ((packed & 7) + 1);
        if (!gifReadPal(n)) {
            gifFile.close();
            return false;
        }
    }

    bool wider = (int64_t)gifLogicW * GIF_H > (int64_t)gifLogicH * GIF_W;
    gifOutW = GIF_W;
    gifOutH = GIF_H;
    if (wider) gifOutH = (int)((int64_t)gifLogicH * GIF_W / gifLogicW);
    else gifOutW = (int)((int64_t)gifLogicW * GIF_H / gifLogicH);
    if (gifOutW < 1) gifOutW = 1;
    if (gifOutH < 1) gifOutH = 1;
    gifOx = (GIF_W - gifOutW) / 2;
    gifOy = (GIF_H - gifOutH) / 2;

    gifCanvas = (uint16_t*)heap_caps_malloc(GIF_W * GIF_H * sizeof(uint16_t), MALLOC_CAP_8BIT);
    if (!gifCanvas) {
        gifFile.close();
        return false;
    }
    uint16_t bg = (gifBg >= 0 && gifBg < gifPalCount) ? gifPal[gifBg] : BG;
    gifFillRect(0, 0, GIF_W, GIF_H, bg);

    gifDataPos = gifFile.position();
    gifFrames = 0;
    gifTrans = -1;
    gifSingle = false;
    gifDelayMs = 100;
    if (!gifStep()) {
        gifStop();
        return false;
    }
    gifPlaying = true;
    gifNextAt = millis() + gifDelayMs;
    return true;
}

void sdImageTick() {
    if (!gifPlaying || gifSingle) return;
    if ((int32_t)(millis() - gifNextAt) < 0) return;
    if (!gifStep()) {
        gifStop();
        return;
    }
    gifNextAt = millis() + gifDelayMs;
}
