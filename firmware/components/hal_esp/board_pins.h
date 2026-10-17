#pragma once

// JP1 header wiring, see docs/wiring.md.
#define PIN_LCD_SCLK    1
#define PIN_LCD_CS      2
#define PIN_TOUCH_CS    3
#define PIN_LCD_DC      4
#define PIN_LCD_RST     5
#define PIN_TOUCH_IRQ   20
#define PIN_LCD_MOSI    32
#define PIN_LCD_MISO    33

#define LCD_H_RES       480
#define LCD_V_RES       320

// RPi-style ILI9486 boards are flaky above ~20 MHz.
#define LCD_SPI_HZ      (20 * 1000 * 1000)
#define TOUCH_SPI_HZ    (2 * 1000 * 1000)
