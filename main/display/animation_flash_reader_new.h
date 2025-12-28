#ifndef ANIMATION_FLASH_READER_NEW_H
#define ANIMATION_FLASH_READER_NEW_H

#include "ssd1331.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_partition.h"

// Streaming-friendly reader that keeps only small buffers in RAM.
class AnimationFlashReader {
public:
    struct AnimationEntry {
        char name[32];
        uint32_t offset;
        uint32_t length;
    };

    // partition_label: name of a DATA partition that stores animations.
    // base_offset_bytes: offset inside that partition where the PACK header starts.
    AnimationFlashReader(const char* partition_label = "animations", uint32_t base_offset_bytes = 0);

    // Initialize partition handle and load animation table.
    bool begin();

    // Reload the animation table if contents changed.
    bool reload();

    // Enumerate animation names.
    std::vector<std::string> getAnimationNames() const;

    // Play by name; returns false on any read/parse error.
    bool play(const char* animation_name, SSD1331* display, int delay_ms = 100);

    // Get frame count for an entry.
    int getFrameCount(const AnimationEntry& anim) const;

    // Lookup by name (NAME_SIZE comparison).
    const AnimationEntry* findAnimation(const char* name) const;

    // Optional raw write helper (chunks, caller must erase region first).
    bool writeRaw(uint32_t rel_addr, const void* data, size_t len);

private:
    static constexpr size_t MAX_FRAME_PIXELS = 96 * 64;
    static constexpr size_t NAME_SIZE = 32;
    static constexpr size_t MAX_ANIMATIONS = 70;
    static constexpr size_t READ_CHUNK = 256; // Small chunk to limit RAM

    bool loadAnimationTable();
    bool read(uint32_t rel_addr, void* out, size_t len) const;
    bool decodeRLEFrameStreamed(const AnimationEntry& anim, int frame_idx, std::vector<uint16_t>& out_frame);

    const esp_partition_t* partition_ = nullptr;
    std::string partition_label_;
    uint32_t base_offset_ = 0;
    std::vector<AnimationEntry> animations_;
    std::vector<uint16_t> frame_buffer_; // Reused frame buffer (~12 KB)
    mutable std::array<uint8_t, READ_CHUNK + 3> rle_chunk_{}; // +3 for carryover
};

// Example usage:
// AnimationFlashReader reader("animations", 0);
// reader.begin();
// reader.play("welcome", &display, 50);

#endif // ANIMATION_FLASH_READER_NEW_H
