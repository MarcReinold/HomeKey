// ============================================================
// TFT_eSPI User_Setup.h fuer: Lilygo T-Display S3 (1.9" 170x320)
// 8-bit Parallel Interface
//
// ANLEITUNG: Diese Datei nach
//   Dokumente\Arduino\libraries\TFT_eSPI\User_Setup.h
// kopieren (die alte Datei ersetzen).
// ============================================================

#define USER_SETUP_ID 206
#define ST7789_DRIVER
#define INIT_SEQUENCE_3
#define CGRAM_OFFSET
#define TFT_RGB_ORDER TFT_RGB
#define TFT_INVERSION_ON

// 8-bit Parallel (kein SPI!)
#define TFT_PARALLEL_8_BIT

#define TFT_WIDTH  170
#define TFT_HEIGHT 320

#define TFT_CS   6
#define TFT_DC   7
#define TFT_RST  5
#define TFT_WR   8
#define TFT_RD   9

// Daten-Pins D0-D7
#define TFT_D0  39
#define TFT_D1  40
#define TFT_D2  41
#define TFT_D3  42
#define TFT_D4  45
#define TFT_D5  46
#define TFT_D6  47
#define TFT_D7  48

// Backlight GPIO 38
#define TFT_BL  38
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
