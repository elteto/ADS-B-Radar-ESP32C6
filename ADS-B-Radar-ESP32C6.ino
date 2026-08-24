#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <math.h>

// ======================================================
// CONFIGURACION GENERAL
// ======================================================

const int SEARCH_LEVELS = 5;
const unsigned long UPDATE_INTERVAL = 30000;
const unsigned long MANUAL_SCREEN_DURATION = 15000;
const unsigned long STARTUP_DISPLAY_DURATION = 60000;
const unsigned long ALERT_DURATION = 30000;
const unsigned long BLINK_INTERVAL = 400;
const unsigned long CONFIG_BUTTON_HOLD = 3000;
const unsigned long PANEL_ROTATION_INTERVAL = 10000;

const double PASS_NEAR_KM = 10.0;
const double MAX_CPA_HOURS = 2.0;

// ======================================================
// COLORES RGB565
// ======================================================

#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define CYAN        0x07FF
#define YELLOW      0xFFE0
#define GREY        0x8410
#define DARKGREY    0x4208

// ======================================================
// WAVESHARE ESP32-C6-LCD-1.47
// ======================================================

#define LCD_SCK      7
#define LCD_MOSI     6
#define LCD_CS       14
#define LCD_DC       15
#define LCD_RST      21
#define LCD_BL       22
#define RGB_LED      8
#define BUTTON_BOOT  9

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED
);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus, LCD_RST, 1, true, 172, 320, 34, 0, 34, 0
);

// ======================================================
// CONFIGURACION PERSISTENTE
// ======================================================

Preferences preferences;

struct DeviceConfig {
  String ssid1;
  String pass1;
  String ssid2;
  String pass2;
  String ssid3;
  String pass3;
  double latitude;
  double longitude;
  int baseRadiusNM;
  bool configured;
};

DeviceConfig config;

const char* CONFIG_AP_SSID = "ADS-B-Radar-Setup";
const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer webServer(80);

// ======================================================
// DATOS DE AVIONES
// ======================================================

struct Aircraft {
  String callsign;
  String hex;
  String type;
  String origin;
  String destination;
  double lat;
  double lon;
  double distanceKm;
  double bearing;
  double altitude;
  double speed;
  double track;
  bool cpaValid;
  bool approaching;
  double closestApproachKm;
  double minutesToClosest;
};

#define MAX_AIRCRAFT 30
Aircraft aircraft[MAX_AIRCRAFT];
int aircraftCount = 0;
int featuredAircraftIndex = -1;

// ======================================================
// ESTADOS
// ======================================================

unsigned long lastUpdate = 0;
bool baseHasAircraft = false;
bool hadBaseAircraft = false;

bool manualMode = false;
int manualLevel = 0;
unsigned long manualScreenStart = 0;

bool startupMode = true;
bool startupFoundAircraft = false;
unsigned long startupDisplayStart = 0;

int displayedRadiusNM = 15;

bool alertBlinking = false;
bool ledState = false;
unsigned long alertStart = 0;
unsigned long lastBlink = 0;

// Vista derecha: false = clasica, true = proximidad
bool showProximityPanel = false;
unsigned long lastPanelRotation = 0;

String lastRouteCallsign = "";
String lastOrigin = "";
String lastDestination = "";

// ======================================================
// UTILIDADES
// ======================================================

double degToRad(double deg) { return deg * PI / 180.0; }
double radToDeg(double rad) { return rad * 180.0 / PI; }

double getDistanceKm(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;
  double dLat = degToRad(lat2 - lat1);
  double dLon = degToRad(lon2 - lon1);

  double a =
    sin(dLat / 2.0) * sin(dLat / 2.0) +
    cos(degToRad(lat1)) * cos(degToRad(lat2)) *
    sin(dLon / 2.0) * sin(dLon / 2.0);

  return R * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

double getBearing(double lat1, double lon1, double lat2, double lon2) {
  double p1 = degToRad(lat1);
  double p2 = degToRad(lat2);
  double dLon = degToRad(lon2 - lon1);

  double y = sin(dLon) * cos(p2);
  double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dLon);

  return fmod(radToDeg(atan2(y, x)) + 360.0, 360.0);
}

int radiusForLevel(int level) {
  int radius = config.baseRadiusNM * (1 << level);
  if (radius > 250) radius = 250;
  return radius;
}

String formatEta(double minutes) {
  if (minutes < 0) return "---";
  int totalMinutes = (int)round(minutes);
  if (totalMinutes < 60) return String(totalMinutes) + "m";

  int h = totalMinutes / 60;
  int m = totalMinutes % 60;
  if (m == 0) return String(h) + "h";
  return String(h) + "h" + String(m) + "m";
}

// ======================================================
// CPA - MAXIMA APROXIMACION
// ======================================================

void calculateClosestApproach(Aircraft &a) {
  a.cpaValid = false;
  a.approaching = false;
  a.closestApproachKm = a.distanceKm;
  a.minutesToClosest = 0;

  if (a.speed < 30.0) return;

  double bearingRad = degToRad(a.bearing);
  double trackRad = degToRad(a.track);

  double rx = a.distanceKm * sin(bearingRad);
  double ry = a.distanceKm * cos(bearingRad);

  double vx = a.speed * sin(trackRad);
  double vy = a.speed * cos(trackRad);

  double v2 = vx * vx + vy * vy;
  if (v2 < 1.0) return;

  double tHours = -((rx * vx) + (ry * vy)) / v2;
  a.cpaValid = true;

  if (tHours <= 0.0 || tHours > MAX_CPA_HOURS) return;

  double cx = rx + vx * tHours;
  double cy = ry + vy * tHours;

  a.closestApproachKm = sqrt(cx * cx + cy * cy);
  a.minutesToClosest = tHours * 60.0;
  a.approaching = true;
}

void chooseFeaturedAircraft() {
  featuredAircraftIndex = -1;
  if (aircraftCount <= 0) return;

  double bestCPA = 1e9;
  double bestTime = 1e9;

  for (int i = 0; i < aircraftCount; i++) {
    Aircraft &a = aircraft[i];
    if (!a.cpaValid || !a.approaching) continue;

    if (a.closestApproachKm < bestCPA - 0.1 ||
        (fabs(a.closestApproachKm - bestCPA) <= 0.1 &&
         a.minutesToClosest < bestTime)) {
      bestCPA = a.closestApproachKm;
      bestTime = a.minutesToClosest;
      featuredAircraftIndex = i;
    }
  }

  if (featuredAircraftIndex < 0) {
    double bestDistance = 1e9;
    for (int i = 0; i < aircraftCount; i++) {
      if (aircraft[i].distanceKm < bestDistance) {
        bestDistance = aircraft[i].distanceKm;
        featuredAircraftIndex = i;
      }
    }
  }
}

// ======================================================
// PANTALLA / RGB
// ======================================================

void screenOn() { digitalWrite(LCD_BL, HIGH); }
void screenOff() { digitalWrite(LCD_BL, LOW); }
void rgbOn() { rgbLedWrite(RGB_LED, 255, 0, 0); }
void rgbOff() { rgbLedWrite(RGB_LED, 0, 0, 0); }

void startAircraftAlert() {
  alertBlinking = true;
  alertStart = millis();
  lastBlink = millis();
  ledState = true;
  rgbOn();
}

void updateAircraftAlert() {
  if (!alertBlinking) return;

  if (millis() - alertStart >= ALERT_DURATION) {
    alertBlinking = false;
    rgbOff();
    return;
  }

  if (millis() - lastBlink >= BLINK_INTERVAL) {
    lastBlink = millis();
    ledState = !ledState;
    if (ledState) rgbOn(); else rgbOff();
  }
}

// ======================================================
// PREFERENCES
// ======================================================

void loadConfig() {
  preferences.begin("adsbradar", true);

  config.ssid1 = preferences.getString("ssid1", "");
  config.pass1 = preferences.getString("pass1", "");
  config.ssid2 = preferences.getString("ssid2", "");
  config.pass2 = preferences.getString("pass2", "");
  config.ssid3 = preferences.getString("ssid3", "");
  config.pass3 = preferences.getString("pass3", "");
  config.latitude = preferences.getDouble("lat", -31.380000);
  config.longitude = preferences.getDouble("lon", -57.980000);
  config.baseRadiusNM = preferences.getInt("radius", 15);
  config.configured = preferences.getBool("configured", false);

  preferences.end();

  if (config.baseRadiusNM < 1) config.baseRadiusNM = 15;
  if (config.baseRadiusNM > 250) config.baseRadiusNM = 250;
  displayedRadiusNM = config.baseRadiusNM;
}

void saveConfig() {
  preferences.begin("adsbradar", false);

  preferences.putString("ssid1", config.ssid1);
  preferences.putString("pass1", config.pass1);
  preferences.putString("ssid2", config.ssid2);
  preferences.putString("pass2", config.pass2);
  preferences.putString("ssid3", config.ssid3);
  preferences.putString("pass3", config.pass3);
  preferences.putDouble("lat", config.latitude);
  preferences.putDouble("lon", config.longitude);
  preferences.putInt("radius", config.baseRadiusNM);
  preferences.putBool("configured", true);

  preferences.end();
}

// ======================================================
// PORTAL CAUTIVO
// ======================================================

String htmlEscape(const String &value) {
  String out = value;
  out.replace("&", "&amp;");
  out.replace("\"", "&quot;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  return out;
}

String buildConfigPage() {
  String html;
  html.reserve(7000);

  html += "<!DOCTYPE html><html lang='es'><head>";
  html += "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>ADS-B Radar Setup</title><style>";
  html += "body{font-family:Arial;background:#101318;color:#eee;margin:0;padding:20px}";
  html += ".card{max-width:520px;margin:auto;background:#1b2028;padding:22px;border-radius:14px}";
  html += "h1{color:#4dd0e1}label{display:block;margin-top:14px;margin-bottom:5px;font-weight:bold}";
  html += "input{box-sizing:border-box;width:100%;padding:12px;border-radius:8px;border:1px solid #3b4552;background:#0e1116;color:#fff;font-size:16px}";
  html += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}";
  html += "button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:9px;background:#00acc1;color:white;font-size:17px;font-weight:bold}";
  html += ".small{font-size:13px;color:#8794a3}@media(max-width:520px){.grid{grid-template-columns:1fr}}";
  html += "</style></head><body><div class='card'><h1>ADS-B Radar</h1>";
  html += "<p>Configura Wi-Fi, ubicacion y radio base.</p><form method='POST' action='/save'>";

  html += "<label>Wi-Fi 1</label><input name='ssid1' value='" + htmlEscape(config.ssid1) + "'>";
  html += "<label>Clave Wi-Fi 1</label><input type='password' name='pass1' value='" + htmlEscape(config.pass1) + "'>";
  html += "<label>Wi-Fi 2</label><input name='ssid2' value='" + htmlEscape(config.ssid2) + "'>";
  html += "<label>Clave Wi-Fi 2</label><input type='password' name='pass2' value='" + htmlEscape(config.pass2) + "'>";
  html += "<label>Wi-Fi 3</label><input name='ssid3' value='" + htmlEscape(config.ssid3) + "'>";
  html += "<label>Clave Wi-Fi 3</label><input type='password' name='pass3' value='" + htmlEscape(config.pass3) + "'>";

  html += "<div class='grid'>";
  html += "<div><label>Latitud</label><input type='number' step='0.000001' name='lat' value='" + String(config.latitude, 6) + "' required></div>";
  html += "<div><label>Longitud</label><input type='number' step='0.000001' name='lon' value='" + String(config.longitude, 6) + "' required></div></div>";

  html += "<label>Radio base (NM)</label><input type='number' min='1' max='250' name='radius' value='" + String(config.baseRadiusNM) + "' required>";
  html += "<p class='small'>La pantalla queda encendida solo si hay vuelos dentro de este radio.</p>";
  html += "<button type='submit'>Guardar y reiniciar</button></form></div></body></html>";

  return html;
}

void handlePortalRoot() {
  webServer.send(200, "text/html; charset=utf-8", buildConfigPage());
}

void handlePortalSave() {
  config.ssid1 = webServer.arg("ssid1");
  config.pass1 = webServer.arg("pass1");
  config.ssid2 = webServer.arg("ssid2");
  config.pass2 = webServer.arg("pass2");
  config.ssid3 = webServer.arg("ssid3");
  config.pass3 = webServer.arg("pass3");
  config.latitude = webServer.arg("lat").toDouble();
  config.longitude = webServer.arg("lon").toDouble();
  config.baseRadiusNM = webServer.arg("radius").toInt();

  if (config.baseRadiusNM < 1) config.baseRadiusNM = 15;
  if (config.baseRadiusNM > 250) config.baseRadiusNM = 250;

  saveConfig();

  webServer.send(200, "text/html; charset=utf-8",
    "<html><body style='font-family:Arial;background:#101318;color:white;text-align:center;padding:40px'>"
    "<h2>Configuracion guardada</h2><p>Reiniciando...</p></body></html>");

  delay(1200);
  ESP.restart();
}

void startConfigPortal() {
  screenOn();
  rgbOff();

  WiFi.disconnect(true);
  delay(300);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(CONFIG_AP_SSID);

  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);

  webServer.on("/", HTTP_GET, handlePortalRoot);
  webServer.on("/save", HTTP_POST, handlePortalSave);
  webServer.onNotFound(handlePortalRoot);
  webServer.begin();

  gfx->fillScreen(BLACK);
  gfx->setTextColor(CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(28, 24);
  gfx->println("MODO CONFIG");
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(22, 62);
  gfx->println("Conectate a:");
  gfx->setTextColor(YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(22, 78);
  gfx->println(CONFIG_AP_SSID);
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(22, 120);
  gfx->println(apIP.toString());

  while (true) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    delay(2);
  }
}

bool bootHeldForConfig() {
  if (digitalRead(BUTTON_BOOT) != LOW) return false;

  unsigned long start = millis();
  while (digitalRead(BUTTON_BOOT) == LOW) {
    if (millis() - start >= CONFIG_BUTTON_HOLD) return true;
    delay(20);
  }
  return false;
}

// ======================================================
// WIFI
// ======================================================

bool tryWiFiNetwork(const String &ssid, const String &password) {
  if (ssid.length() == 0) return false;

  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long started = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - started < 8000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.disconnect();
  delay(250);
  return false;
}

bool connectWiFi() {
  screenOn();
  gfx->fillScreen(BLACK);
  gfx->setTextColor(CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(15, 30);
  gfx->println("RADAR ADS-B");
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(15, 65);
  gfx->println("Buscando WiFi...");

  WiFi.mode(WIFI_STA);

  bool ok =
    tryWiFiNetwork(config.ssid1, config.pass1) ||
    tryWiFiNetwork(config.ssid2, config.pass2) ||
    tryWiFiNetwork(config.ssid3, config.pass3);

  if (ok) {
    gfx->setTextColor(GREEN);
    gfx->setCursor(15, 85);
    gfx->println("WiFi OK");
    delay(500);
  }

  return ok;
}

// ======================================================
// ORIGEN / DESTINO
// ======================================================

void getRoute(Aircraft &a) {
  a.origin = "---";
  a.destination = "---";

  if (a.callsign.length() == 0) return;

  if (a.callsign == lastRouteCallsign) {
    a.origin = lastOrigin;
    a.destination = lastDestination;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.adsbdb.com/v0/callsign/" + a.callsign;
  if (!http.begin(client, url)) return;

  http.setTimeout(10000);
  http.addHeader("User-Agent", "ESP32-C6-FlightRadar/1.0");
  http.addHeader("Accept", "application/json");

  int code = http.GET();

  if (code != 200) {
    http.end();
    lastRouteCallsign = a.callsign;
    lastOrigin = "---";
    lastDestination = "---";
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getStream())) {
    http.end();
    return;
  }

  JsonObject route = doc["response"]["flightroute"];
  if (route.isNull()) {
    http.end();
    lastRouteCallsign = a.callsign;
    lastOrigin = "---";
    lastDestination = "---";
    return;
  }

  const char* oiata = route["origin"]["iata_code"] | "";
  const char* oicao = route["origin"]["icao_code"] | "";
  const char* diata = route["destination"]["iata_code"] | "";
  const char* dicao = route["destination"]["icao_code"] | "";

  if (strlen(oiata) > 0) a.origin = String(oiata);
  else if (strlen(oicao) > 0) a.origin = String(oicao);

  if (strlen(diata) > 0) a.destination = String(diata);
  else if (strlen(dicao) > 0) a.destination = String(dicao);

  lastRouteCallsign = a.callsign;
  lastOrigin = a.origin;
  lastDestination = a.destination;
  http.end();
}

// ======================================================
// CONSULTAR AVIONES
// ======================================================

bool getAircraft(int radiusNM) {
  if (WiFi.status() != WL_CONNECTED) {
    if (!connectWiFi()) {
      startConfigPortal();
      return false;
    }
  }

  if (radiusNM > 250) radiusNM = 250;
  displayedRadiusNM = radiusNM;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url =
    "https://api.airplanes.live/v2/point/" +
    String(config.latitude, 6) + "/" +
    String(config.longitude, 6) + "/" +
    String(radiusNM);

  if (!http.begin(client, url)) return false;

  http.setTimeout(15000);
  http.addHeader("User-Agent", "ESP32-C6-FlightRadar/1.0");
  http.addHeader("Accept", "application/json");

  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream());

  if (error) {
    Serial.print("JSON error: ");
    Serial.println(error.c_str());
    http.end();
    return false;
  }

  aircraftCount = 0;
  featuredAircraftIndex = -1;

  JsonArray ac = doc["ac"].as<JsonArray>();

  for (JsonObject plane : ac) {
    if (aircraftCount >= MAX_AIRCRAFT) break;
    if (plane["lat"].isNull() || plane["lon"].isNull()) continue;

    Aircraft &a = aircraft[aircraftCount];

    a.origin = "";
    a.destination = "";
    a.lat = plane["lat"] | 0.0;
    a.lon = plane["lon"] | 0.0;

    const char* flight = plane["flight"] | "";
    a.callsign = String(flight);
    a.callsign.trim();

    const char* hex = plane["hex"] | "";
    a.hex = String(hex);
    if (a.callsign.length() == 0) a.callsign = a.hex;

    const char* type = plane["t"] | "";
    a.type = String(type);

    double gs = plane["gs"] | 0.0;
    a.speed = gs * 1.852;

    if (plane["alt_baro"].is<float>() ||
        plane["alt_baro"].is<int>() ||
        plane["alt_baro"].is<double>()) {
      a.altitude = ((double)plane["alt_baro"]) * 0.3048;
    } else {
      a.altitude = 0;
    }

    a.track = plane["track"] | 0.0;
    a.distanceKm = getDistanceKm(config.latitude, config.longitude, a.lat, a.lon);
    a.bearing = getBearing(config.latitude, config.longitude, a.lat, a.lon);

    calculateClosestApproach(a);
    aircraftCount++;
  }

  http.end();

  chooseFeaturedAircraft();

  if (featuredAircraftIndex >= 0) {
    getRoute(aircraft[featuredAircraftIndex]);
  }

  return true;
}

// ======================================================
// DIBUJAR AVION
// ======================================================

void drawPlane(int x, int y, double heading, uint16_t color) {
  double a = degToRad(heading - 90.0);
  double cosA = cos(a);
  double sinA = sin(a);

  int nx = x + cosA * 6;
  int ny = y + sinA * 6;
  int tx = x - cosA * 5;
  int ty = y - sinA * 5;

  double px = -sinA;
  double py = cosA;

  gfx->drawLine(nx, ny, tx, ty, color);
  gfx->drawLine(x + px * 5, y + py * 5, x - px * 5, y - py * 5, color);
  gfx->drawLine(tx + px * 3, ty + py * 3, tx - px * 3, ty - py * 3, color);
  gfx->fillCircle(x, y, 1, color);
}

// ======================================================
// PANEL DERECHO - VISTA CLASICA
// ======================================================

void drawClassicPanel(Aircraft &a) {
  // Etiquetas chicas pero descriptivas; valores grandes.
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(177, 62);
  gfx->print("DISTANCIA");

  gfx->setTextSize(2);
  gfx->setCursor(177, 73);
  gfx->print(a.distanceKm, 1);
  gfx->print("km");

  gfx->setTextSize(1);
  gfx->setCursor(177, 97);
  gfx->print("ALTURA");

  gfx->setTextSize(2);
  gfx->setCursor(177, 108);
  if (a.altitude > 0) {
    gfx->print(a.altitude, 0);
    gfx->print("m");
  } else {
    gfx->print("---");
  }

  gfx->setTextSize(1);
  gfx->setCursor(177, 132);
  gfx->print("VELOCIDAD");

  gfx->setTextSize(2);
  gfx->setCursor(177, 143);
  gfx->print(a.speed, 0);
  gfx->print("kmh");
}

// ======================================================
// PANEL DERECHO - VISTA PROXIMIDAD
// ======================================================

void drawProximityPanel(Aircraft &a) {
  gfx->setTextColor(WHITE);

  if (a.cpaValid && a.approaching) {
    bool passesNear = a.closestApproachKm <= PASS_NEAR_KM;

    // El titulo queda mas chico para no comerse todo el panel.
    gfx->setTextSize(1);
    gfx->setCursor(177, 66);

    if (passesNear) {
      gfx->print("PASA CERCA");
    } else {
      gfx->print("APROXIMACION");
    }

    gfx->setTextSize(1);
    gfx->setCursor(177, 86);
    gfx->print("TIEMPO");

    gfx->setTextSize(2);
    gfx->setCursor(177, 97);
    gfx->print(formatEta(a.minutesToClosest));

    gfx->setTextSize(1);
    gfx->setCursor(177, 124);
    gfx->print("DIST. MINIMA");

    gfx->setTextSize(2);
    gfx->setCursor(177, 136);
    gfx->print(a.closestApproachKm, 1);
    gfx->print("km");

  } else {
    gfx->setTextSize(1);
    gfx->setCursor(177, 72);

    if (a.cpaValid) {
      gfx->print("ESTADO");
      gfx->setTextSize(2);
      gfx->setCursor(177, 88);
      gfx->print("SE ALEJA");
    } else {
      gfx->print("ESTADO");
      gfx->setTextSize(2);
      gfx->setCursor(177, 88);
      gfx->print("SIN PROY.");
    }

    gfx->setTextSize(1);
    gfx->setCursor(177, 122);
    gfx->print("DIST. ACTUAL");

    gfx->setTextSize(2);
    gfx->setCursor(177, 136);
    gfx->print(a.distanceKm, 1);
    gfx->print("km");
  }
}

// ======================================================
// DIBUJAR RADAR
// ======================================================

void drawRadar() {
  gfx->fillScreen(BLACK);

  const int CX = 82;
  const int CY = 86;
  const int R = 70;
  double maxKm = displayedRadiusNM * 1.852;

  gfx->drawCircle(CX, CY, R, DARKGREY);
  gfx->drawCircle(CX, CY, R * 2 / 3, DARKGREY);
  gfx->drawCircle(CX, CY, R / 3, DARKGREY);
  gfx->drawLine(CX, CY - R, CX, CY + R, DARKGREY);
  gfx->drawLine(CX - R, CY, CX + R, CY, DARKGREY);
  gfx->fillCircle(CX, CY, 3, GREEN);

  gfx->setTextSize(1);
  gfx->setTextColor(GREY);
  gfx->setCursor(CX - 3, 2);   gfx->print("N");
  gfx->setCursor(CX - 3, 163); gfx->print("S");
  gfx->setCursor(2, CY - 3);   gfx->print("O");
  gfx->setCursor(157, CY - 3); gfx->print("E");

  for (int i = 0; i < aircraftCount; i++) {
    Aircraft &a = aircraft[i];
    if (a.distanceKm > maxKm) continue;

    double radius = (a.distanceKm / maxKm) * R;
    double angle = degToRad(a.bearing - 90.0);
    int x = CX + cos(angle) * radius;
    int y = CY + sin(angle) * radius;

    uint16_t color = (i == featuredAircraftIndex) ? YELLOW : CYAN;
    drawPlane(x, y, a.track, color);

    if (i < 5 && a.callsign.length() > 0) {
      String label = a.callsign;
      if (label.length() > 6) label = label.substring(0, 6);

      int labelX = x + 5;
      int labelY = y - 10;
      if (labelX > 130) labelX = x - 35;
      if (labelY < 0) labelY = y + 5;

      gfx->setTextSize(1);
      gfx->setTextColor(color);
      gfx->setCursor(labelX, labelY);
      gfx->print(label);
    }
  }

  gfx->drawLine(168, 0, 168, 171, DARKGREY);

  gfx->setTextSize(1);
  gfx->setTextColor((manualMode || startupMode) ? YELLOW : CYAN);
  gfx->setCursor(177, 3);
  gfx->print(displayedRadiusNM);
  gfx->print("NM ");
  gfx->print(maxKm, 0);
  gfx->print("km");

  if (aircraftCount == 0 || featuredAircraftIndex < 0) {
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(183, 60);
    gfx->print("SIN");
    gfx->setCursor(177, 82);
    gfx->print("VUELOS");
    return;
  }

  Aircraft &a = aircraft[featuredAircraftIndex];

  gfx->setTextColor(YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(177, 18);

  String cs = a.callsign;
  if (cs.length() > 8) cs = cs.substring(0, 8);
  gfx->print(cs);

  gfx->setTextSize(2);
  gfx->setTextColor(CYAN);
  gfx->setCursor(177, 40);
  gfx->print(a.origin);
  gfx->print(">");
  gfx->print(a.destination);

  if (showProximityPanel) {
    drawProximityPanel(a);
  } else {
    drawClassicPanel(a);
  }
}

void updatePanelRotation() {
  if (aircraftCount <= 0 || featuredAircraftIndex < 0) return;

  if (millis() - lastPanelRotation >= PANEL_ROTATION_INTERVAL) {
    lastPanelRotation = millis();
    showProximityPanel = !showProximityPanel;
    drawRadar();
  }
}

// ======================================================
// CAMBIAR AVION DESTACADO CON BOOT
// ======================================================

void cycleFeaturedAircraft() {
  if (aircraftCount <= 1) return;

  if (featuredAircraftIndex < 0 || featuredAircraftIndex >= aircraftCount) {
    featuredAircraftIndex = 0;
  } else {
    featuredAircraftIndex++;
    if (featuredAircraftIndex >= aircraftCount) featuredAircraftIndex = 0;
  }

  // Buscamos la ruta del nuevo avion seleccionado.
  getRoute(aircraft[featuredAircraftIndex]);

  // Cada cambio manual vuelve primero a la vista clasica.
  showProximityPanel = false;
  lastPanelRotation = millis();

  drawRadar();
}

// ======================================================
// CONSULTA NORMAL
// ======================================================

void performBaseSearch() {
  if (!getAircraft(config.baseRadiusNM)) return;

  baseHasAircraft = aircraftCount > 0;

  if (baseHasAircraft) {
    manualMode = false;
    manualLevel = 0;
    screenOn();

    if (!hadBaseAircraft) startAircraftAlert();

    hadBaseAircraft = true;
    showProximityPanel = false;
    lastPanelRotation = millis();

    drawRadar();
    return;
  }

  hadBaseAircraft = false;
  alertBlinking = false;
  rgbOff();

  if (manualMode || startupMode) return;

  gfx->fillScreen(BLACK);
  screenOff();
}

// ======================================================
// BUSQUEDA INICIAL
// ======================================================

void performStartupSearch() {
  startupMode = true;
  startupFoundAircraft = false;
  screenOn();

  for (int level = 0; level < SEARCH_LEVELS; level++) {
    int radius = radiusForLevel(level);

    gfx->fillScreen(BLACK);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(70, 45);
    gfx->print("BUSCANDO");
    gfx->setTextColor(YELLOW);
    gfx->setCursor(95, 75);
    gfx->print(radius);
    gfx->print(" NM");

    if (getAircraft(radius) && aircraftCount > 0) {
      startupFoundAircraft = true;
      startupDisplayStart = millis();
      showProximityPanel = false;
      lastPanelRotation = millis();
      drawRadar();
      return;
    }

    if (radius >= 250) break;
    delay(400);
  }

  startupMode = false;
  startupFoundAircraft = false;
  aircraftCount = 0;
  featuredAircraftIndex = -1;
  displayedRadiusNM = config.baseRadiusNM;
  baseHasAircraft = false;
  hadBaseAircraft = false;

  gfx->fillScreen(BLACK);
  screenOff();
}

void updateStartupMode() {
  if (!startupMode || !startupFoundAircraft) return;
  if (millis() - startupDisplayStart < STARTUP_DISPLAY_DURATION) return;

  startupMode = false;
  startupFoundAircraft = false;
  displayedRadiusNM = config.baseRadiusNM;
  manualMode = false;
  manualLevel = 0;

  performBaseSearch();
  lastUpdate = millis();
}

// ======================================================
// BUSQUEDA MANUAL CON BOOT
// ======================================================

void performManualSearch() {
  manualLevel++;
  if (manualLevel > SEARCH_LEVELS) manualLevel = 1;

  int radius = radiusForLevel(manualLevel - 1);

  manualMode = true;
  manualScreenStart = millis();
  screenOn();

  gfx->fillScreen(BLACK);
  gfx->setTextColor(YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(70, 50);
  gfx->print("RADAR ");
  gfx->print(radius);
  gfx->print("NM");

  gfx->setTextSize(1);
  gfx->setTextColor(GREY);
  gfx->setCursor(105, 85);
  gfx->print("Buscando...");

  if (getAircraft(radius)) {
    showProximityPanel = false;
    lastPanelRotation = millis();
    drawRadar();
  }
}

void updateButton() {
  static bool previousButton = HIGH;
  static unsigned long lastButtonTime = 0;

  bool button = digitalRead(BUTTON_BOOT);

  if (previousButton == HIGH && button == LOW) {
    if (millis() - lastButtonTime > 250) {
      lastButtonTime = millis();

      // Si hay varios vuelos visibles, BOOT cambia al siguiente.
      if (aircraftCount > 1 && (baseHasAircraft || manualMode)) {
        cycleFeaturedAircraft();
      }
      // Si no hay vuelos en el radio base, conserva la funcion
      // anterior de ampliar el rango manual.
      else if (!startupMode && !baseHasAircraft) {
        performManualSearch();
      }
    }
  }

  previousButton = button;
}

void updateManualTimeout() {
  if (!manualMode) return;
  if (millis() - manualScreenStart < MANUAL_SCREEN_DURATION) return;

  manualMode = false;
  manualLevel = 0;

  if (baseHasAircraft) {
    displayedRadiusNM = config.baseRadiusNM;
    performBaseSearch();
  } else {
    gfx->fillScreen(BLACK);
    screenOff();
  }
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(1200);

  pinMode(LCD_BL, OUTPUT);
  pinMode(BUTTON_BOOT, INPUT_PULLUP);
  pinMode(RGB_LED, OUTPUT);

  screenOn();
  rgbOff();

  gfx->begin();
  gfx->fillScreen(BLACK);

  loadConfig();

  if (bootHeldForConfig()) startConfigPortal();

  if (!config.configured || config.ssid1.length() == 0) {
    startConfigPortal();
  }

  if (!connectWiFi()) startConfigPortal();

  performStartupSearch();
  lastUpdate = millis();
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  updateAircraftAlert();

  if (startupMode) {
    updatePanelRotation();
    updateStartupMode();
    delay(20);
    return;
  }

  updateButton();
  updateManualTimeout();
  updatePanelRotation();

  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();
    performBaseSearch();
  }

  delay(20);
}
