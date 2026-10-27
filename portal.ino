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
    page.reserve(2800);
    page += F("<!DOCTYPE html><html><head><meta name=viewport content=\"width=device-width,initial-scale=1\">"
              "<title>NEOSTICK</title><style>"
              ":root{color-scheme:dark;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;background:#10161b;color:#d7ffe9}"
              "*{box-sizing:border-box}body{margin:0;min-height:100vh;padding:22px 14px;background:radial-gradient(circle at 50% 0,#27363d,#10161b 52%);display:flex;justify-content:center;align-items:flex-start}"
              ".device{width:min(100%,390px);margin:0 auto;padding:10px;border:1px solid #52616a;border-radius:28px;background:#202a30;box-shadow:0 18px 45px #050708,inset 0 1px #748087}"
              ".speaker{width:54px;height:5px;margin:2px auto 9px;border-radius:5px;background:#0b1013;box-shadow:inset 0 1px #465158}"
              ".screen{padding:18px 16px 16px;border:5px solid #0a0e10;border-radius:17px;background:#07120f;box-shadow:inset 0 0 0 1px #294239,inset 0 0 28px #061b13}"
              ".top{display:flex;justify-content:space-between;align-items:center;padding-bottom:12px;border-bottom:1px solid #294239;color:#5fbc8a;font-size:12px;letter-spacing:.16em}"
              ".led{display:inline-block;width:7px;height:7px;margin-right:5px;border-radius:50%;background:#5fbc8a;box-shadow:0 0 8px #5fbc8a;vertical-align:middle}h1{margin:20px 0 4px;font-size:22px;letter-spacing:.16em;color:#d7ffe9}"
              ".subtitle{margin:0 0 18px;color:#78968a;font-size:12px;line-height:1.6}.label{display:block;margin:16px 0 5px;color:#5fbc8a;font-size:11px;letter-spacing:.12em}"
              "input,button{width:100%;font:inherit;font-size:14px;border-radius:7px}input{padding:12px;border:1px solid #315044;background:#0b1c16;color:#fff;outline:none}input:focus{border-color:#5fbc8a;box-shadow:0 0 0 2px #5fbc8a33}"
              "button{margin-top:9px;padding:12px;border:1px solid #5fbc8a;background:#1c6b45;color:#f0fff7;font-weight:700;letter-spacing:.08em;box-shadow:inset 0 1px #76d39c;cursor:pointer}button:active{transform:translateY(1px);box-shadow:none}"
              ".saved{margin:22px 0 8px;padding-top:14px;border-top:1px dashed #294239}.saved-title{color:#5fbc8a;font-size:11px;letter-spacing:.12em}.empty{color:#78968a;font-size:12px}.network{margin-top:7px}.network button{margin-top:0;background:#13251e;border-color:#315044;box-shadow:none;text-align:left;color:#d7ffe9;font-weight:400;letter-spacing:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
              ".actions{margin-top:20px;padding-top:14px;border-top:1px solid #294239}.actions button{background:#182d25}.actions form:last-child button{border-color:#745c28;background:#342d18;color:#ffe9a8;box-shadow:none}.foot{margin:17px 0 0;color:#526e63;font-size:10px;line-height:1.6;text-align:center}"
              ".feet{height:8px;width:90px;margin:10px auto 0;border-radius:0 0 8px 8px;background:#10161b}"
              "</style></head><body><main class=device><div class=speaker></div><section class=screen>"
              "<div class=top><span><i class=led></i>ONLINE</span><span>SETUP / 01</span></div>"
              "<h1>NEOSTICK</h1><p class=subtitle>POCKET NETWORK CONSOLE<br>LOCAL CONTROL INTERFACE</p>"
              "<form method=POST action=/save><label class=label>NEW NETWORK</label>"
              "<input name=ssid maxlength=32 placeholder=NETWORK required>"
              "<input name=pass maxlength=64 type=password placeholder=PASSWORD>"
              "<button type=submit>SAVE TO DEVICE</button></form>");

    page += F("<div class=saved><div class=saved-title>SAVED NETWORKS</div>");
    if (savedCount == 0) {
        page += F("<p class=empty>NO NETWORKS STORED</p>");
    }
    for (int i = 0; i < savedCount; i++) {
        page += F("<form class=network method=POST action=/forget><input type=hidden name=i value=");
        page += i;
        page += F("><button type=submit>FORGET / ");
        appendEscaped(page, savedNets[i].ssid);
        page += F("</button></form>");
    }

    page += F("</div><div class=actions><form method=POST action=/connect><button type=submit>CONNECT STRONGEST</button></form>");
    page += bleBeacon
        ? F("<form method=POST action=/beacon><button type=submit>BEACON OFF</button></form>")
        : F("<form method=POST action=/beacon><button type=submit>BEACON ON</button></form>");
    page += F("<form method=POST action=/reboot><button type=submit>REBOOT DEVICE</button></form>"
              "</div><p class=foot>JOIN OPEN NETWORK NEOSTICK<br>192.168.4.1 / NO PASSWORD</p>"
              "</section><div class=feet></div></main></body></html>");
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
        "<!DOCTYPE html><html><meta name=viewport content=\"width=device-width,initial-scale=1\"><style>body{margin:0;padding:24px;background:#10161b;color:#d7ffe9;font:14px monospace;text-align:center}main{max-width:360px;margin:auto;padding:32px 20px;border:1px solid #52616a;border-radius:20px;background:#07120f;box-shadow:0 12px 30px #050708}h1{color:#5fbc8a;letter-spacing:.16em}p{color:#78968a;line-height:1.6}a{display:block;margin-top:24px;padding:12px;border:1px solid #5fbc8a;border-radius:7px;color:#d7ffe9;text-decoration:none}</style><main>"
        "<h1>SAVED</h1><p>Password stored on the device.</p><a href=/>RETURN TO CONSOLE</a></main></html>");
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
        "<!DOCTYPE html><meta name=viewport content=\"width=device-width,initial-scale=1\"><style>body{margin:0;padding:24px;background:#10161b;color:#d7ffe9;font:14px monospace;text-align:center}main{max-width:360px;margin:auto;padding:32px 20px;border:1px solid #52616a;border-radius:20px;background:#07120f;box-shadow:0 12px 30px #050708}h1{color:#fdc84b;letter-spacing:.16em}p{color:#78968a;line-height:1.6}</style><main>"
        "<h1>JOINING</h1><p>The setup network will close. Reconnect your phone to your own Wi-Fi.</p></main></html>");
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
