#include "SSD1331.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "font8x8_basic.h"
#include "esp_log.h"
SSD1331::SSD1331(spi_device_handle_t spi, gpio_num_t dc, gpio_num_t rst)
    : _spi(spi), _dc(dc), _rst(rst) {}

void SSD1331::begin() {
    // Configure DC and RST pins
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << _dc) | (1ULL << _rst);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    hw_reset();

    // Initialization sequence (same as before)
    writeCommand(0xAE); // DISPLAYOFF
    writeCommand(0xA0); // SETREMAP
    writeCommand(0xA0); // RGB Color
    writeCommand(0xA1); // STARTLINE
    writeCommand(0x0);
    writeCommand(0xA2); // DISPLAYOFFSET
    writeCommand(0x0);
    writeCommand(0xA4); // NORMALDISPLAY
    writeCommand(0xA8); // SETMULTIPLEX
    writeCommand(0x3F);
    writeCommand(0xAD); // SETMASTER
    writeCommand(0x8E);
    writeCommand(0xB0); // POWERMODE
    writeCommand(0x0B);
    writeCommand(0xB1); // PRECHARGE
    writeCommand(0x31);
    writeCommand(0xB3); // CLOCKDIV
    writeCommand(0xD0);
    writeCommand(0x8A); // PRECHARGEA
    writeCommand(0x64);
    writeCommand(0x8B); // PRECHARGEB
    writeCommand(0x78);
    writeCommand(0x8C); // PRECHARGEC
    writeCommand(0x64);
    writeCommand(0xBB); // PRECHARGELEVEL
    writeCommand(0x3A);
    writeCommand(0xBE); // VCOMH
    writeCommand(0x3E);
    writeCommand(0x87); // MASTERCURRENT
    writeCommand(0x06);
    writeCommand(0x81); // CONTRASTA
    writeCommand(0x91);
    writeCommand(0x82); // CONTRASTB
    writeCommand(0x50);
    writeCommand(0x83); // CONTRASTC
    writeCommand(0x7D);
    writeCommand(0xAF); // DISPLAYON
}

void SSD1331::hw_reset() {
    gpio_set_level(_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void SSD1331::writeCommand(uint8_t cmd) {
    gpio_set_level(_dc, 0);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(_spi, &t);
}

void SSD1331::writeData(uint8_t data) {
    gpio_set_level(_dc, 1);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    spi_device_transmit(_spi, &t);
}

// Return the size of the display (per current rotation)
int16_t SSD1331::width(void) const {
  return TFTWIDTH;
}

int16_t SSD1331::height(void) const {
  return TFTHEIGHT;
}

void SSD1331::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;
    goTo(x, y);
    writeData((uint8_t)((color >> 11) << 1));
    writeData((uint8_t)((color >> 5) & 0x3F));
    writeData((uint8_t)((color << 1) & 0x3F));
}
void SSD1331::drawChar(int16_t x, int16_t y, char c, uint16_t color) {
   if (c < 0 || c >= 128) return;
    for (int row = 0; row < 8; row++) {
        uint8_t rowBits = font8x8_basic[(uint8_t)c][row];
        for (int col = 0; col < 8; col++) {
            if (rowBits & (1 << col)) {
                drawPixel(x + col, y + row, color);
            }
        }
    }
}

void SSD1331::drawText(int16_t x, int16_t y, const char* text, uint16_t color) {
    while (*text) {
        drawChar(x, y, *text, color);
        x += 6; // 5 pixels + 1 spacing
        text++;
    }
}

void SSD1331::goHome(void) {
    goTo(0, 0);
}

void SSD1331::goTo(int x, int y) {
    writeCommand(0x15); // SSD1331_CMD_SETCOLUMN
    writeCommand(x);
    writeCommand(x);
    writeCommand(0x75); // SSD1331_CMD_SETROW
    writeCommand(y);
    writeCommand(y);
}

void SSD1331::clearWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    writeCommand(0x26); // SSD1331_CMD_FILL
    writeCommand(0x01); // Enable fill

    writeCommand(0x22); // SSD1331_CMD_DRAWRECT
    writeCommand(x0); // Starting column
    writeCommand(y0); // Starting row
    writeCommand(x1); // End column
    writeCommand(y1); // End row

    // Set color to black (0x0000 in RGB565) for both outline and fill
    // R (6-bit), G (6-bit), B (6-bit) -> 0x00 for all
    writeCommand(0x00); // R (outline)
    writeCommand(0x00); // G (outline)
    writeCommand(0x00); // B (outline)
    writeCommand(0x00); // R (fill)
    writeCommand(0x00); // G (fill)
    writeCommand(0x00); // B (fill)

    vTaskDelay(pdMS_TO_TICKS(3)); // Give time for the command to execute
}

void SSD1331::clearScreen() {
    clearWindow(0, 0, TFTWIDTH - 1, TFTHEIGHT - 1);
}

void SSD1331::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= width() || y >= height()) return;
    if (x1 >= width()) x1 = width() - 1;
    if (y1 >= height()) y1 = height() - 1;

    // Enable fill
    writeCommand(0x26);
    writeCommand(0x01);

    // Draw rectangle
    writeCommand(0x22);
    writeCommand(x);
    writeCommand(y);
    writeCommand(x1);
    writeCommand(y1);

    // Convert RGB565 to 6-bit per channel for SSD1331
    uint8_t r6 = ((color >> 11) & 0x1F) << 1; // 5-bit -> 6-bit
    uint8_t g6 = (color >> 5) & 0x3F;         // 6-bit
    uint8_t b6 = (color & 0x1F) << 1;         // 5-bit -> 6-bit

    // Outline color (unused visually when fill=1, set same as fill)
    writeCommand(r6);
    writeCommand(g6);
    writeCommand(b6);
    // Fill color
    writeCommand(r6);
    writeCommand(g6);
    writeCommand(b6);

    vTaskDelay(pdMS_TO_TICKS(1));
}

void SSD1331::fillScreen(uint16_t color) {
    fillRect(0, 0, TFTWIDTH, TFTHEIGHT, color);
}

void SSD1331::sendData_chunked(const uint8_t *data, size_t len) {
    const size_t MAX_CHUNK = 4092;  // safe value < 4096
    size_t sent = 0;

    while (sent < len) {
        size_t chunk = (len - sent) > MAX_CHUNK ? MAX_CHUNK : (len - sent);
        gpio_set_level(_dc, 1); // Data mode

        spi_transaction_t t = {};
        t.length = chunk * 8;
        t.tx_buffer = data + sent;

        esp_err_t ret = spi_device_transmit(_spi, &t);
        if (ret != ESP_OK) {
            ESP_LOGE("SSD1331", "sendData_chunked() thất bại: %s", esp_err_to_name(ret));
            return;
        }

        sent += chunk;
    }
}


void SSD1331::sendData(const uint8_t *data, size_t len) {
    if (!_spi || !data || len == 0) return;

    // Đặt chân DC = 1 để báo là data
    gpio_set_level(_dc, 1); // 'dc_pin' là biến thành viên bạn cần khai báo trong SSD1331 class

    spi_transaction_t t = {};
    t.length = len * 8;              // tính theo bit
    t.tx_buffer = data;
    t.user = NULL;                   // không cần user field nếu không dùng pre/post callbacks

    esp_err_t ret = spi_device_transmit(_spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE("SSD1331", "Gửi SPI data thất bại: %s", esp_err_to_name(ret));
    }
}

void SSD1331::drawBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h)
{
    if (!_spi) return;

    // 1. Set column and row range (window)
    writeCommand(0x15); // Set column address
    writeCommand(x);    // Start
    writeCommand(x + w - 1); // End

    writeCommand(0x75); // Set row address
    writeCommand(y);    // Start
    writeCommand(y + h - 1); // End

    writeCommand(0x5C); // Enable RAM write

    // 2. Convert RGB565 to SSD1331 18-bit format (6-bit per channel)
    size_t pixelCount = w * h;
    size_t bufSize = pixelCount * 3;
    uint8_t* spi_buf = (uint8_t*)heap_caps_malloc(bufSize, MALLOC_CAP_DMA);
    if (!spi_buf) {
        ESP_LOGE("SSD1331", "Không cấp phát được bộ nhớ SPI");
        return;
    }

    for (size_t i = 0; i < pixelCount; ++i) {
        uint16_t color = bitmap[i];
        uint8_t r = (color >> 11) & 0x1F;
        uint8_t g = (color >> 5) & 0x3F;
        uint8_t b = color & 0x1F;

        // SSD1331 dùng 6-bit mỗi thành phần
        spi_buf[i * 3 + 0] = r << 1; // R
        spi_buf[i * 3 + 1] = g;      // G
        spi_buf[i * 3 + 2] = b << 1; // B
    }

    sendData_chunked(spi_buf, bufSize);
    free(spi_buf);
}
