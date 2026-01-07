#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Xiaozhi ESP32 - OGG Audio Optimizer
Chuyển đổi file audio sang OGG với dung lượng thấp nhất nhưng vẫn giữ các yếu tố gốc.

Sử dụng Opus codec - codec audio tốt nhất cho chất lượng cao với bitrate thấp.

Cách sử dụng:
    python optimize_ogg.py input.wav                    # Convert 1 file
    python optimize_ogg.py input.mp3 -o output.ogg      # Convert với output path
    python optimize_ogg.py *.wav -d output_dir          # Batch convert
    python optimize_ogg.py input.wav --preset voice     # Dùng preset tối ưu cho giọng nói
    python optimize_ogg.py input.wav --preset music     # Dùng preset tối ưu cho nhạc
    python optimize_ogg.py input.wav --analyze          # Phân tích trước khi convert

Yêu cầu:
    - Python 3.7+
    - ffmpeg (cài đặt và thêm vào PATH)
    - ffprobe (thường đi kèm ffmpeg)

Cài đặt ffmpeg:
    - Windows: https://ffmpeg.org/download.html hoặc choco install ffmpeg
    - Mac: brew install ffmpeg
    - Linux: apt install ffmpeg / yum install ffmpeg
"""

import os
import sys
import argparse
import subprocess
import json
import shutil
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, List, Tuple


@dataclass
class AudioInfo:
    """Thông tin file audio"""
    duration: float          # Thời lượng (giây)
    sample_rate: int         # Tần số lấy mẫu
    channels: int            # Số kênh
    bitrate: Optional[int]   # Bitrate gốc
    codec: str               # Codec gốc
    file_size: int           # Kích thước file (bytes)


@dataclass
class ConversionSettings:
    """Cài đặt chuyển đổi"""
    bitrate: str             # Bitrate đích (vd: "16k", "24k", "32k")
    sample_rate: int         # Sample rate đích
    channels: int            # Số kênh đích
    frame_duration: int      # Frame duration cho Opus (2.5, 5, 10, 20, 40, 60 ms)
    vbr: str                 # Variable bitrate: "on", "off", "constrained"
    compression_level: int   # Compression level 0-10 (10 = chậm nhất, nén tốt nhất)
    application: str         # "voip", "audio", "lowdelay"
    normalize: bool          # Normalize loudness
    target_lufs: float       # Target loudness (LUFS)


# Presets tối ưu cho các trường hợp sử dụng khác nhau
PRESETS = {
    # Preset mặc định cho ESP32 - tối ưu dung lượng
    "default": ConversionSettings(
        bitrate="16k",
        sample_rate=16000,
        channels=1,
        frame_duration=60,
        vbr="on",
        compression_level=10,
        application="audio",
        normalize=True,
        target_lufs=-16.0
    ),
    
    # Preset cho giọng nói/TTS - dung lượng rất thấp
    "voice": ConversionSettings(
        bitrate="12k",
        sample_rate=16000,
        channels=1,
        frame_duration=60,
        vbr="on",
        compression_level=10,
        application="voip",
        normalize=True,
        target_lufs=-16.0
    ),
    
    # Preset cho âm thanh hiệu ứng ngắn
    "sfx": ConversionSettings(
        bitrate="24k",
        sample_rate=24000,
        channels=1,
        frame_duration=20,
        vbr="on",
        compression_level=10,
        application="lowdelay",
        normalize=True,
        target_lufs=-14.0
    ),
    
    # Preset cho nhạc - chất lượng cao hơn
    "music": ConversionSettings(
        bitrate="32k",
        sample_rate=24000,
        channels=1,
        frame_duration=60,
        vbr="on",
        compression_level=10,
        application="audio",
        normalize=True,
        target_lufs=-14.0
    ),
    
    # Preset ultra-low cho dung lượng cực thấp (hi sinh chất lượng)
    "ultralow": ConversionSettings(
        bitrate="8k",
        sample_rate=8000,
        channels=1,
        frame_duration=60,
        vbr="on",
        compression_level=10,
        application="voip",
        normalize=True,
        target_lufs=-16.0
    ),
    
    # Preset tương thích với cấu hình hiện tại của project
    "xiaozhi": ConversionSettings(
        bitrate="16k",
        sample_rate=16000,
        channels=1,
        frame_duration=60,
        vbr="on",
        compression_level=10,
        application="audio",
        normalize=True,
        target_lufs=-16.0
    ),
}


def check_ffmpeg() -> bool:
    """Kiểm tra ffmpeg đã được cài đặt chưa"""
    try:
        subprocess.run(
            ["ffmpeg", "-version"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True
        )
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def check_ffprobe() -> bool:
    """Kiểm tra ffprobe đã được cài đặt chưa"""
    try:
        subprocess.run(
            ["ffprobe", "-version"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True
        )
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def get_audio_info(file_path: str) -> Optional[AudioInfo]:
    """Lấy thông tin file audio bằng ffprobe"""
    try:
        result = subprocess.run(
            [
                "ffprobe",
                "-v", "quiet",
                "-print_format", "json",
                "-show_format",
                "-show_streams",
                file_path
            ],
            capture_output=True,
            text=True,
            check=True
        )
        
        data = json.loads(result.stdout)
        
        # Tìm audio stream
        audio_stream = None
        for stream in data.get("streams", []):
            if stream.get("codec_type") == "audio":
                audio_stream = stream
                break
        
        if not audio_stream:
            print(f"⚠ Không tìm thấy audio stream trong: {file_path}")
            return None
        
        format_info = data.get("format", {})
        
        return AudioInfo(
            duration=float(format_info.get("duration", 0)),
            sample_rate=int(audio_stream.get("sample_rate", 0)),
            channels=int(audio_stream.get("channels", 1)),
            bitrate=int(format_info.get("bit_rate", 0)) if format_info.get("bit_rate") else None,
            codec=audio_stream.get("codec_name", "unknown"),
            file_size=int(format_info.get("size", 0))
        )
    except Exception as e:
        print(f"⚠ Lỗi khi đọc thông tin file: {e}")
        return None


def format_size(size_bytes: int) -> str:
    """Format kích thước file"""
    if size_bytes < 1024:
        return f"{size_bytes} B"
    elif size_bytes < 1024 * 1024:
        return f"{size_bytes / 1024:.2f} KB"
    else:
        return f"{size_bytes / (1024 * 1024):.2f} MB"


def format_duration(seconds: float) -> str:
    """Format thời lượng"""
    if seconds < 60:
        return f"{seconds:.2f}s"
    else:
        mins = int(seconds // 60)
        secs = seconds % 60
        return f"{mins}m {secs:.2f}s"


def analyze_audio(file_path: str) -> None:
    """Phân tích file audio và đề xuất cài đặt"""
    info = get_audio_info(file_path)
    if not info:
        return
    
    print(f"\n📊 Phân tích: {os.path.basename(file_path)}")
    print("=" * 50)
    print(f"  📁 Kích thước:   {format_size(info.file_size)}")
    print(f"  ⏱  Thời lượng:   {format_duration(info.duration)}")
    print(f"  🎵 Sample rate:  {info.sample_rate} Hz")
    print(f"  🔊 Channels:     {info.channels}")
    print(f"  💿 Codec:        {info.codec}")
    if info.bitrate:
        print(f"  📶 Bitrate:      {info.bitrate // 1000} kbps")
    
    # Đề xuất preset
    print("\n💡 Đề xuất:")
    
    if info.duration < 3:
        print("  → Preset 'sfx' - File ngắn, phù hợp cho âm thanh hiệu ứng")
        suggested = "sfx"
    elif info.sample_rate >= 44100 and info.channels >= 2:
        print("  → Preset 'music' - Chất lượng cao, có thể là nhạc")
        suggested = "music"
    elif info.sample_rate <= 16000:
        print("  → Preset 'voice' - Sample rate thấp, có thể là giọng nói")
        suggested = "voice"
    else:
        print("  → Preset 'default' - Cân bằng giữa chất lượng và dung lượng")
        suggested = "default"
    
    # Ước tính kích thước sau khi convert
    preset = PRESETS[suggested]
    estimated_bitrate = int(preset.bitrate.replace("k", "")) * 1000
    estimated_size = int((estimated_bitrate * info.duration) / 8)
    
    print(f"\n📦 Ước tính sau khi convert với preset '{suggested}':")
    print(f"   Kích thước: ~{format_size(estimated_size)}")
    print(f"   Giảm: ~{((info.file_size - estimated_size) / info.file_size * 100):.1f}%")


def convert_to_ogg(
    input_path: str,
    output_path: str,
    settings: ConversionSettings,
    verbose: bool = False
) -> Tuple[bool, Optional[int]]:
    """
    Chuyển đổi file audio sang OGG/Opus với cài đặt tối ưu.
    
    Returns:
        Tuple[bool, Optional[int]]: (Thành công, Kích thước file output)
    """
    
    # Build ffmpeg command
    cmd = ["ffmpeg", "-y", "-i", input_path]
    
    # Audio filters
    audio_filters = []
    
    # Normalize loudness nếu được bật
    if settings.normalize:
        audio_filters.append(f"loudnorm=I={settings.target_lufs}:TP=-1.5:LRA=11")
    
    # High-pass filter để loại bỏ tần số cực thấp không cần thiết
    audio_filters.append("highpass=f=80")
    
    # Low-pass filter tương ứng với sample rate đích
    nyquist = settings.sample_rate // 2
    audio_filters.append(f"lowpass=f={nyquist - 100}")
    
    if audio_filters:
        cmd.extend(["-af", ",".join(audio_filters)])
    
    # Output settings
    cmd.extend([
        "-c:a", "libopus",
        "-b:a", settings.bitrate,
        "-ac", str(settings.channels),
        "-ar", str(settings.sample_rate),
        "-frame_duration", str(settings.frame_duration),
        "-vbr", settings.vbr,
        "-compression_level", str(settings.compression_level),
        "-application", settings.application,
    ])
    
    # Output file
    cmd.append(output_path)
    
    if verbose:
        print(f"🔧 Command: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=True
        )
        
        if os.path.exists(output_path):
            output_size = os.path.getsize(output_path)
            return True, output_size
        else:
            return False, None
            
    except subprocess.CalledProcessError as e:
        if verbose:
            print(f"❌ FFmpeg error: {e.stderr}")
        return False, None


def process_file(
    input_path: str,
    output_path: Optional[str],
    output_dir: Optional[str],
    settings: ConversionSettings,
    verbose: bool = False
) -> bool:
    """Xử lý một file"""
    
    input_path = os.path.abspath(input_path)
    
    if not os.path.exists(input_path):
        print(f"❌ File không tồn tại: {input_path}")
        return False
    
    # Xác định output path
    if output_path:
        final_output = output_path
    elif output_dir:
        base_name = os.path.splitext(os.path.basename(input_path))[0]
        final_output = os.path.join(output_dir, f"{base_name}.ogg")
    else:
        base_name = os.path.splitext(input_path)[0]
        final_output = f"{base_name}_optimized.ogg"
    
    # Tạo thư mục output nếu cần
    os.makedirs(os.path.dirname(final_output) or ".", exist_ok=True)
    
    # Lấy thông tin file gốc
    original_info = get_audio_info(input_path)
    original_size = os.path.getsize(input_path) if os.path.exists(input_path) else 0
    
    print(f"\n🎵 Converting: {os.path.basename(input_path)}")
    if original_info:
        print(f"   Original: {format_size(original_size)} | {format_duration(original_info.duration)}")
    
    # Convert
    success, output_size = convert_to_ogg(input_path, final_output, settings, verbose)
    
    if success and output_size:
        reduction = ((original_size - output_size) / original_size * 100) if original_size > 0 else 0
        print(f"   ✅ Output: {format_size(output_size)} (giảm {reduction:.1f}%)")
        print(f"   📁 Saved: {final_output}")
        return True
    else:
        print(f"   ❌ Conversion failed!")
        return False


def process_files(
    input_files: List[str],
    output_path: Optional[str],
    output_dir: Optional[str],
    settings: ConversionSettings,
    verbose: bool = False
) -> Tuple[int, int]:
    """
    Xử lý nhiều files.
    
    Returns:
        Tuple[int, int]: (Số file thành công, Số file thất bại)
    """
    success_count = 0
    fail_count = 0
    
    # Nếu chỉ có 1 file và có output_path, dùng output_path
    # Nếu có nhiều file, phải dùng output_dir
    if len(input_files) > 1 and output_path and not output_dir:
        print("⚠ Nhiều file input, sử dụng output_dir thay vì output_path")
        output_dir = os.path.dirname(output_path) or "output"
        output_path = None
    
    for input_file in input_files:
        if len(input_files) > 1:
            # Batch mode - không dùng output_path
            result = process_file(input_file, None, output_dir, settings, verbose)
        else:
            result = process_file(input_file, output_path, output_dir, settings, verbose)
        
        if result:
            success_count += 1
        else:
            fail_count += 1
    
    return success_count, fail_count


def main():
    parser = argparse.ArgumentParser(
        description="Chuyển đổi audio sang OGG/Opus với dung lượng tối ưu",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ví dụ:
  %(prog)s input.wav                        Convert với preset mặc định
  %(prog)s input.mp3 -o output.ogg          Chỉ định output path
  %(prog)s *.wav -d output_folder           Batch convert nhiều file
  %(prog)s input.wav --preset voice         Dùng preset cho giọng nói
  %(prog)s input.wav --analyze              Phân tích trước khi convert
  %(prog)s input.wav -b 24k -r 24000        Custom bitrate và sample rate

Presets có sẵn:
  default  - Cân bằng (16k, 16kHz) - Mặc định
  voice    - Tối ưu cho giọng nói (12k, 16kHz) - Dung lượng rất thấp
  sfx      - Âm thanh hiệu ứng (24k, 24kHz) - Độ trễ thấp
  music    - Nhạc (32k, 24kHz) - Chất lượng cao hơn
  ultralow - Cực thấp (8k, 8kHz) - Hi sinh chất lượng
  xiaozhi  - Tương thích project (16k, 16kHz)
        """
    )
    
    parser.add_argument("input", nargs="+", help="File(s) audio đầu vào")
    parser.add_argument("-o", "--output", help="File output (chỉ cho 1 file input)")
    parser.add_argument("-d", "--output-dir", help="Thư mục output cho batch convert")
    
    parser.add_argument(
        "-p", "--preset",
        choices=list(PRESETS.keys()),
        default="default",
        help="Preset cài đặt (default: default)"
    )
    
    # Custom settings (override preset)
    parser.add_argument("-b", "--bitrate", help="Bitrate (vd: 16k, 24k, 32k)")
    parser.add_argument("-r", "--sample-rate", type=int, help="Sample rate (vd: 8000, 16000, 24000)")
    parser.add_argument("-c", "--channels", type=int, choices=[1, 2], help="Số kênh (1=mono, 2=stereo)")
    parser.add_argument("--frame-duration", type=int, choices=[2, 5, 10, 20, 40, 60], help="Frame duration (ms)")
    parser.add_argument("--no-normalize", action="store_true", help="Tắt normalize loudness")
    parser.add_argument("--target-lufs", type=float, help="Target loudness (LUFS)")
    
    parser.add_argument("--analyze", action="store_true", help="Chỉ phân tích, không convert")
    parser.add_argument("-v", "--verbose", action="store_true", help="Hiển thị chi tiết")
    parser.add_argument("--list-presets", action="store_true", help="Liệt kê tất cả presets")
    
    args = parser.parse_args()
    
    # List presets
    if args.list_presets:
        print("\n📋 Danh sách Presets:\n")
        for name, preset in PRESETS.items():
            print(f"  {name}:")
            print(f"    Bitrate: {preset.bitrate}, Sample Rate: {preset.sample_rate}Hz")
            print(f"    Channels: {preset.channels}, Frame Duration: {preset.frame_duration}ms")
            print(f"    Application: {preset.application}, VBR: {preset.vbr}")
            print()
        return
    
    # Check ffmpeg
    if not check_ffmpeg():
        print("❌ FFmpeg chưa được cài đặt hoặc không có trong PATH!")
        print("   Vui lòng cài đặt từ: https://ffmpeg.org/download.html")
        sys.exit(1)
    
    if args.analyze and not check_ffprobe():
        print("❌ FFprobe chưa được cài đặt hoặc không có trong PATH!")
        sys.exit(1)
    
    # Analyze mode
    if args.analyze:
        for input_file in args.input:
            analyze_audio(input_file)
        return
    
    # Load preset và apply custom settings
    settings = PRESETS[args.preset]
    
    # Override với custom settings nếu có
    if args.bitrate:
        settings = ConversionSettings(
            bitrate=args.bitrate,
            sample_rate=settings.sample_rate,
            channels=settings.channels,
            frame_duration=settings.frame_duration,
            vbr=settings.vbr,
            compression_level=settings.compression_level,
            application=settings.application,
            normalize=settings.normalize,
            target_lufs=settings.target_lufs
        )
    
    if args.sample_rate:
        settings = ConversionSettings(
            bitrate=settings.bitrate,
            sample_rate=args.sample_rate,
            channels=settings.channels,
            frame_duration=settings.frame_duration,
            vbr=settings.vbr,
            compression_level=settings.compression_level,
            application=settings.application,
            normalize=settings.normalize,
            target_lufs=settings.target_lufs
        )
    
    if args.channels:
        settings = ConversionSettings(
            bitrate=settings.bitrate,
            sample_rate=settings.sample_rate,
            channels=args.channels,
            frame_duration=settings.frame_duration,
            vbr=settings.vbr,
            compression_level=settings.compression_level,
            application=settings.application,
            normalize=settings.normalize,
            target_lufs=settings.target_lufs
        )
    
    if args.frame_duration:
        settings = ConversionSettings(
            bitrate=settings.bitrate,
            sample_rate=settings.sample_rate,
            channels=settings.channels,
            frame_duration=args.frame_duration,
            vbr=settings.vbr,
            compression_level=settings.compression_level,
            application=settings.application,
            normalize=settings.normalize,
            target_lufs=settings.target_lufs
        )
    
    if args.no_normalize:
        settings = ConversionSettings(
            bitrate=settings.bitrate,
            sample_rate=settings.sample_rate,
            channels=settings.channels,
            frame_duration=settings.frame_duration,
            vbr=settings.vbr,
            compression_level=settings.compression_level,
            application=settings.application,
            normalize=False,
            target_lufs=settings.target_lufs
        )
    
    if args.target_lufs:
        settings = ConversionSettings(
            bitrate=settings.bitrate,
            sample_rate=settings.sample_rate,
            channels=settings.channels,
            frame_duration=settings.frame_duration,
            vbr=settings.vbr,
            compression_level=settings.compression_level,
            application=settings.application,
            normalize=settings.normalize,
            target_lufs=args.target_lufs
        )
    
    print(f"\n🔧 Sử dụng preset: {args.preset}")
    print(f"   Bitrate: {settings.bitrate}, Sample Rate: {settings.sample_rate}Hz")
    print(f"   Channels: {settings.channels}, Frame Duration: {settings.frame_duration}ms")
    
    # Process files
    success, fail = process_files(
        args.input,
        args.output,
        args.output_dir,
        settings,
        args.verbose
    )
    
    print(f"\n📊 Kết quả: {success} thành công, {fail} thất bại")
    
    if fail > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
