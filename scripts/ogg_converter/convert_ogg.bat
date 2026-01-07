@echo off
REM ============================================================
REM Xiaozhi ESP32 - OGG Optimizer Batch Script
REM Chuyển đổi tất cả file audio trong thư mục sang OGG tối ưu
REM ============================================================

setlocal enabledelayedexpansion

REM Cấu hình mặc định
set BITRATE=16k
set SAMPLE_RATE=16000
set CHANNELS=1
set FRAME_DURATION=60
set TARGET_LUFS=-16

REM Thư mục
set INPUT_DIR=%~dp0input
set OUTPUT_DIR=%~dp0output

REM Kiểm tra ffmpeg
where ffmpeg >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] FFmpeg khong duoc cai dat hoac khong co trong PATH!
    echo         Vui long cai dat tu: https://ffmpeg.org/download.html
    pause
    exit /b 1
)

REM Tạo thư mục nếu chưa có
if not exist "%INPUT_DIR%" (
    mkdir "%INPUT_DIR%"
    echo [INFO] Da tao thu muc input: %INPUT_DIR%
    echo [INFO] Vui long copy cac file audio vao thu muc input va chay lai script
    pause
    exit /b 0
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo.
echo ============================================================
echo   Xiaozhi ESP32 - OGG Audio Optimizer
echo ============================================================
echo   Bitrate:        %BITRATE%
echo   Sample Rate:    %SAMPLE_RATE% Hz
echo   Channels:       %CHANNELS% (mono)
echo   Frame Duration: %FRAME_DURATION% ms
echo   Target LUFS:    %TARGET_LUFS%
echo ============================================================
echo.

set COUNT=0
set SUCCESS=0
set FAIL=0

REM Xử lý các định dạng audio phổ biến
for %%e in (wav mp3 flac ogg m4a aac wma) do (
    for %%f in ("%INPUT_DIR%\*.%%e") do (
        if exist "%%f" (
            set /a COUNT+=1
            set "FILENAME=%%~nf"
            set "OUTPUT_FILE=%OUTPUT_DIR%\!FILENAME!.ogg"
            
            echo [!COUNT!] Converting: %%~nxf
            
            ffmpeg -y -i "%%f" ^
                -af "loudnorm=I=%TARGET_LUFS%:TP=-1.5:LRA=11,highpass=f=80,lowpass=f=7900" ^
                -c:a libopus ^
                -b:a %BITRATE% ^
                -ac %CHANNELS% ^
                -ar %SAMPLE_RATE% ^
                -frame_duration %FRAME_DURATION% ^
                -vbr on ^
                -compression_level 10 ^
                -application audio ^
                "!OUTPUT_FILE!" 2>nul
            
            if exist "!OUTPUT_FILE!" (
                set /a SUCCESS+=1
                
                REM Hiển thị kích thước
                for %%s in ("%%f") do set "SIZE_IN=%%~zs"
                for %%s in ("!OUTPUT_FILE!") do set "SIZE_OUT=%%~zs"
                
                set /a REDUCTION=100-^(!SIZE_OUT!*100/!SIZE_IN!^)
                echo     [OK] Output: !OUTPUT_FILE!
                echo          Size: !SIZE_IN! bytes -^> !SIZE_OUT! bytes ^(giam ~!REDUCTION!%%^)
            ) else (
                set /a FAIL+=1
                echo     [FAIL] Khong the convert file nay
            )
            echo.
        )
    )
)

if %COUNT%==0 (
    echo [INFO] Khong tim thay file audio nao trong thu muc input
    echo [INFO] Dinh dang ho tro: wav, mp3, flac, ogg, m4a, aac, wma
) else (
    echo ============================================================
    echo   Ket qua: %SUCCESS%/%COUNT% file thanh cong
    echo   Output: %OUTPUT_DIR%
    echo ============================================================
)

pause
