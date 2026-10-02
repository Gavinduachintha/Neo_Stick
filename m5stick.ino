#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "config.h"

// =====================================================
// TFT PINS — XIAO ESP32-S3
// =====================================================
#define TFT_SCLK 7
#define TFT_MOSI 9
#define TFT_RST  4
#define TFT_DC   5
#define TFT_CS   6

// Button pin (adjust to your actual button pin)
#define BTN_PIN  1

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
    uint16_t color;
};

// Define menu items
MenuItem menuItems[] = {
    {"FLASHER", CYAN},
    {"WIFI", GREEN},
    {"BLE", AMBER},
    {"SETTINGS", WHITE},
    {"SYSTEM", DIM_GREEN},
    {"ABOUT", GRAY}
};

const int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);
int currentMenuItem = 0;
bool buttonPressed = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;



// =====================================================
// BOOT SCREEN (240 × 135)
// =====================================================

void drawBootLogo() {
    tft.fillScreen(BG);
    // Top bar
    tft.setTextSize(1);
    tft.setTextColor(DIM_GREEN);
    tft.setCursor(4, 3);
    tft.print("XIAO-ESP32S3");
    tft.setCursor(175, 3);
    tft.print("16MB");
    tft.drawFastHLine(4, 14, 232, DARK_GRAY);

    // Logo
    tft.setTextColor(GREEN);
    tft.setTextSize(3);
    tft.setCursor(55, 28);
    tft.print("GAVI");

    tft.setTextColor(CYAN);
    tft.setTextSize(1);
    tft.setCursor(55, 55);
    tft.print("CARDPUTER // OS 0.1");

    // Progress box
    tft.drawRect(20, 75, 200, 10, PANEL_HI);

    // Status text
    tft.setTextColor(GRAY);
    tft.setCursor(20, 92);
    tft.print("INITIALIZING");

    // Status indicators
    tft.setCursor(20, 110);
    tft.setTextColor(GRAY);
    tft.print("[");
    tft.setTextColor(GREEN);
    tft.print("BOOT");
    tft.setTextColor(GRAY);
    tft.print("]");

    tft.setCursor(75, 110);
    tft.print("[");
    tft.setTextColor(GREEN);
    tft.print("DISP");
    tft.setTextColor(GRAY);
    tft.print("]");

    tft.setCursor(130, 110);
    tft.print("[");
    tft.setTextColor(AMBER);
    tft.print("SYS");
    tft.setTextColor(GRAY);
    tft.print("]");
}


void bootAnimation() {

    const char* messages[] = {
        "INIT DISPLAY",
        "CHECK MEMORY",
        "LOAD UI",
        "START SYSTEM"
    };

    for (int i = 0; i < 4; i++) {

        // Clear bar
        tft.fillRect(22, 77, 196, 6, BG);

        // Progress
        int width = (i + 1) * 49;
        tft.fillRect(22, 77, width, 6, GREEN);

        // Message
        tft.fillRect(20, 92, 200, 10, BG);
        tft.setCursor(20, 92);
        tft.setTextColor(GRAY);
        tft.print(messages[i]);

        for (int j = 0; j < 3; j++) {
            tft.print(".");
            delay(80);
        }

        delay(220);
    }

    delay(200);
}


// =====================================================
// HOME SCREEN (240 × 135)
// =====================================================

void drawHome() {
    tft.fillScreen(BG);

    // Header
    tft.setTextSize(1);
    tft.setTextColor(GREEN);
    tft.setCursor(4, 3);
    tft.print("GAVI OS");

    tft.setTextColor(GRAY);
    tft.setCursor(60, 3);
    tft.print("// CARDPUTER");

    tft.setTextColor(DIM_GREEN);
    tft.setCursor(175, 3);
    tft.print("S3");

    tft.drawFastHLine(4, 14, 232, DARK_GRAY);

    // Footer
    tft.drawFastHLine(4, 122, 232, DARK_GRAY);

    tft.setTextColor(GRAY);
    tft.setCursor(4, 126);
    tft.print("BTN");

    tft.setTextColor(WHITE);
    tft.print(" NEXT");

    tft.setTextColor(CYAN);
    tft.setCursor(90, 126);
    tft.print("[");
    tft.print(currentMenuItem + 1);
    tft.print("/");
    tft.print(menuItemCount);
    tft.print("]");

    tft.setTextColor(DIM_GREEN);
    tft.setCursor(200, 126);
    tft.print("v0.1");
}


// =====================================================
// MENU ITEM DISPLAY (Centered, Large)
// =====================================================

void drawCenteredMenuItem(int index) {
    // Clear the content area (between header and footer)
    tft.fillRect(4, 16, 232, 105, BG);

    MenuItem item = menuItems[index];

    // Calculate center positions
    int panelWidth = 180;
    int panelHeight = 70;
    int panelX = (240 - panelWidth) / 2;
    int panelY = (135 - panelHeight) / 2 - 5; // Slightly up from center

    // Draw main panel
    tft.fillRoundRect(panelX, panelY, panelWidth, panelHeight, 6, PANEL);
    tft.drawRoundRect(panelX, panelY, panelWidth, panelHeight, 6, item.color);
    tft.drawRoundRect(panelX + 1, panelY + 1, panelWidth - 2, panelHeight - 2, 5, PANEL_HI);

    // Draw item number indicator
    tft.setTextSize(1);
    tft.setTextColor(GRAY);
    tft.setCursor(panelX + 8, panelY + 8);
    tft.print("#");
    tft.print(index + 1);

    // Draw accent line
    tft.fillRect(panelX + 8, panelY + 22, panelWidth - 16, 2, item.color);

    // Draw menu item name (centered, large)
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    
    // Calculate text centering
    int textWidth = strlen(item.name) * 12; // Approximate width for size 2
    int textX = panelX + (panelWidth - textWidth) / 2;
    int textY = panelY + 35;
    
    tft.setCursor(textX, textY);
    tft.print(item.name);

    // Draw selection indicator arrows
    tft.setTextSize(2);
    tft.setTextColor(item.color);
    tft.setCursor(panelX + 10, textY);
    tft.print(">");
    tft.setCursor(panelX + panelWidth - 22, textY);
    tft.print("<");
}


// =====================================================
// OLD MENU ITEM (compact) - kept for reference
// =====================================================

void drawMenuItem(
    int x,
    int y,
    const char* number,
    const char* name,
    uint16_t color
) {

    tft.fillRoundRect(x, y, 114, 28, 3, PANEL);
    tft.drawRoundRect(x, y, 114, 28, 3, PANEL_HI);

    tft.setTextSize(1);

    // Number
    tft.setTextColor(GRAY);
    tft.setCursor(x + 5, y + 6);
    tft.print(number);

    // Accent
    tft.fillRect(x + 24, y + 5, 2, 10, color);

    // Name
    tft.setTextColor(WHITE);
    tft.setCursor(x + 32, y + 6);
    tft.print(name);

    // Arrow
    tft.setTextColor(GRAY);
    tft.setCursor(x + 100, y + 6);
    tft.print(">");
}


// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);

    // Setup button pin
    pinMode(BTN_PIN, INPUT_PULLUP);

    mySPI.begin(
        TFT_SCLK,
        -1,
        TFT_MOSI,
        TFT_CS
    );

    // Correct init for 135×240
    tft.init(135, 240);

    // Landscape → 240 × 135
    tft.setRotation(1);

    tft.fillScreen(BG);

    drawBootLogo();
    bootAnimation();
    drawHome();
    
    // Draw the first menu item
    drawCenteredMenuItem(currentMenuItem);
}


// =====================================================
// LOOP
// =====================================================

void loop() {
    // Read button state (active LOW with pull-up)
    bool buttonState = (digitalRead(BTN_PIN) == LOW);
    if (digitalRead(BTN_PIN) == LOW) {
        Serial.println("PRESSED");
    } else {
        Serial.println("RELEASED");
    }

    delay(100);
    
    // Button press detection with debounce
    if (buttonState && !buttonPressed && (millis() - lastButtonPress > debounceDelay)) {
        buttonPressed = true;
        lastButtonPress = millis();
        
        // Move to next menu item
        currentMenuItem++;
        if (currentMenuItem >= menuItemCount) {
            currentMenuItem = 0; // Wrap around to first item
        }
        
        // Update display
        drawCenteredMenuItem(currentMenuItem);
        
        // Update footer counter
        tft.fillRect(90, 126, 50, 8, BG);
        tft.setTextSize(1);
        tft.setTextColor(CYAN);
        tft.setCursor(90, 126);
        tft.print("[");
        tft.print(currentMenuItem + 1);
        tft.print("/");
        tft.print(menuItemCount);
        tft.print("]");
        
        Serial.print("Menu item: ");
        Serial.println(menuItems[currentMenuItem].name);
    }
    
    // Reset button state when released
    if (!buttonState) {
        buttonPressed = false;
    }
    
    delay(10); // Small delay to prevent excessive polling
}