// =============================================================================
// MODIFICARI PENTRU PANA DE CURENT / RESTART ROUTER:
// 
// 1. TIMEOUT LA CONECTARE (WiFiManager):
//    - S-a adaugat 'wm.setConfigPortalTimeout(90)' si 'wm.setConnectTimeout(30)'.
//    - Daca dupa pana de curent routerul inca nu a pornit (sau nu are semnal),
//      ESP32 NU ramane blocat la infinit in modul ConfigPortal/AP.
//    - Daca trec 90 secunde si nu s-a putut conecta, ESP32 isi da restart si
//      reincearca din nou, pana cand routerul devine functional.
//
// 2. TIMEOUT LA SINCRONIZARE NTP (setup):
//    - Daca routerul a pornit WiFi-ul dar internetul de la furnizor intarzie,
//      asteptarea dupa primul pachet NTP este limitata la 30 secunde,
//      evitand blocarea permanenta si repornind curat.
//
// 3. WATCHDOG SI AUTO-RECOVERY IN LOOP:
//    - Daca netul pica in timpul functionarii, LED-ul clipeste rapid si
//      se apeleaza reconectarea la fiecare 10 secunde (fara delay-uri blocante).
//    - Daca starea de deconectat depaseste 3 minute continuu, ESP32 se restarteaza
//      automat pentru a reseta complet stiva WiFi.
// =============================================================================

// based on info and software from https://youtube.com/shorts/Q6-oOOAFoyY?is=aJi1mZaNmuxJ3PvN
// Nicu FLORICA (niq_ro) design the clock using AI 
//
// SCHEMA DE CONECTARE:
// - ESP32 conectat la 4 ecrane ST7789 (170x320) prin interfața SPI partajată
// - SCLK (CLK) -> GPIO 4
// - MOSI (DIN) -> GPIO 6
// - DC (AO)    -> GPIO 7
// - RES RST)   -> 3.3V
// - CS H1 (Ore Zeci)   -> GPIO 10
// - CS H2 (Ore Unit)   -> GPIO 3
// - CS M1 (Min Zeci)   -> GPIO 2
// - CS M2 (Min Unit)   -> GPIO 1
// - LED Secunde + 200R -> GPIO 5

#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFiManager.h>

// Fișierele de imagini comprimate (0..9 și blank pentru 10)
#include "img_0_rle.h"
#include "img_1_rle.h"
#include "img_2_rle.h"
#include "img_3_rle.h"
#include "img_4_rle.h"
#include "img_5_rle.h"
#include "img_6_rle.h"
#include "img_7_rle.h"
#include "img_8_rle.h"
#include "img_9_rle.h"
#include "img_blank_rle.h"

// Server NTP public
const char* ntpServer = "pool.ntp.org";

// <-- ORA VARA/IARNA: String POSIX pentru România/Europa de Est
const char* timezone_info = "EET-2EEST,M3.5.0/3,M10.5.0/4"; 

#define LED_SECUNDE 5 // Pinul unde legi LED-ul

// Pini SPI comuni
#define TFT_SCLK  4   
#define TFT_MOSI  6   
#define TFT_DC    7   

// Pini CS independenți pentru cele 4 ecrane (De la stânga la dreapta)
#define CS_H1  10   // Ore Zeci
#define CS_H2   3   // Ore Unități
#define CS_M1   2   // Minute Zeci
#define CS_M2   1   // Minute Unități

#define SCREEN_W 170
#define SCREEN_H 320

// Inițializare cele 4 ecrane (RST dezactivat cu -1)
Adafruit_ST7789 tftH1 = Adafruit_ST7789(CS_H1, TFT_DC, -1);
Adafruit_ST7789 tftH2 = Adafruit_ST7789(CS_H2, TFT_DC, -1);
Adafruit_ST7789 tftM1 = Adafruit_ST7789(CS_M1, TFT_DC, -1);
Adafruit_ST7789 tftM2 = Adafruit_ST7789(CS_M2, TFT_DC, -1);

uint16_t lineBuffer[SCREEN_W];

// Istoric stări ecrane pentru a preveni rescrierea cadrelor identice
int uH1 = -1, uH2 = -1, uM1 = -1, uM2 = -1;

// Variabile pentru monitorizare si reconectare WiFi
unsigned long lastReconnectAttempt = 0; 
unsigned long wifiLostTime = 0;

const uint8_t* getImage(uint8_t n, uint32_t &size) {
  switch (n) {
    case 0: size = img_0_rle_size; return img_0_rle;
    case 1: size = img_1_rle_size; return img_1_rle;
    case 2: size = img_2_rle_size; return img_2_rle;
    case 3: size = img_3_rle_size; return img_3_rle;
    case 4: size = img_4_rle_size; return img_4_rle;
    case 5: size = img_5_rle_size; return img_5_rle;
    case 6: size = img_6_rle_size; return img_6_rle;
    case 7: size = img_7_rle_size; return img_7_rle;
    case 8: size = img_8_rle_size; return img_8_rle;
    case 9: size = img_9_rle_size; return img_9_rle;
    default: size = img_blank_rle_size; return img_blank_rle; // 10 sau orice altceva = Blank
  }
}

// Funcție universală de randare pe un ecran țintă specificat
void drawToDisplay(Adafruit_ST7789 &display, uint8_t n) {
  uint32_t size;
  const uint8_t *data = getImage(n, size);
  uint32_t pos = 0;
  int x = 0, y = 0;

  while (pos + 2 < size && y < SCREEN_H) {
    uint16_t color = (uint16_t)pgm_read_byte(data + pos) | ((uint16_t)pgm_read_byte(data + pos + 1) << 8);
    uint8_t count = pgm_read_byte(data + pos + 2);
    pos += 3;

    while (count--) {
      lineBuffer[x++] = color;
      if (x >= SCREEN_W) {
        display.drawRGBBitmap(0, y, lineBuffer, SCREEN_W, 1);
        x = 0; y++;
        if (y >= SCREEN_H) break;
      }
    }
  }
}

// Funcție verificare și reconectare WiFi cu auto-recovery
void verificaWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiLostTime = 0;
    return; 
  }

  unsigned long acum = millis();
  if (wifiLostTime == 0) {
    wifiLostTime = acum;
  }

  // Incercare de reconectare la fiecare 10 secunde
  if (acum - lastReconnectAttempt > 10000) {
    lastReconnectAttempt = acum;
    Serial.println("WiFi pierdut! Se incearca reconectarea...");
    WiFi.reconnect();  
  }

  // Daca sta deconectat mai mult de 3 minute continuu, restart complet ESP32
  if (acum - wifiLostTime > 180000) {
    Serial.println("WiFi deconectat prelungit (>3 min). Restartare ESP32...");
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200); 

  // Configurare pini CS și decuplare electrică inițială
  int pinsCS[] = {CS_H1, CS_H2, CS_M1, CS_M2};
  for(int i=0; i<4; i++) {
    pinMode(pinsCS[i], OUTPUT);
    digitalWrite(pinsCS[i], HIGH);
  }
  pinMode(LED_SECUNDE, OUTPUT);
  
  // Pornire magistrală SPI
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1); 

  // Inițializare software secvențială pentru toate ecranele
  tftH1.init(170, 320); tftH1.setRotation(0);
  tftH2.init(170, 320); tftH2.setRotation(0);
  tftM1.init(170, 320); tftM1.setRotation(0);
  tftM2.init(170, 320); tftM2.setRotation(0);

  // Afișăm fundalul de lampă gol (10/Blank) pe toate ecranele la pornire
  drawToDisplay(tftH1, 10);
  drawToDisplay(tftH2, 10);
  drawToDisplay(tftM1, 10);
  drawToDisplay(tftM2, 10);

  // --- Configurare WiFi cu WiFiManager și TIMEOUT ANTIBLOCARE ---
  WiFiManager wm;

  // Timeout pentru conectarea la WiFi-ul salvat (in secunde)
  wm.setConnectTimeout(30);

  // Daca nu gaseste routerul, portalul AP sta deschis doar 90 de secunde, apoi iese!
  // Previne ramanerea blocata in mod AP dupa pana de curent.
  wm.setConfigPortalTimeout(90);

  bool res = wm.autoConnect("NixieClock_Setup");

  if (!res) {
    Serial.println("WiFi conectare esuata sau timeout portal! Restartare in 2 secunde...");
    delay(2000);
    ESP.restart(); // Se restarteaza si reincearca pana cand routerul a terminat de bootat
  } else {
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    Serial.println("WiFi conectat cu succes!");
  }
  // -------------------------------------------------------------

  // Setează fusul orar cu reguli oficiale de schimbare automată
  configTzTime(timezone_info, ntpServer);

  // Sincronizare initiala a orei cu timeout de 30 secunde
  Serial.print("Asteptare sincronizare NTP...");
  struct tm initialTime;
  unsigned long startNTP = millis();
  while (!getLocalTime(&initialTime, 1000)) {
    Serial.print(".");
    if (millis() - startNTP > 30000) {
      Serial.println("\nTimeout NTP initial! Restartare ESP32...");
      ESP.restart();
    }
  }
  Serial.println("\nTimp sincronizat cu succes!");
}

void loop() {
  // Verificăm starea WiFi la fiecare iterație
  verificaWiFi();

  // Dacă nu avem WiFi, avertizam vizual si nu actualizam afisajul
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_SECUNDE, !digitalRead(LED_SECUNDE)); 
    delay(250);
    return; 
  }

  struct tm timeinfo;
  
  // Încercăm să citim timpul local preluat de cip de la NTP
  if (!getLocalTime(&timeinfo, 500)) {
    delay(200);
    return; 
  }

  // Extragere valori curente pentru Ore și Minute
  int h1 = timeinfo.tm_hour / 10;
  int h2 = timeinfo.tm_hour % 10;
  int m1 = timeinfo.tm_min / 10;
  int m2 = timeinfo.tm_min % 10;

  // --- LOGICĂ STINGERE ZECI ORE (LEADING ZERO BLANKING) ---
  if (h1 == 0) {
    h1 = 10; 
  }
  // ---------------------------------------------------------

  // Actualizare afisaj cand se schimba cifrele
  if (h1 != uH1) { drawToDisplay(tftH1, h1); uH1 = h1; }
  if (h2 != uH2) { drawToDisplay(tftH2, h2); uH2 = h2; }
  if (m1 != uM1) { drawToDisplay(tftM1, m1); uM1 = m1; }
  if (m2 != uM2) { drawToDisplay(tftM2, m2); uM2 = m2; }

  // --- LOGICĂ LED SECUNDE (1 secunda ciclu complet: 500ms ON, 500ms OFF) ---
  digitalWrite(LED_SECUNDE, HIGH); 
  delay(500);                     
  digitalWrite(LED_SECUNDE, LOW);  
  delay(500);                     
}
