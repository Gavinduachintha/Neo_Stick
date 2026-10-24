// =====================================================
// SETUP AP — phone page for saved networks and a few actions
// =====================================================

#include <WebServer.h>
#include <DNSServer.h>

WebServer portalServer(80);
DNSServer portalDns;
bool portalOn = false;
bool portalRoutes = false;
bool portalJoin = false;
bool portalReboot = false;
unsigned long portalDrawn = 0;
int portalClients = -1;

const IPAddress PORTAL_IP(192, 168, 4, 1);

void appendEscaped(String &out, const char* s) {
    for (; *s; s++) {
        if (*s == '&') out += F("&amp;");
        else if (*s == '<') out += F("&lt;");
        else if (*s == '>') out += F("&gt;");
        else if (*s == '"') out += F("&quot;");
        else out += *s;
    }
}

void sendPortalPage() {
    String page;
    page.reserve(1400);
    page += F("<!DOCTYPE html><html><head><meta name=viewport content=\"width=device-width,initial-scale=1\">"
              "<title>NEOSTICK</title><style>"
              "body{font-family:sans-serif;background:#000;color:#d7ffe9;margin:0;padding:16px}"
              "h1{font-size:18px;letter-spacing:.18em;color:#5fbc8a}"
              "input,button{width:100%;box-sizing:border-box;margin:6px 0;padding:12px;border:0;border-radius:8px;font-size:16px}"
              "input{background:#102018;color:#fff}"
              "button{background:#1c6b45;color:#fff;font-weight:700}"
              "a{color:#5fbc8a}"
              "small{color:#8aa}"
              "</style></head><body><h1>NEOSTICK</h1>"
              "<p>Save a network. The password stays on this device.</p>"
              "<form method=POST action=/save>"
              "<input name=ssid maxlength=32 placeholder=Network required>"
              "<input name=pass maxlength=64 type=password placeholder=Password>"
              "<button type=submit>SAVE</button></form>");

    page += F("<p><small>SAVED</small></p>");
    if (savedCount == 0) {
        page += F("<p><small>None yet.</small></p>");
    }
    for (int i = 0; i < savedCount; i++) {
        page += F("<form method=POST action=/forget><input type=hidden name=i value=");
        page += i;
        page += F("><button type=submit>FORGET ");
        appendEscaped(page, savedNets[i].ssid);
        page += F("</button></form>");
    }

    page += F("<form method=POST action=/connect><button type=submit>CONNECT STRONGEST</button></form>");
    page += bleBeacon
        ? F("<form method=POST action=/beacon><button type=submit>BEACON OFF</button></form>")
        : F("<form method=POST action=/beacon><button type=submit>BEACON ON</button></form>");
    page += F("<form method=POST action=/reboot><button type=submit>REBOOT</button></form>"
              "<p><small>Join the open network NEOSTICK, then this page opens.</small></p>"
              "</body></html>");
    portalServer.send(200, "text/html", page);
}

void sendRedirectHome() {
    portalServer.sendHeader("Location", "http://192.168.4.1/", true);
    portalServer.send(302, "text/plain", "");
}

void handlePortalSave() {
    String ssid = portalServer.arg("ssid");
    String pass = portalServer.arg("pass");
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
        portalServer.send(400, "text/plain", "Network name is missing or too long.");
        return;
    }
    addSavedNet(ssid.c_str(), pass.c_str());
    Serial.print("Saved network from setup page: ");
    Serial.println(ssid);
    portalServer.send(200, "text/html",
        "<!DOCTYPE html><html><body style=background:#000;color:#d7ffe9;font-family:sans-serif;padding:16px>"
        "<h1>SAVED</h1><p>Password stored on the device.</p><a href=/>Back</a></body></html>");
}

void handlePortalForget() {
    int idx = portalServer.arg("i").toInt();
    removeSavedNet(idx);
    sendPortalPage();
}

void handlePortalConnect() {
    if (savedCount == 0) {
        portalServer.send(400, "text/plain", "Save a network first.");
        return;
    }
    portalJoin = true;
    portalServer.send(200, "text/html",
        "<!DOCTYPE html><html><body style=background:#000;color:#d7ffe9;font-family:sans-serif;padding:16px>"
        "<h1>JOINING</h1><p>The setup network will close. Reconnect your phone to your own Wi-Fi.</p></body></html>");
}

void handlePortalBeacon() {
    setBeacon(!bleBeacon);
    sendPortalPage();
}

void handlePortalReboot() {
    portalReboot = true;
    portalServer.send(200, "text/plain", "Rebooting.");
}

void drawPortal(bool full) {
    int clients = WiFi.softAPgetStationNum();
    if (!full && clients == portalClients) return;
    portalClients = clients;

    if (full) {
        drawShell("SETUP", nullptr, "STOP", 0, 0);
        tft.setTextSize(1);
        tft.setTextColor(GRAY);
        tft.setCursor(16, 28);
        tft.print("JOIN");
        tft.setTextColor(WHITE);
        tft.setCursor(70, 28);
        tft.print(DEVICE_NAME);

        tft.setTextColor(GRAY);
        tft.setCursor(16, 46);
        tft.print("OPEN");
        tft.setTextColor(GREEN);
        tft.setCursor(70, 46);
        tft.print("NO PASSWORD");

        tft.setTextColor(GRAY);
        tft.setCursor(16, 64);
        tft.print("PAGE");
        tft.setTextColor(WHITE);
        tft.setCursor(70, 64);
        tft.print("192.168.4.1");
    } else {
        tft.fillRect(70, 80, 80, 12, BG);
    }

    tft.setTextSize(1);
    tft.setTextColor(GRAY);
    tft.setCursor(16, 82);
    tft.print("PHONES");
    tft.setTextColor(clients > 0 ? GREEN : DIM_GREEN);
    tft.setCursor(70, 82);
    tft.print(clients);
    portalDrawn = millis();
}

void startPortal() {
    stopPacketMonitor();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(PORTAL_IP, PORTAL_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(DEVICE_NAME);
    wifiConnected = false;

    portalDns.start(53, "*", PORTAL_IP);
    if (!portalRoutes) {
        portalServer.on("/", HTTP_GET, sendPortalPage);
        portalServer.on("/save", HTTP_POST, handlePortalSave);
        portalServer.on("/forget", HTTP_POST, handlePortalForget);
        portalServer.on("/connect", HTTP_POST, handlePortalConnect);
        portalServer.on("/beacon", HTTP_POST, handlePortalBeacon);
        portalServer.on("/reboot", HTTP_POST, handlePortalReboot);
        portalServer.onNotFound(sendRedirectHome);
        portalRoutes = true;
    }
    portalServer.begin();

    portalOn = true;
    portalJoin = false;
    portalReboot = false;
    portalClients = -1;
    screen = SCREEN_WIFI_PORTAL;
    drawPortal(true);
    Serial.println("Setup AP ready at http://192.168.4.1/");
}

void stopPortal() {
    if (!portalOn) return;
    portalDns.stop();
    portalServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    portalOn = false;
    wifiConnected = (WiFi.status() == WL_CONNECTED);
}

void portalTick() {
    if (!portalOn) return;
    portalDns.processNextRequest();
    portalServer.handleClient();
    touchActivity();

    if (portalReboot) {
        delay(300);
        ESP.restart();
    }
    if (portalJoin) {
        portalJoin = false;
        stopPortal();
        connectWIFI();
        return;
    }
    if (millis() - portalDrawn >= 1000) drawPortal(false);
}
