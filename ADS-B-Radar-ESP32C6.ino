#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <math.h>

// ======================================================
// WIFI - AGREGA TUS REDES ACA
// ======================================================

struct WifiNetwork {
  const char* ssid;
  const char* password;
};

WifiNetwork wifiNetworks[] = {
  { "TU_WIFI_1", "TU_CLAVE_1" },
  { "TU_WIFI_2", "TU_CLAVE_2" },
  { "TU_HOTSPOT", "TU_CLAVE_3" }
};

const int WIFI_NETWORK_COUNT =
  sizeof(wifiNetworks) / sizeof(wifiNetworks[0]);

// ======================================================
// UBICACION DEL RADAR
// ======================================================

const double MY_LAT = -31.380000;
const double MY_LON = -57.980000;

// ======================================================
// RADIOS
// ======================================================

const int BASE_RADIUS_NM = 15;
const int SEARCH_LEVELS = 5;

// ======================================================
// TIEMPOS
// ======================================================

const unsigned long UPDATE_INTERVAL = 30000;
const unsigned long MANUAL_SCREEN_DURATION = 15000;
const unsigned long STARTUP_DISPLAY_DURATION = 60000;
const unsigned long ALERT_DURATION = 30000;
const unsigned long BLINK_INTERVAL = 400;

// ======================================================
// COLORES
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
  LCD_DC,
  LCD_CS,
  LCD_SCK,
  LCD_MOSI,
  GFX_NOT_DEFINED
);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  LCD_RST,
  1,
  true,
  172,
  320,
  34,
  0,
  34,
  0
);

// ======================================================
// AVIONES
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
};

#define MAX_AIRCRAFT 30
Aircraft aircraft[MAX_AIRCRAFT];
int aircraftCount = 0;

// ======================================================
// ESTADOS GENERALES
// ======================================================

unsigned long lastUpdate = 0;
bool baseHasAircraft = false;
bool hadBaseAircraft = false;

// ======================================================
// MODO MANUAL
// ======================================================

bool manualMode = false;
int manualLevel = 0;
unsigned long manualScreenStart = 0;

// ======================================================
// MODO INICIAL
// ======================================================

bool startupMode = true;
bool startupFoundAircraft = false;
unsigned long startupDisplayStart = 0;

// ======================================================
// RADIO QUE SE ESTA MOSTRANDO
// ======================================================

int displayedRadiusNM = BASE_RADIUS_NM;

// ======================================================
// LED
// ======================================================

bool alertBlinking = false;
bool ledState = false;
unsigned long alertStart = 0;
unsigned long lastBlink = 0;

// ======================================================
// CACHE RUTA
// ======================================================

String lastRouteCallsign = "";
String lastOrigin = "";
String lastDestination = "";

// ======================================================
// MATEMATICA
// ======================================================

double degToRad(double deg) { return deg * PI / 180.0; }
double radToDeg(double rad) { return rad * 180.0 / PI; }

double getDistanceKm(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;
  double dLat = degToRad(lat2 - lat1);
  double dLon = degToRad(lon2 - lon1);
  double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
             cos(degToRad(lat1)) * cos(degToRad(lat2)) *
             sin(dLon / 2.0) * sin(dLon / 2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return R * c;
}

double getBearing(double lat1, double lon1, double lat2, double lon2) {
  double p1 = degToRad(lat1);
  double p2 = degToRad(lat2);
  double dLon = degToRad(lon2 - lon1);
  double y = sin(dLon) * cos(p2);
  double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dLon);
  double brg = radToDeg(atan2(y, x));
  return fmod(brg + 360.0, 360.0);
}

String bearingToText(double bearing) {
  if (bearing >= 337.5 || bearing < 22.5) return "N";
  if (bearing < 67.5)  return "NE";
  if (bearing < 112.5) return "E";
  if (bearing < 157.5) return "SE";
  if (bearing < 202.5) return "S";
  if (bearing < 247.5) return "SO";
  if (bearing < 292.5) return "O";
  return "NO";
}

// ======================================================
// PANTALLA
// ======================================================

void screenOn() { digitalWrite(LCD_BL, HIGH); }
void screenOff() { digitalWrite(LCD_BL, LOW); }

// ======================================================
// RGB
// ======================================================

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
    if (ledState) rgbOn();
    else rgbOff();
  }
}

// ======================================================
// WIFI
// ======================================================

void connectWiFi() {
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

  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
      Serial.println();
      Serial.print("Probando WiFi: ");
      Serial.println(wifiNetworks[i].ssid);

      gfx->fillRect(15, 85, 295, 40, BLACK);
      gfx->setTextColor(GREY);
      gfx->setTextSize(1);
      gfx->setCursor(15, 85);
      gfx->print("Probando: ");
      gfx->setTextColor(WHITE);
      gfx->print(wifiNetworks[i].ssid);

      WiFi.begin(wifiNetworks[i].ssid, wifiNetworks[i].password);
      unsigned long startAttempt = millis();

      while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 7000) {
        delay(250);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi conectado");
        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());

        gfx->fillRect(15, 85, 295, 40, BLACK);
        gfx->setTextColor(GREEN);
        gfx->setCursor(15, 85);
        gfx->println("WiFi OK");
        gfx->setTextColor(WHITE);
        gfx->setCursor(15, 100);
        gfx->print("Red: ");
        gfx->println(WiFi.SSID());
        delay(700);
        return;
      }

      WiFi.disconnect();
      delay(300);
    }

    gfx->fillRect(15, 85, 295, 40, BLACK);
    gfx->setTextColor(RED);
    gfx->setCursor(15, 85);
    gfx->println("Sin WiFi");
    Serial.println();
    Serial.println("Ninguna red disponible");
    delay(5000);
  }
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

  Serial.print("Buscando ruta: ");
  Serial.println(a.callsign);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.adsbdb.com/v0/callsign/" + a.callsign;

  if (!http.begin(client, url)) {
    Serial.println("Error API ruta");
    return;
  }

  http.setTimeout(10000);
  http.addHeader("User-Agent", "ESP32-C6-FlightRadar/1.0");
  http.addHeader("Accept", "application/json");

  int code = http.GET();
  Serial.print("Ruta HTTP: ");
  Serial.println(code);

  if (code != 200) {
    http.end();
    lastRouteCallsign = a.callsign;
    lastOrigin = "---";
    lastDestination = "---";
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream());

  if (error) {
    Serial.print("JSON ruta: ");
    Serial.println(error.c_str());
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

  Serial.print("Ruta: ");
  Serial.print(a.origin);
  Serial.print(" -> ");
  Serial.println(a.destination);

  http.end();
}

// ======================================================
// CONSULTAR AVIONES
// ======================================================

bool getAircraft(int radiusNM) {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (radiusNM > 250) radiusNM = 250;
  displayedRadiusNM = radiusNM;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url =
    "https://api.airplanes.live/v2/point/" +
    String(MY_LAT, 6) + "/" +
    String(MY_LON, 6) + "/" +
    String(radiusNM);

  Serial.println();
  Serial.print("Consultando radio ");
  Serial.print(radiusNM);
  Serial.println(" NM");

  if (!http.begin(client, url)) {
    Serial.println("Error HTTP");
    return false;
  }

  http.setTimeout(15000);
  http.addHeader("User-Agent", "ESP32-C6-FlightRadar/1.0");
  http.addHeader("Accept", "application/json");

  int httpCode = http.GET();
  Serial.print("HTTP: ");
  Serial.println(httpCode);

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
      double altFt = plane["alt_baro"];
      a.altitude = altFt * 0.3048;
    } else {
      a.altitude = 0;
    }

    a.track = plane["track"] | 0.0;
    a.distanceKm = getDistanceKm(MY_LAT, MY_LON, a.lat, a.lon);
    a.bearing = getBearing(MY_LAT, MY_LON, a.lat, a.lon);
    aircraftCount++;
  }

  http.end();

  for (int i = 0; i < aircraftCount - 1; i++) {
    for (int j = i + 1; j < aircraftCount; j++) {
      if (aircraft[j].distanceKm < aircraft[i].distanceKm) {
        Aircraft temp = aircraft[i];
        aircraft[i] = aircraft[j];
        aircraft[j] = temp;
      }
    }
  }

  Serial.print("Aviones: ");
  Serial.println(aircraftCount);

  if (aircraftCount > 0) getRoute(aircraft[0]);

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

  int wx1 = x + px * 5;
  int wy1 = y + py * 5;
  int wx2 = x - px * 5;
  int wy2 = y - py * 5;
  int sx1 = tx + px * 3;
  int sy1 = ty + py * 3;
  int sx2 = tx - px * 3;
  int sy2 = ty - py * 3;

  gfx->drawLine(nx, ny, tx, ty, color);
  gfx->drawLine(wx1, wy1, wx2, wy2, color);
  gfx->drawLine(sx1, sy1, sx2, sy2, color);
  gfx->fillCircle(x, y, 1, color);
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
  gfx->setCursor(CX - 3, 2); gfx->print("N");
  gfx->setCursor(CX - 3, 163); gfx->print("S");
  gfx->setCursor(2, CY - 3); gfx->print("O");
  gfx->setCursor(157, CY - 3); gfx->print("E");

  for (int i = 0; i < aircraftCount; i++) {
    Aircraft &a = aircraft[i];
    if (a.distanceKm > maxKm) continue;

    double normalized = a.distanceKm / maxKm;
    double radius = normalized * R;
    double angle = degToRad(a.bearing - 90.0);
    int x = CX + cos(angle) * radius;
    int y = CY + sin(angle) * radius;

    uint16_t color = CYAN;
    if (i == 0) color = YELLOW;

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
  if (manualMode || startupMode) gfx->setTextColor(YELLOW);
  else gfx->setTextColor(CYAN);

  gfx->setCursor(177, 3);
  gfx->print(displayedRadiusNM);
  gfx->print("NM ");
  gfx->print(maxKm, 0);
  gfx->print("km");

  if (aircraftCount == 0) {
    gfx->setTextColor(GREY);
    gfx->setTextSize(2);
    gfx->setCursor(183, 60);
    gfx->print("SIN");
    gfx->setCursor(177, 82);
    gfx->print("VUELOS");
    return;
  }

  Aircraft &a = aircraft[0];

  gfx->setTextColor(YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(177, 20);
  String cs = a.callsign;
  if (cs.length() > 8) cs = cs.substring(0, 8);
  gfx->print(cs);

  gfx->setTextSize(2);
  gfx->setTextColor(CYAN);
  gfx->setCursor(177, 42);
  gfx->print(a.origin);
  gfx->print(">");
  gfx->print(a.destination);

  gfx->setTextSize(1);
  gfx->setTextColor(GREY);
  gfx->setCursor(177, 68);
  gfx->print("DIST");

  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(177, 78);
  gfx->print(a.distanceKm, 1);
  gfx->print("km");

  gfx->setTextSize(1);
  gfx->setTextColor(GREY);
  gfx->setCursor(177, 102);
  gfx->print("ALT");

  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(177, 112);
  if (a.altitude > 0) {
    gfx->print(a.altitude, 0);
    gfx->print("m");
  } else {
    gfx->print("---");
  }

  gfx->setTextSize(1);
  gfx->setTextColor(GREY);
  gfx->setCursor(177, 136);
  gfx->print("VEL");

  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(177, 146);
  gfx->print(a.speed, 0);
  gfx->print("kmh");
}

// ======================================================
// CONSULTA NORMAL
// ======================================================

void performBaseSearch() {
  Serial.println();
  Serial.println("=== RADIO BASE ===");

  if (!getAircraft(BASE_RADIUS_NM)) return;

  baseHasAircraft = aircraftCount > 0;

  if (baseHasAircraft) {
    manualMode = false;
    manualLevel = 0;
    screenOn();

    if (!hadBaseAircraft) {
      Serial.println("*** ENTRO UN VUELO AL RADIO BASE ***");
      startAircraftAlert();
    }

    hadBaseAircraft = true;
    drawRadar();
    return;
  }

  if (hadBaseAircraft) Serial.println("*** RADIO BASE VACIO ***");

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

  Serial.println();
  Serial.println("================================");
  Serial.println(" BUSQUEDA INICIAL");
  Serial.println("================================");

  for (int level = 0; level < SEARCH_LEVELS; level++) {
    int radius = BASE_RADIUS_NM * (1 << level);
    if (radius > 250) radius = 250;

    gfx->fillScreen(BLACK);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(70, 45);
    gfx->print("BUSCANDO");
    gfx->setTextColor(YELLOW);
    gfx->setCursor(95, 75);
    gfx->print(radius);
    gfx->print(" NM");

    Serial.print("Inicio: ");
    Serial.print(radius);
    Serial.println(" NM");

    if (getAircraft(radius) && aircraftCount > 0) {
      startupFoundAircraft = true;
      startupDisplayStart = millis();
      Serial.print("Encontrados: ");
      Serial.println(aircraftCount);
      drawRadar();
      return;
    }

    if (radius >= 250) break;
    delay(400);
  }

  Serial.println("Sin vuelos en busqueda inicial");

  startupMode = false;
  startupFoundAircraft = false;
  aircraftCount = 0;
  displayedRadiusNM = BASE_RADIUS_NM;
  baseHasAircraft = false;
  hadBaseAircraft = false;
  gfx->fillScreen(BLACK);
  screenOff();
}

void updateStartupMode() {
  if (!startupMode || !startupFoundAircraft) return;
  if (millis() - startupDisplayStart < STARTUP_DISPLAY_DURATION) return;

  Serial.println();
  Serial.println("Fin busqueda inicial");

  startupMode = false;
  startupFoundAircraft = false;
  displayedRadiusNM = BASE_RADIUS_NM;
  manualMode = false;
  manualLevel = 0;

  performBaseSearch();
  lastUpdate = millis();
}

// ======================================================
// BUSQUEDA MANUAL
// ======================================================

void performManualSearch() {
  manualLevel++;
  if (manualLevel > SEARCH_LEVELS) manualLevel = 1;

  int multiplier = 1 << (manualLevel - 1);
  int radius = BASE_RADIUS_NM * multiplier;
  if (radius > 250) radius = 250;

  manualMode = true;
  manualScreenStart = millis();
  screenOn();

  Serial.println();
  Serial.print("BOOT NIVEL ");
  Serial.print(manualLevel);
  Serial.print(" - ");
  Serial.print(radius);
  Serial.println(" NM");

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

  if (getAircraft(radius)) drawRadar();
}

// ======================================================
// BOTON BOOT
// ======================================================

void updateButton() {
  static bool previousButton = HIGH;
  static unsigned long lastButtonTime = 0;

  bool button = digitalRead(BUTTON_BOOT);

  if (previousButton == HIGH && button == LOW) {
    if (millis() - lastButtonTime > 250) {
      lastButtonTime = millis();

      if (!startupMode && !baseHasAircraft) {
        performManualSearch();
      }
    }
  }

  previousButton = button;
}

// ======================================================
// TIMEOUT MANUAL
// ======================================================

void updateManualTimeout() {
  if (!manualMode) return;
  if (millis() - manualScreenStart < MANUAL_SCREEN_DURATION) return;

  Serial.println("Fin modo manual");

  manualMode = false;
  manualLevel = 0;

  if (baseHasAircraft) {
    displayedRadiusNM = BASE_RADIUS_NM;
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
  delay(2000);

  pinMode(LCD_BL, OUTPUT);
  pinMode(BUTTON_BOOT, INPUT_PULLUP);
  pinMode(RGB_LED, OUTPUT);

  screenOn();
  rgbOff();

  gfx->begin();
  gfx->fillScreen(BLACK);

  connectWiFi();
  performStartupSearch();
  lastUpdate = millis();
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  updateAircraftAlert();

  if (startupMode) {
    updateStartupMode();
    delay(20);
    return;
  }

  updateButton();
  updateManualTimeout();

  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();
    performBaseSearch();
  }

  delay(20);
}
