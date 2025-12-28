#ifndef _SSD1331_H_
#define _SSD1331_H_

#include <adafruit_gfx/adafruit_gfx.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define TFTWIDTH 96
#define TFTHEIGHT 64

class SSD1331{
public:
    SSD1331(spi_device_handle_t spi, gpio_num_t dc, gpio_num_t rst);
    void begin();
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawChar(int16_t x, int16_t y, char c, uint16_t color);
    void drawText(int16_t x, int16_t y, const char* text, uint16_t color);
    void drawBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h);
    void clearWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
    void clearScreen();
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillScreen(uint16_t color);
    // commands
    void goHome(void);
    void goTo(int x, int y);
    int16_t height(void) const;
    int16_t width(void) const;
private:
    spi_device_handle_t _spi;
    gpio_num_t _dc;
    gpio_num_t _rst;

    void writeData(uint8_t data);
    void writeCommand(uint8_t data);
    void sendData(const uint8_t *data, size_t len);
    void sendData_chunked(const uint8_t *data, size_t len);
    void hw_reset();
};

#endif