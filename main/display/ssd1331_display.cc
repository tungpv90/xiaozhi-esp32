#include "ssd1331_display.h"
#include "font8x8_basic.h"
#include <cstring>
#include <string.h>
#include <cstdio>
#include <algorithm>
#include <esp_log.h>
#include "esp_partition.h"

static const char* TAG = "Ssd1331Display";

Ssd1331Display::Ssd1331Display(spi_device_handle_t spi,
                               gpio_num_t dc,
                               gpio_num_t rst,
                               int width,
                               int height)
    : lcd_(spi, dc, rst) {
    width_ = width;
    height_ = height;

    mutex_ = xSemaphoreCreateRecursiveMutex();

    lcd_.begin();
    lcd_.clearScreen();
    
    /*
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "animations"
    );

    if (!part) {
        ESP_LOGE(TAG, "animations partition not found");
        return;
    }

    uint8_t test[16] = {0};
    esp_err_t err = esp_partition_read(part, 0, test, sizeof(test));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "partition read failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "partition addr=0x%08lx size=%lu",
            (unsigned long)part->address, (unsigned long)part->size);

    ESP_LOG_BUFFER_HEX(TAG, test, sizeof(test));
*/
    lcd_.fillScreen(0xFF00);
    // Simple boot marker
    //lcd_.drawText(0, 0, "Booting...", 0xFF00);

    // Auto-play the "meter" animation at startup if available
    if (initAnimations() && loadAnimation("meter")) {
        playAnimation(false);
    }
}

Ssd1331Display::~Ssd1331Display() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

bool Ssd1331Display::Lock(int timeout_ms) {
    if (!mutex_) return true;
    TickType_t ticks = (timeout_ms <= 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(mutex_, ticks) == pdTRUE;
}

void Ssd1331Display::Unlock() {
    if (!mutex_) return;
    xSemaphoreGiveRecursive(mutex_);
}

void Ssd1331Display::Clear() {
    lcd_.clearScreen();
}

void Ssd1331Display::DrawTextLines(int16_t x, int16_t y, const char* text, uint16_t color) {
    return;
    if (!text) return;
    // 8x8 font; we advance 6 pixels per char for compactness
    const int16_t char_w = 6;
    const int16_t char_h = 8;
    int16_t cx = x;
    int16_t cy = y;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') {
            cx = x;
            cy += char_h;
            if (cy + char_h > height_) break;
            continue;
        }
        if (cx + char_w > width_) {
            cx = x;
            cy += char_h;
            if (cy + char_h > height_) break;
        }
        lcd_.drawChar(cx, cy, *p, color);
        cx += char_w;
    }
}

void Ssd1331Display::SetStatus(const char* status) {
    return;
    ESP_LOGI(TAG, "SetStatus: %s", status ? status : "");
    DisplayLockGuard lock(this);
    if (power_save_) return;
    Clear();
    DrawTextLines(0, 0, status ? status : "", 0xFFFF);
}

void Ssd1331Display::ShowNotification(const char* notification, int /*duration_ms*/) {
    return;
    ESP_LOGI(TAG, "ShowNotification: %s", notification ? notification : "");
    DisplayLockGuard lock(this);
    if (power_save_) return;
    Clear();
    DrawTextLines(0, 0, notification ? notification : "", 0x07E0); // green
}

void Ssd1331Display::SetEmotion(const char* emotion) {
    return;
    ESP_LOGI(TAG, "SetEmotion: %s", emotion ? emotion : "");
    DisplayLockGuard lock(this);
    if (power_save_) return;
    Clear();
    // Minimal representation: show emotion text
    DrawTextLines(0, 0, emotion ? emotion : "", 0xF800); // red
}

void Ssd1331Display::SetChatMessage(const char* role, const char* content) {
    return;
    ESP_LOGI(TAG, "SetChatMessage: %s: %s", role ? role : "", content ? content : "");
    DisplayLockGuard lock(this);
    if (power_save_) return;
    Clear();
    char buf[160] = {0};
    int n = 0;
    if (role && *role) {
        n = snprintf(buf, sizeof(buf), "%s: ", role);
        if (n < 0) n = 0; else if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    }
    if (content && n < (int)sizeof(buf)) {
        snprintf(buf + n, sizeof(buf) - n, "%s", content);
    }
    DrawTextLines(0, 0, buf, 0xFFFF);
}

void Ssd1331Display::SetTheme(Theme* theme) {
    Display::SetTheme(theme);
}

void Ssd1331Display::UpdateStatusBar(bool /*update_all*/) {
    // No-op for this lightweight display
}

void Ssd1331Display::SetPowerSaveMode(bool on) {
    DisplayLockGuard lock(this);
    power_save_ = on;
    if (on) {
        Clear();
    }
}
bool Ssd1331Display::initAnimations() {
    return anim_reader_.begin();
}

bool Ssd1331Display::loadAnimation(const char* animation_name) {
    if (!animation_name || !*animation_name) return false;
    if (!anim_reader_.begin()) return false; // ensure table loaded
    if (!anim_reader_.findAnimation(animation_name)) {
        ESP_LOGE(TAG, "Animation not found: %s", animation_name);
        animation_loaded_ = false;
        return false;
    }
    current_anim_name_ = animation_name;
    animation_loaded_ = true;
    return true;
}

void Ssd1331Display::playAnimation(bool loop) {
    if (!animation_loaded_) return;
    do {
        // Streamed playback from internal flash; uses shared frame buffer to minimize RAM
        anim_reader_.play(current_anim_name_.c_str(), &lcd_, 33);
    } while (loop);
}
