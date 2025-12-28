# LSM6DS3TR-C Driver for ESP-IDF

Driver cho cảm biến IMU 6 trục LSM6DS3TR-C (Accelerometer + Gyroscope) trên ESP-IDF.

## Tổng quan

LSM6DS3TR-C là một cảm biến system-in-package bao gồm:
- **Accelerometer 3 trục**: ±2g / ±4g / ±8g / ±16g
- **Gyroscope 3 trục**: ±125 / ±250 / ±500 / ±1000 / ±2000 dps
- **Cảm biến nhiệt độ** tích hợp
- **FIFO hardware** lên đến 8KB
- Các tính năng nhúng: pedometer, step counter, tap detection, tilt detection

## Kết nối phần cứng

### I2C Interface

| Pin LSM6DS3TR-C | ESP32 | Mô tả |
|-----------------|-------|-------|
| VDD             | 3.3V  | Nguồn (1.71V - 3.6V) |
| VDDIO           | 3.3V  | Nguồn I/O |
| GND             | GND   | Ground |
| SDA             | GPIOx | I2C Data |
| SCL             | GPIOx | I2C Clock |
| SA0             | GND/3.3V | Địa chỉ I2C (0=0x6A, 1=0x6B) |

### Địa chỉ I2C
- **SA0 = GND**: 0x6A
- **SA0 = VCC**: 0x6B

## Cách sử dụng

### 1. Khởi tạo I2C

```cpp
#include <driver/i2c.h>

// Cấu hình I2C
i2c_config_t i2c_config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master = {
        .clk_speed = 400000  // 400kHz
    }
};

i2c_param_config(I2C_NUM_0, &i2c_config);
i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
```

### 2. Khởi tạo cảm biến

```cpp
#include "gyro/lsm6ds3tr_c.h"

// Tạo instance với I2C port và địa chỉ
Lsm6ds3trC imu(I2C_NUM_0, LSM6DS3TR_C_I2C_ADDR_LOW);

// Khởi tạo
esp_err_t ret = imu.Initialize();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize IMU");
}
```

### 3. Đọc dữ liệu cảm biến

```cpp
// Đọc tất cả dữ liệu cùng lúc
lsm6ds3tr_c_sensor_data_t data;
if (imu.ReadAllData(&data) == ESP_OK) {
    ESP_LOGI(TAG, "Accel: X=%.3fg Y=%.3fg Z=%.3fg", 
             data.accel.x, data.accel.y, data.accel.z);
    ESP_LOGI(TAG, "Gyro: X=%.2fdps Y=%.2fdps Z=%.2fdps",
             data.gyro.x, data.gyro.y, data.gyro.z);
    ESP_LOGI(TAG, "Temperature: %.1f°C", data.temperature);
}

// Hoặc đọc riêng từng loại
lsm6ds3tr_c_data_t accel;
imu.ReadAccel(&accel);

lsm6ds3tr_c_data_t gyro;
imu.ReadGyro(&gyro);

float temp;
imu.ReadTemperature(&temp);
```

### 4. Cấu hình Output Data Rate và Full Scale

```cpp
// Accelerometer: 208Hz, ±4g
imu.ConfigureAccelerometer(LSM6DS3TR_C_XL_ODR_208Hz, LSM6DS3TR_C_XL_FS_4G);

// Gyroscope: 416Hz, ±500dps
imu.ConfigureGyroscope(LSM6DS3TR_C_GY_ODR_416Hz, LSM6DS3TR_C_GY_FS_500DPS);
```

### 5. Sử dụng Pedometer (đếm bước)

```cpp
// Bật pedometer
imu.EnablePedometer(true);

// Đọc số bước
uint16_t steps;
imu.ReadStepCounter(&steps);
ESP_LOGI(TAG, "Steps: %u", steps);

// Reset bộ đếm
imu.ResetStepCounter();
```

### 6. Phát hiện Tap (chạm)

```cpp
// Bật phát hiện single-tap
imu.EnableTapDetection(true, false);

// Bật phát hiện double-tap
imu.EnableTapDetection(true, true);

// Kiểm tra tap
bool is_double;
if (imu.IsTapDetected(&is_double)) {
    ESP_LOGI(TAG, "%s-tap detected!", is_double ? "Double" : "Single");
}
```

### 7. Phát hiện Tilt (nghiêng)

```cpp
// Bật phát hiện tilt
imu.EnableTiltDetection(true);

// Kiểm tra tilt
if (imu.IsTiltDetected()) {
    ESP_LOGI(TAG, "Device tilted!");
}
```

## Ví dụ hoàn chỉnh

```cpp
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "gyro/lsm6ds3tr_c.h"

static const char* TAG = "IMU_Example";

void imu_task(void* arg) {
    Lsm6ds3trC* imu = static_cast<Lsm6ds3trC*>(arg);
    
    while (true) {
        lsm6ds3tr_c_sensor_data_t data;
        if (imu->ReadAllData(&data) == ESP_OK) {
            ESP_LOGI(TAG, "A: [%+.3f, %+.3f, %+.3f] g | G: [%+.1f, %+.1f, %+.1f] dps | T: %.1f°C",
                     data.accel.x, data.accel.y, data.accel.z,
                     data.gyro.x, data.gyro.y, data.gyro.z,
                     data.temperature);
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // 10Hz output
    }
}

extern "C" void app_main() {
    // Initialize I2C
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed = 400000 }
    };
    i2c_param_config(I2C_NUM_0, &i2c_config);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    
    // Initialize IMU
    static Lsm6ds3trC imu(I2C_NUM_0, LSM6DS3TR_C_I2C_ADDR_LOW);
    if (imu.Initialize() == ESP_OK) {
        // Configure for higher performance
        imu.ConfigureAccelerometer(LSM6DS3TR_C_XL_ODR_208Hz, LSM6DS3TR_C_XL_FS_4G);
        imu.ConfigureGyroscope(LSM6DS3TR_C_GY_ODR_208Hz, LSM6DS3TR_C_GY_FS_500DPS);
        
        // Start reading task
        xTaskCreate(imu_task, "imu_task", 4096, &imu, 5, NULL);
    }
}
```

## Thông số kỹ thuật

### Output Data Rate (ODR)

| ODR | Accelerometer | Gyroscope |
|-----|---------------|-----------|
| 12.5 Hz | ✓ | ✓ |
| 26 Hz | ✓ | ✓ |
| 52 Hz | ✓ | ✓ |
| 104 Hz | ✓ | ✓ |
| 208 Hz | ✓ | ✓ |
| 416 Hz | ✓ | ✓ |
| 833 Hz | ✓ | ✓ |
| 1660 Hz | ✓ | ✓ |
| 3330 Hz | ✓ | - |
| 6660 Hz | ✓ | - |

### Độ nhạy (Sensitivity)

**Accelerometer:**
| Full Scale | Sensitivity |
|------------|-------------|
| ±2g | 0.061 mg/LSB |
| ±4g | 0.122 mg/LSB |
| ±8g | 0.244 mg/LSB |
| ±16g | 0.488 mg/LSB |

**Gyroscope:**
| Full Scale | Sensitivity |
|------------|-------------|
| ±125 dps | 4.375 mdps/LSB |
| ±250 dps | 8.75 mdps/LSB |
| ±500 dps | 17.50 mdps/LSB |
| ±1000 dps | 35.0 mdps/LSB |
| ±2000 dps | 70.0 mdps/LSB |

## Tài liệu tham khảo

- [LSM6DS3TR-C Datasheet](https://www.st.com/resource/en/datasheet/lsm6ds3tr-c.pdf)
- [Application Note AN5130](https://www.st.com/resource/en/application_note/an5130-lsm6ds3trc-alwayson-3d-accelerometer-and-3d-gyroscope-stmicroelectronics.pdf)
