# Xiaozhi ESP32 - OGG Audio Converter / Optimizer

Bộ công cụ chuyển đổi và tối ưu hóa file audio sang định dạng OGG/Opus cho ESP32.

## Các công cụ có sẵn

| Script | Mô tả |
|--------|-------|
| `optimize_ogg.py` | **CLI tool chính** - Nhiều tính năng, presets, phân tích audio |
| `xiaozhi_ogg_converter.py` | GUI tool với giao diện đồ họa |
| `convert_ogg.bat` | Batch script cho Windows - Đơn giản, nhanh chóng |
| `convert_ogg.sh` | Shell script cho Linux/Mac |

## Yêu cầu

- **FFmpeg** (bắt buộc) - Cài đặt từ: https://ffmpeg.org/download.html
  - Windows: `choco install ffmpeg` hoặc download từ website
  - Mac: `brew install ffmpeg`
  - Linux: `apt install ffmpeg` / `yum install ffmpeg`

- **Python 3.7+** (cho script Python)

## Cài đặt nhanh

```bash
# Tạo và kích hoạt virtual environment
python -m venv venv
source venv/bin/activate  # Mac/Linux
venv\Scripts\activate     # Windows

# Cài đặt dependencies (cho GUI tool)
pip install ffmpeg-python
```

---

## 1. optimize_ogg.py (Khuyến nghị)

CLI tool mạnh mẽ với nhiều presets và tùy chọn tối ưu hóa.

### Cách sử dụng

```bash
# Convert 1 file với preset mặc định
python optimize_ogg.py input.wav

# Convert với output path cụ thể
python optimize_ogg.py input.mp3 -o output.ogg

# Batch convert nhiều file
python optimize_ogg.py *.wav -d output_folder

# Sử dụng preset cho giọng nói (dung lượng rất thấp)
python optimize_ogg.py input.wav --preset voice

# Sử dụng preset cho nhạc (chất lượng cao hơn)
python optimize_ogg.py input.wav --preset music

# Sử dụng preset cho âm thanh hiệu ứng
python optimize_ogg.py input.wav --preset sfx

# Phân tích file trước khi convert
python optimize_ogg.py input.wav --analyze

# Custom bitrate và sample rate
python optimize_ogg.py input.wav -b 24k -r 24000

# Liệt kê tất cả presets
python optimize_ogg.py --list-presets
```

### Presets có sẵn

| Preset | Bitrate | Sample Rate | Mô tả |
|--------|---------|-------------|-------|
| `default` | 16k | 16kHz | Cân bằng chất lượng/dung lượng |
| `voice` | 12k | 16kHz | Tối ưu cho giọng nói, TTS |
| `sfx` | 24k | 24kHz | Âm thanh hiệu ứng ngắn |
| `music` | 32k | 24kHz | Nhạc, chất lượng cao |
| `ultralow` | 8k | 8kHz | Dung lượng cực thấp |
| `xiaozhi` | 16k | 16kHz | Tương thích project hiện tại |

### Tính năng

- ✅ Nhiều presets tối ưu cho từng loại audio
- ✅ Phân tích file và đề xuất preset phù hợp
- ✅ Normalize loudness tự động (LUFS)
- ✅ High-pass/Low-pass filter để loại bỏ tần số không cần thiết
- ✅ VBR (Variable Bitrate) cho nén tối ưu
- ✅ Compression level cao nhất
- ✅ Hiển thị % giảm dung lượng

---

## 2. xiaozhi_ogg_converter.py (GUI)

Tool với giao diện đồ họa, dễ sử dụng.

```bash
python xiaozhi_ogg_converter.py
```

**Tính năng:**
- Chọn file bằng file dialog
- Chuyển đổi 2 chiều: Audio ↔ OGG
- Điều chỉnh loudness (LUFS)

---

## 3. convert_ogg.bat (Windows)

Script batch đơn giản cho Windows.

**Cách sử dụng:**
1. Copy file audio vào thư mục `input/`
2. Chạy `convert_ogg.bat`
3. File output sẽ ở thư mục `output/`

---

## 4. convert_ogg.sh (Linux/Mac)

```bash
# Cấp quyền thực thi
chmod +x convert_ogg.sh

# Convert 1 file
./convert_ogg.sh input.wav

# Sử dụng preset
./convert_ogg.sh --preset voice input.wav

# Batch convert
./convert_ogg.sh *.mp3
```

---

## Thông số kỹ thuật tối ưu cho ESP32

Cấu hình được tối ưu cho ESP32 với esp-opus decoder:

| Thông số | Giá trị | Lý do |
|----------|---------|-------|
| Codec | Opus | Chất lượng tốt nhất ở bitrate thấp |
| Bitrate | 16kbps | Cân bằng chất lượng/dung lượng |
| Sample Rate | 16kHz | Đủ cho giọng nói và âm thanh |
| Channels | Mono | Giảm 50% dung lượng |
| Frame Duration | 60ms | Giảm overhead, phù hợp cho audio |
| VBR | On | Tối ưu bitrate cho từng phần |
| Compression | 10 | Nén tối đa |

## So sánh dung lượng

Ví dụ với file WAV 10 giây, 44.1kHz, stereo (~1.7MB):

| Preset | Dung lượng | Giảm |
|--------|------------|------|
| Original WAV | ~1.7MB | - |
| music (32k) | ~40KB | 97.6% |
| default (16k) | ~20KB | 98.8% |
| voice (12k) | ~15KB | 99.1% |
| ultralow (8k) | ~10KB | 99.4% |

---

## Ví dụ thực tế

### Kết quả convert sound.ogg với preset sfx:

| Property | Value |
|----------|-------|
| **Original Size** | 145.11 KB |
| **Duration** | 5.98 seconds |
| **Output Size** | 23.02 KB |
| **Reduction** | 84.1% |
| **Preset Used** | sfx |

**Command:**
```bash
python optimize_ogg.py sound.ogg --preset sfx
```

---

## Troubleshooting

### FFmpeg không tìm thấy
```
Thêm ffmpeg vào PATH hoặc copy ffmpeg.exe vào thư mục script
```

### Lỗi "libopus encoder not found"
```
Cài lại ffmpeg với hỗ trợ libopus:
brew install ffmpeg --with-opus  # Mac
```

### File output không phát được
```
Kiểm tra sample rate không quá thấp (tối thiểu 8000Hz)
Thử preset 'default' thay vì 'ultralow'
```

---

## 中文说明 (Chinese)

本工具是OGG批量转换工具，支持将输入的音频文件转换为小智可使用的OGG格式。

### 快速使用

```bash
# 使用Python CLI工具（推荐）
python optimize_ogg.py input.wav --preset voice

# 分析文件
python optimize_ogg.py input.wav --analyze

# 使用GUI工具
python xiaozhi_ogg_converter.py
```

基于 `ffmpeg` 实现，需要先安装 ffmpeg 环境。
