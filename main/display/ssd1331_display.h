#ifndef SSD1331_DISPLAY_H
#define SSD1331_DISPLAY_H

#include "display.h"
#include "ssd1331.h"
#include "animation_flash_reader_new.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>

class Ssd1331Display : public Display {
public:
    Ssd1331Display(spi_device_handle_t spi,
                   gpio_num_t dc,
                   gpio_num_t rst,
                   int width = TFTWIDTH,
                   int height = TFTHEIGHT);
    virtual ~Ssd1331Display();

    // Display interface
    virtual void SetStatus(const char* status) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetTheme(Theme* theme) override;
    virtual void UpdateStatusBar(bool update_all = false) override;
    virtual void SetPowerSaveMode(bool on) override;

    // Animation API
    bool initAnimations();
    bool loadAnimation(const char* animation_name);
    void playAnimation(bool loop);

protected:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

private:
    SSD1331 lcd_;
    SemaphoreHandle_t mutex_ = nullptr;
    bool power_save_ = false;

    // Animations (delegated to internal-flash AnimationFlashReader)
    AnimationFlashReader anim_reader_{};
    std::string current_anim_name_;
    bool animation_loaded_ = false;
    static constexpr int kWidth = TFTWIDTH;
    static constexpr int kHeight = TFTHEIGHT;

    void Clear();
    void DrawTextLines(int16_t x, int16_t y, const char* text, uint16_t color);
};

#endif // SSD1331_DISPLAY_H
