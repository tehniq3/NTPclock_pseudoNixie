// based on info and software from https://youtube.com/shorts/Q6-oOOAFoyY?is=aJi1mZaNmuxJ3PvN
// Nicu FLORICA (niq_ro) design the 2 number counter using AI 

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

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

#define TFT_SCLK  4   
#define TFT_MOSI  6   
#define TFT_DC    7   

#define TFT1_CS  10   // Ecran STÂNGA (Zeci)
#define TFT2_CS   3   // Ecran DREAPTA (Unități)

#define SCREEN_W 170
#define SCREEN_H 320

Adafruit_ST7789 tft1 = Adafruit_ST7789(TFT1_CS, TFT_DC, -1);
Adafruit_ST7789 tft2 = Adafruit_ST7789(TFT2_CS, TFT_DC, -1);

uint16_t lineBuffer[SCREEN_W];

// Variabile pentru stocarea timpului curent și a stării anterioare
int secunde = 0;
int ultima_cifra_zeci = -1; 

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

void showImage1(uint8_t n) {
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
        tft1.drawRGBBitmap(0, y, lineBuffer, SCREEN_W, 1);
        x = 0; y++;
        if (y >= SCREEN_H) break;
      }
    }
  }
}

void showImage2(uint8_t n) {
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
        tft2.drawRGBBitmap(0, y, lineBuffer, SCREEN_W, 1);
        x = 0; y++;
        if (y >= SCREEN_H) break;
      }
    }
  }
}

void setup() {
  pinMode(TFT1_CS, OUTPUT);
  pinMode(TFT2_CS, OUTPUT);
  digitalWrite(TFT1_CS, HIGH);
  digitalWrite(TFT2_CS, HIGH);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1); 

  tft1.init(170, 320);
  tft1.setRotation(0);

  tft2.init(170, 320);
  tft2.setRotation(0);

  // Pornim ambele ecrane pe imaginea neutră (00)
  showImage1(0);
  showImage2(0);
  ultima_cifra_zeci = 0; // Salvăm starea inițială
  delay(1000);
}

void loop() {
  // Spargem numărul total de secunde în Zeci și Unități
  int cifra_zeci = secunde / 10;
  int cifra_unitati = secunde % 10;

  // OPTIMIZARE: Actualizăm ecranul 1 (Zecile) DOAR dacă s-a schimbat cifra zecilor
  // Evită ca ecranul din stânga să clipească la fiecare secundă
  if (cifra_zeci != ultima_cifra_zeci) {
    showImage1(cifra_zeci);
    ultima_cifra_zeci = cifra_zeci;
  }

  // Ecranul 2 (Unitățile) se schimbă în fiecare secundă
  showImage2(cifra_unitati);

  // Așteptăm exact o secundă
  delay(1000);

  // Incrementăm timpul
  secunde++;

  // Resetăm cronometrul când ajunge la 100 (afișează de la 00 la 99)
  if (secunde >= 100) {
    secunde = 0;
  }
}
