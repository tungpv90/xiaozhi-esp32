#!/bin/bash
# ============================================================
# Xiaozhi ESP32 - OGG Optimizer Shell Script
# Chuyển đổi file audio sang OGG với dung lượng tối ưu
# ============================================================
# Cách sử dụng:
#   ./convert_ogg.sh input.wav                 # Convert 1 file
#   ./convert_ogg.sh input.wav output.ogg      # Convert với output cụ thể
#   ./convert_ogg.sh *.wav                     # Batch convert
#   ./convert_ogg.sh --preset voice input.wav  # Dùng preset
# ============================================================

# Màu sắc
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Cấu hình mặc định (preset: default)
BITRATE="16k"
SAMPLE_RATE="16000"
CHANNELS="1"
FRAME_DURATION="60"
VBR="on"
COMPRESSION_LEVEL="10"
APPLICATION="audio"
TARGET_LUFS="-16"
NORMALIZE="yes"

# Thư mục output mặc định
OUTPUT_DIR="./output"

# Hàm hiển thị usage
show_usage() {
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}  Xiaozhi ESP32 - OGG Audio Optimizer${NC}"
    echo -e "${BLUE}============================================================${NC}"
    echo ""
    echo "Cách sử dụng:"
    echo "  $0 [options] <input_file(s)>"
    echo ""
    echo "Options:"
    echo "  -o, --output <file>      File output (chỉ cho 1 file input)"
    echo "  -d, --output-dir <dir>   Thư mục output"
    echo "  -p, --preset <preset>    Preset: default, voice, sfx, music, ultralow"
    echo "  -b, --bitrate <rate>     Bitrate (vd: 16k, 24k, 32k)"
    echo "  -r, --sample-rate <rate> Sample rate (vd: 8000, 16000, 24000)"
    echo "  --no-normalize           Tắt normalize loudness"
    echo "  -v, --verbose            Hiển thị chi tiết"
    echo "  -h, --help               Hiển thị help"
    echo ""
    echo "Presets:"
    echo "  default  - Cân bằng (16k, 16kHz)"
    echo "  voice    - Giọng nói (12k, 16kHz) - Dung lượng rất thấp"
    echo "  sfx      - Hiệu ứng (24k, 24kHz) - Độ trễ thấp"
    echo "  music    - Nhạc (32k, 24kHz) - Chất lượng cao"
    echo "  ultralow - Cực thấp (8k, 8kHz)"
    echo ""
    echo "Ví dụ:"
    echo "  $0 sound.wav"
    echo "  $0 --preset voice *.mp3"
    echo "  $0 -b 24k -r 24000 music.flac"
}

# Hàm apply preset
apply_preset() {
    case $1 in
        "voice")
            BITRATE="12k"
            SAMPLE_RATE="16000"
            APPLICATION="voip"
            ;;
        "sfx")
            BITRATE="24k"
            SAMPLE_RATE="24000"
            FRAME_DURATION="20"
            APPLICATION="lowdelay"
            TARGET_LUFS="-14"
            ;;
        "music")
            BITRATE="32k"
            SAMPLE_RATE="24000"
            TARGET_LUFS="-14"
            ;;
        "ultralow")
            BITRATE="8k"
            SAMPLE_RATE="8000"
            APPLICATION="voip"
            ;;
        "default"|"xiaozhi")
            # Giữ nguyên default
            ;;
        *)
            echo -e "${RED}[ERROR] Preset không hợp lệ: $1${NC}"
            exit 1
            ;;
    esac
}

# Hàm format size
format_size() {
    local size=$1
    if [ $size -lt 1024 ]; then
        echo "${size}B"
    elif [ $size -lt 1048576 ]; then
        echo "$(echo "scale=2; $size/1024" | bc)KB"
    else
        echo "$(echo "scale=2; $size/1048576" | bc)MB"
    fi
}

# Hàm convert file
convert_file() {
    local input="$1"
    local output="$2"
    
    # Kiểm tra file input
    if [ ! -f "$input" ]; then
        echo -e "${RED}[ERROR] File không tồn tại: $input${NC}"
        return 1
    fi
    
    # Lấy thông tin file gốc
    local input_size=$(stat -f%z "$input" 2>/dev/null || stat -c%s "$input" 2>/dev/null)
    
    echo -e "${BLUE}[CONVERT]${NC} $(basename "$input")"
    echo "          Input size: $(format_size $input_size)"
    
    # Build audio filter
    local af_filter=""
    if [ "$NORMALIZE" = "yes" ]; then
        af_filter="loudnorm=I=${TARGET_LUFS}:TP=-1.5:LRA=11,"
    fi
    
    # Low-pass frequency based on sample rate
    local lowpass_freq=$((SAMPLE_RATE / 2 - 100))
    af_filter="${af_filter}highpass=f=80,lowpass=f=${lowpass_freq}"
    
    # FFmpeg command
    if [ "$VERBOSE" = "yes" ]; then
        ffmpeg -y -i "$input" \
            -af "$af_filter" \
            -c:a libopus \
            -b:a "$BITRATE" \
            -ac "$CHANNELS" \
            -ar "$SAMPLE_RATE" \
            -frame_duration "$FRAME_DURATION" \
            -vbr "$VBR" \
            -compression_level "$COMPRESSION_LEVEL" \
            -application "$APPLICATION" \
            "$output"
    else
        ffmpeg -y -i "$input" \
            -af "$af_filter" \
            -c:a libopus \
            -b:a "$BITRATE" \
            -ac "$CHANNELS" \
            -ar "$SAMPLE_RATE" \
            -frame_duration "$FRAME_DURATION" \
            -vbr "$VBR" \
            -compression_level "$COMPRESSION_LEVEL" \
            -application "$APPLICATION" \
            "$output" 2>/dev/null
    fi
    
    if [ $? -eq 0 ] && [ -f "$output" ]; then
        local output_size=$(stat -f%z "$output" 2>/dev/null || stat -c%s "$output" 2>/dev/null)
        local reduction=$(echo "scale=1; (1 - $output_size/$input_size) * 100" | bc)
        echo -e "          ${GREEN}✓ Output: $(basename "$output")${NC}"
        echo -e "          Size: $(format_size $output_size) (giảm ${reduction}%)"
        return 0
    else
        echo -e "          ${RED}✗ Conversion failed${NC}"
        return 1
    fi
}

# Kiểm tra ffmpeg
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${RED}[ERROR] FFmpeg chưa được cài đặt!${NC}"
    echo "        Cài đặt:"
    echo "        - Mac: brew install ffmpeg"
    echo "        - Ubuntu/Debian: sudo apt install ffmpeg"
    echo "        - CentOS/RHEL: sudo yum install ffmpeg"
    exit 1
fi

# Parse arguments
INPUT_FILES=()
OUTPUT_FILE=""
VERBOSE=""
PRESET=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -o|--output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        -d|--output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -p|--preset)
            PRESET="$2"
            apply_preset "$2"
            shift 2
            ;;
        -b|--bitrate)
            BITRATE="$2"
            shift 2
            ;;
        -r|--sample-rate)
            SAMPLE_RATE="$2"
            shift 2
            ;;
        --no-normalize)
            NORMALIZE="no"
            shift
            ;;
        -v|--verbose)
            VERBOSE="yes"
            shift
            ;;
        -*)
            echo -e "${RED}[ERROR] Unknown option: $1${NC}"
            show_usage
            exit 1
            ;;
        *)
            INPUT_FILES+=("$1")
            shift
            ;;
    esac
done

# Kiểm tra input
if [ ${#INPUT_FILES[@]} -eq 0 ]; then
    echo -e "${RED}[ERROR] Không có file input${NC}"
    show_usage
    exit 1
fi

# Tạo thư mục output nếu cần
mkdir -p "$OUTPUT_DIR"

# Hiển thị cấu hình
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}  Xiaozhi ESP32 - OGG Audio Optimizer${NC}"
echo -e "${BLUE}============================================================${NC}"
echo "  Preset:       ${PRESET:-default}"
echo "  Bitrate:      $BITRATE"
echo "  Sample Rate:  $SAMPLE_RATE Hz"
echo "  Channels:     $CHANNELS (mono)"
echo "  Frame:        $FRAME_DURATION ms"
echo "  Normalize:    $NORMALIZE (LUFS: $TARGET_LUFS)"
echo -e "${BLUE}============================================================${NC}"
echo ""

# Process files
SUCCESS=0
FAIL=0

for input in "${INPUT_FILES[@]}"; do
    # Xác định output path
    if [ -n "$OUTPUT_FILE" ] && [ ${#INPUT_FILES[@]} -eq 1 ]; then
        output="$OUTPUT_FILE"
    else
        basename_noext="${input%.*}"
        basename_only=$(basename "$basename_noext")
        output="$OUTPUT_DIR/${basename_only}.ogg"
    fi
    
    if convert_file "$input" "$output"; then
        ((SUCCESS++))
    else
        ((FAIL++))
    fi
    echo ""
done

# Kết quả
echo -e "${BLUE}============================================================${NC}"
echo -e "  Kết quả: ${GREEN}$SUCCESS thành công${NC}, ${RED}$FAIL thất bại${NC}"
echo -e "${BLUE}============================================================${NC}"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
