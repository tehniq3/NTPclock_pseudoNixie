// based on info and software from https://youtube.com/shorts/Q6-oOOAFoyY?is=aJi1mZaNmuxJ3PvN
// Nicu FLORICA (niq_ro) design the clock using AI 
//
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

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

// Date rețea Wi-Fi
const char* STATION_SSID     = "bbk2";  // your WiFi network name
const char* STATION_PASSWORD = "internet2";  // your WiFi password

// Server NTP și configurare fus orar (Ex: Europa/București are GMT+2 și DST/+1)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 2 * 3600;      // UTC + 2 ore standard
const int   daylightOffset_sec = 3600;     // +1 oră pe timpul de vară

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
    default: size = img_blank_rle_size; return img_blank_rle;
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

void setup() {
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

  // Afișăm fundalul de lampă gol (10) pe toate ecranele în timpul conectării
  drawToDisplay(tftH1, 10);
  drawToDisplay(tftH2, 10);
  drawToDisplay(tftM1, 10);
  drawToDisplay(tftM2, 10);

  // Conectare la rețeaua Wi-Fi
  WiFi.begin(STATION_SSID, STATION_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Inițializare și sincronizare timp prin serverele NTP publice
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  struct tm timeinfo;
  
  // Încercăm să citim timpul local preluat de cip de la NTP
  if (!getLocalTime(&timeinfo)) {
    delay(1000);
    return; // Dacă nu s-a sincronizat încă, reîncearcă bucla
  }

  // Extragere valori curente pentru Ore și Minute
  int h1 = timeinfo.tm_hour / 10;
  int h2 = timeinfo.tm_hour % 10;
  int m1 = timeinfo.tm_min / 10;
  int m2 = timeinfo.tm_min % 10;

  // Actualizare inteligentă: Schimbă imaginile DOAR dacă valorile diferă de secunda precedentă
  if (h1 != uH1) { drawToDisplay(tftH1, h1); uH1 = h1; }
  if (h2 != uH2) { drawToDisplay(tftH2, h2); uH2 = h2; }
  if (m1 != uM1) { drawToDisplay(tftM1, m1); uM1 = m1; }
  if (m2 != uM2) { drawToDisplay(tftM2, m2); uM2 = m2; }

 // delay(1000); // Interval verificare/sincronizare
 // --- LOGICĂ LED SECUNDE ---
  digitalWrite(LED_SECUNDE, HIGH); // Aprinde LED-ul
  delay(500);                     // Stă aprins 0.5 secunde
  digitalWrite(LED_SECUNDE, LOW);  // Stinge LED-ul
  delay(500);                     // Stă stins 0.5 secunde
}
