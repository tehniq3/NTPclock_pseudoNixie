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

// =====================================================
// ESP32-C3 SUPER MINI + ST7789 170x320
// IN-14 image selector, compressed images
//
// This version uses LOSSLESS RLE compression.
// The displayed pixels are identical to the original
// RGB565 .h images, but the Flash usage is much lower.
//
// img_0 = lamp glowing, no digit
// img_1..img_9 = digits 1..9
//
// ST7789:
// VCC -> 3.3V
// GND -> GND
// SCL -> GPIO4
// SDA -> GPIO6
// RST -> GPIO8
// DC  -> GPIO7
// CS  -> GPIO10
// BLK -> 3.3V
//
// TENSTAR EC11:
// GND -> GND
// S1  -> GPIO2
// S2  -> GPIO3
// KEY -> GPIO5
// 5V  -> 3.3V
//
// Verified encoder:
// CW  : 11 -> 01 -> 00 -> 11
// CCW : 11 -> 10 -> 00 -> 11
// =====================================================

#define TFT_CS    10
#define TFT_DC     7
#define TFT_RST    8
#define TFT_SCLK   4
#define TFT_MOSI   6

//#define ENC_S1     2
//#define ENC_S2     3
//#define ENC_KEY    5

#define SCREEN_W 170
#define SCREEN_H 320

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

uint8_t digit = 0;
uint8_t previousState = 0;

bool keyPressed = false;
unsigned long lastKeyTime = 0;

// One scanline only: 170 pixels = 340 bytes RAM.
uint16_t lineBuffer[SCREEN_W];

int nr = 10;


// =====================================================
// Select compressed image
// =====================================================

const uint8_t* getImage(uint8_t n, uint32_t &size)
{
  switch (n)
  {
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

// =====================================================
// Draw one compressed image
// =====================================================

void showImage(uint8_t n)
{
  uint32_t size;
  const uint8_t *data = getImage(n, size);

  uint32_t pos = 0;
  int x = 0;
  int y = 0;

  while (pos + 2 < size && y < SCREEN_H)
  {
    uint16_t color =
      (uint16_t)pgm_read_byte(data + pos) |
      ((uint16_t)pgm_read_byte(data + pos + 1) << 8);

    uint8_t count = pgm_read_byte(data + pos + 2);
    pos += 3;

    while (count--)
    {
      lineBuffer[x++] = color;

      if (x >= SCREEN_W)
      {
        // Send one complete scanline to the display.
        tft.drawRGBBitmap(0, y, lineBuffer, SCREEN_W, 1);

        x = 0;
        y++;

        if (y >= SCREEN_H)
          break;
      }
    }
  }
}


// =====================================================
// Setup
// =====================================================

void setup()
{
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.init(170, 320);
  tft.setRotation(0);

  // Start with heater/mesh glow and no digit.
  showImage(10);
  delay(2000);
}

// =====================================================
// Loop
// =====================================================

void loop()
{
  for (int nr = 10; nr >=0; nr--) 
    {
      showImage(nr);
      delay(1000);
    }

     showImage(10);
      delay(1000);

  for (int nr = 0; nr <= 11; nr++) 
    {
      showImage(nr);
      delay(1000);
    }

  
} // end main loop
