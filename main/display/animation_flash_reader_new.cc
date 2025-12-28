#include "animation_flash_reader_new.h"
#include <cstring>
#include "spi_flash_mmap.h"

#define TAG "FLASH_ANIM_INT"

AnimationFlashReader::AnimationFlashReader(const char* partition_label, uint32_t base_offset_bytes)
    : partition_label_(partition_label ? partition_label : ""), base_offset_(base_offset_bytes) {}

bool AnimationFlashReader::begin() {
    if (!partition_) {
        partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, partition_label_.c_str());
        if (!partition_) {
            ESP_LOGE(TAG, "Partition '%s' not found", partition_label_.c_str());
            return false;
        }
        ESP_LOGI(TAG, "Using partition '%s' at 0x%08lx (size %lu)", partition_->label,
                 static_cast<unsigned long>(partition_->address), static_cast<unsigned long>(partition_->size));
        if (base_offset_ >= partition_->size) {
            ESP_LOGE(TAG, "base_offset (%lu) outside partition size %lu", static_cast<unsigned long>(base_offset_),
                     static_cast<unsigned long>(partition_->size));
            return false;
        }
    }
    return loadAnimationTable();
}

bool AnimationFlashReader::reload() { return loadAnimationTable(); }

std::vector<std::string> AnimationFlashReader::getAnimationNames() const {
    std::vector<std::string> names;
    names.reserve(animations_.size());
    for (const auto& anim : animations_) names.emplace_back(anim.name);
    return names;
}

const AnimationFlashReader::AnimationEntry* AnimationFlashReader::findAnimation(const char* name) const {
    if (!name) return nullptr;
    for (const auto& anim : animations_) {
        if (strncmp(anim.name, name, NAME_SIZE) == 0) return &anim;
    }
    return nullptr;
}

bool AnimationFlashReader::read(uint32_t rel_addr, void* out, size_t len) const {
    if (!partition_) return false;
    if (base_offset_ + rel_addr + len > partition_->size) {
        ESP_LOGE(TAG, "Read out of bounds: base=0x%lx rel=0x%lx len=%lu", static_cast<unsigned long>(base_offset_),
                 static_cast<unsigned long>(rel_addr), static_cast<unsigned long>(len));
        return false;
    }
    esp_err_t err = esp_partition_read(partition_, base_offset_ + rel_addr, out, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_partition_read failed (%s) at rel=0x%lx len=%lu", esp_err_to_name(err),
                 static_cast<unsigned long>(rel_addr), static_cast<unsigned long>(len));
        return false;
    }
    return true;
}

bool AnimationFlashReader::writeRaw(uint32_t rel_addr, const void* data, size_t len) {
    if (!partition_) return false;
    if (base_offset_ + rel_addr + len > partition_->size) {
        ESP_LOGE(TAG, "Write out of bounds: base=0x%lx rel=0x%lx len=%lu",
                 static_cast<unsigned long>(base_offset_), static_cast<unsigned long>(rel_addr),
                 static_cast<unsigned long>(len));
        return false;
    }
    const uint8_t* src = static_cast<const uint8_t*>(data);
    uint32_t addr = rel_addr;
    size_t remaining = len;
    while (remaining > 0) {
        size_t chunk = remaining > READ_CHUNK ? READ_CHUNK : remaining;
        esp_err_t err = esp_partition_write(partition_, base_offset_ + addr, src, chunk);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_partition_write failed (%s) at rel=0x%lx len=%lu", esp_err_to_name(err),
                     static_cast<unsigned long>(addr), static_cast<unsigned long>(chunk));
            return false;
        }
        addr += chunk;
        src += chunk;
        remaining -= chunk;
    }
    return true;
}

bool AnimationFlashReader::loadAnimationTable() {
    animations_.clear();
    
    // Log partition details for debugging
    if (partition_) {
        ESP_LOGI(TAG, "Reading from partition '%s' at flash addr 0x%08lx, size %lu, base_offset=%lu",
                 partition_->label,
                 static_cast<unsigned long>(partition_->address),
                 static_cast<unsigned long>(partition_->size),
                 static_cast<unsigned long>(base_offset_));
        
        // DEBUG: Read directly from flash chip at absolute address (bypass partition API)
        uint8_t direct_buf[16];
        uint32_t abs_addr = partition_->address + base_offset_;
        esp_err_t err = esp_flash_read(esp_flash_default_chip, direct_buf, abs_addr, sizeof(direct_buf));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Direct flash read at 0x%08lx: %02X %02X %02X %02X %02X %02X %02X %02X",
                     static_cast<unsigned long>(abs_addr),
                     direct_buf[0], direct_buf[1], direct_buf[2], direct_buf[3],
                     direct_buf[4], direct_buf[5], direct_buf[6], direct_buf[7]);
        } else {
            ESP_LOGE(TAG, "Direct flash read failed: %s", esp_err_to_name(err));
        }
    }
    
    uint8_t magic[4];
    if (!read(0, magic, sizeof(magic))) {
        ESP_LOGE(TAG, "Failed to read magic");
        return false;
    }
    ESP_LOGI(TAG, "Magic via partition API: %02X %02X %02X %02X ('%c%c%c%c')",
             magic[0], magic[1], magic[2], magic[3],
             (magic[0] >= 32 && magic[0] < 127) ? magic[0] : '.',
             (magic[1] >= 32 && magic[1] < 127) ? magic[1] : '.',
             (magic[2] >= 32 && magic[2] < 127) ? magic[2] : '.',
             (magic[3] >= 32 && magic[3] < 127) ? magic[3] : '.');
    if (memcmp(magic, "PACK", 4) != 0) {
        ESP_LOGE(TAG, "Magic invalid, expected 'PACK' at flash addr 0x%08lx",
                 partition_ ? static_cast<unsigned long>(partition_->address + base_offset_) : 0UL);
        return false;
    }

    uint8_t count_buf[2];
    if (!read(4, count_buf, sizeof(count_buf))) return false;
    uint16_t anim_count = static_cast<uint16_t>((count_buf[1] << 8) | count_buf[0]);
    if (anim_count > MAX_ANIMATIONS) {
        ESP_LOGE(TAG, "anim_count too large: %u", anim_count);
        return false;
    }

    uint32_t offset = 6; // magic(4) + count(2)
    animations_.reserve(anim_count);
    for (uint16_t i = 0; i < anim_count; i++) {
        AnimationEntry entry{};
        if (!read(offset, entry.name, NAME_SIZE)) return false;
        offset += NAME_SIZE;

        uint8_t offbuf[4];
        uint8_t lenbuf[4];
        if (!read(offset, offbuf, sizeof(offbuf))) return false;
        offset += sizeof(offbuf);
        if (!read(offset, lenbuf, sizeof(lenbuf))) return false;
        offset += sizeof(lenbuf);

        entry.offset = (static_cast<uint32_t>(offbuf[3]) << 24) | (static_cast<uint32_t>(offbuf[2]) << 16) |
                       (static_cast<uint32_t>(offbuf[1]) << 8) | offbuf[0];
        entry.length = (static_cast<uint32_t>(lenbuf[3]) << 24) | (static_cast<uint32_t>(lenbuf[2]) << 16) |
                       (static_cast<uint32_t>(lenbuf[1]) << 8) | lenbuf[0];
        animations_.push_back(entry);
    }
    ESP_LOGI(TAG, "Loaded %u animation entries", anim_count);
    return true;
}

int AnimationFlashReader::getFrameCount(const AnimationEntry& anim) const {
    uint8_t header[6];
    if (!read(anim.offset, header, sizeof(header)) || memcmp(header, "ANIM", 4) != 0) return 0;
    return static_cast<int>((header[5] << 8) | header[4]);
}

bool AnimationFlashReader::decodeRLEFrameStreamed(const AnimationEntry& anim, int frame_idx,
                                                  std::vector<uint16_t>& out_frame) {
    // ANIM header format (from pack script):
    // ANIM (4) + frame_count (2) + frame_table_offset (4) + frame_data_size (4) + audio_data_size (4) = 18 bytes
    // Then frame_table: each entry is offset(4) + size(4) = 8 bytes per frame
    // Then frame_data, then audio_data
    
    uint8_t header[18];
    if (!read(anim.offset, header, sizeof(header)) || memcmp(header, "ANIM", 4) != 0) {
        ESP_LOGE(TAG, "Invalid ANIM header in decodeRLEFrameStreamed");
        return false;
    }

    uint16_t frame_count = (header[5] << 8) | header[4];
    uint32_t frame_table_offset = (header[9] << 24) | (header[8] << 16) | (header[7] << 8) | header[6];
    // frame_data_size at header[10-13], audio_data_size at header[14-17] - not needed here
    
    if (frame_count == 0 || frame_idx < 0 || frame_idx >= frame_count) {
        ESP_LOGE(TAG, "Invalid frame_idx %d (frame_count=%u)", frame_idx, frame_count);
        return false;
    }

    // Read frame table entry for this frame (offset + size, each 4 bytes)
    uint32_t table_entry_addr = anim.offset + frame_table_offset + frame_idx * 8;
    uint8_t table_entry[8];
    if (!read(table_entry_addr, table_entry, sizeof(table_entry))) {
        ESP_LOGE(TAG, "Failed to read frame table entry");
        return false;
    }
    
    uint32_t frame_offset = (table_entry[3] << 24) | (table_entry[2] << 16) | (table_entry[1] << 8) | table_entry[0];
    uint32_t frame_size = (table_entry[7] << 24) | (table_entry[6] << 16) | (table_entry[5] << 8) | table_entry[4];
    
    // Frame data starts after frame_table (frame_table_offset + frame_count * 8)
    uint32_t frame_data_base = anim.offset + frame_table_offset + frame_count * 8;
    uint32_t rle_addr = frame_data_base + frame_offset;
    
    ESP_LOGD(TAG, "Frame %d: table_entry_addr=0x%lx, offset=%lu, size=%lu, rle_addr=0x%lx",
             frame_idx, (unsigned long)table_entry_addr, (unsigned long)frame_offset,
             (unsigned long)frame_size, (unsigned long)rle_addr);

    out_frame.assign(MAX_FRAME_PIXELS, 0);
    uint32_t remaining = frame_size;
    uint32_t pixel_index = 0;
    size_t carry_len = 0;

    while (remaining > 0 && pixel_index < MAX_FRAME_PIXELS) {
        size_t to_read = remaining > READ_CHUNK ? READ_CHUNK : remaining;
        if (!read(rle_addr, rle_chunk_.data() + carry_len, to_read)) return false;
        rle_addr += to_read;
        remaining -= to_read;

        size_t buf_len = carry_len + to_read;
        size_t parse_len = buf_len - (buf_len % 3); // keep leftover for next loop

        for (size_t pos = 0; pos < parse_len && pixel_index < MAX_FRAME_PIXELS; pos += 3) {
            uint8_t count = rle_chunk_[pos];
            uint16_t color = (static_cast<uint16_t>(rle_chunk_[pos + 1]) << 8) | rle_chunk_[pos + 2];
            for (uint8_t i = 0; i < count && pixel_index < MAX_FRAME_PIXELS; ++i) {
                out_frame[pixel_index++] = color;
            }
        }

        carry_len = buf_len - parse_len;
        if (carry_len > 0) {
            for (size_t i = 0; i < carry_len; ++i) {
                rle_chunk_[i] = rle_chunk_[parse_len + i];
            }
        }
    }

    if (pixel_index < MAX_FRAME_PIXELS) {
        std::fill(out_frame.begin() + pixel_index, out_frame.end(), 0x0000);
    }
    return true;
}

bool AnimationFlashReader::play(const char* animation_name, SSD1331* display, int delay_ms) {
    if (!display) return false;
    const AnimationEntry* anim = findAnimation(animation_name);
    if (!anim) {
        ESP_LOGE(TAG, "Animation '%s' not found", animation_name ? animation_name : "(null)");
        return false;
    }

    // ANIM header: ANIM(4) + frame_count(2) + frame_table_offset(4) + frame_data_size(4) + audio_data_size(4) = 18 bytes
    uint8_t header[18];
    if (!read(anim->offset, header, sizeof(header)) || memcmp(header, "ANIM", 4) != 0) {
        ESP_LOGE(TAG, "Invalid ANIM header at 0x%lx", static_cast<unsigned long>(anim->offset));
        return false;
    }

    uint16_t frame_count = (header[5] << 8) | header[4];
    uint32_t frame_table_offset = (header[9] << 24) | (header[8] << 16) | (header[7] << 8) | header[6];
    uint32_t frame_data_size = (header[13] << 24) | (header[12] << 16) | (header[11] << 8) | header[10];

    ESP_LOGI(TAG, "Play '%s': %u frames, table_offset=%lu, frame_data=%lu", anim->name, frame_count,
             static_cast<unsigned long>(frame_table_offset), static_cast<unsigned long>(frame_data_size));

    if (frame_count == 0 || frame_count > 10000) {
        ESP_LOGE(TAG, "Invalid frame_count %u", frame_count);
        return false;
    }

    frame_buffer_.resize(MAX_FRAME_PIXELS); // reuse single buffer (~12 KB)

    // Stream each frame; no full frame-table allocation
    for (uint16_t f = 0; f < frame_count; ++f) {
        if (!decodeRLEFrameStreamed(*anim, f, frame_buffer_)) {
            ESP_LOGE(TAG, "Decode failed at frame %u", f);
            return false;
        }
        ESP_LOGD(TAG, "Frame %u decoded, first pixel=0x%04x", f, frame_buffer_[0]);
        display->drawBitmap(0, 0, frame_buffer_.data(), 96, 64);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return true;
}
