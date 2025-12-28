#include "lsm6ds3tr_c.h"
#include <esp_log.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "LSM6DS3TR-C";

// Timeout for I2C operations
#define I2C_TIMEOUT_MS 100

Lsm6ds3trC::Lsm6ds3trC(i2c_port_t i2c_port, uint8_t i2c_addr)
    : i2c_port_(i2c_port)
    , i2c_addr_(i2c_addr)
    , accel_sensitivity_(0.061f)  // Default for ±2g
    , gyro_sensitivity_(8.75f)    // Default for ±250dps
{
}

Lsm6ds3trC::~Lsm6ds3trC() {
}

esp_err_t Lsm6ds3trC::WriteRegister(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    return i2c_master_write_to_device(i2c_port_, i2c_addr_, buffer, 2, 
                                       pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t Lsm6ds3trC::ReadRegister(uint8_t reg, uint8_t* value) {
    return i2c_master_write_read_device(i2c_port_, i2c_addr_, &reg, 1, value, 1,
                                         pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t Lsm6ds3trC::ReadRegisters(uint8_t start_reg, uint8_t* buffer, size_t length) {
    return i2c_master_write_read_device(i2c_port_, i2c_addr_, &start_reg, 1, 
                                         buffer, length, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

bool Lsm6ds3trC::IsConnected() {
    uint8_t who_am_i = 0;
    esp_err_t ret = ReadRegister(LSM6DS3TR_C_WHO_AM_I, &who_am_i);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I register");
        return false;
    }
    
    if (who_am_i != LSM6DS3TR_C_WHO_AM_I_VALUE) {
        ESP_LOGE(TAG, "WHO_AM_I mismatch: expected 0x%02X, got 0x%02X", 
                 LSM6DS3TR_C_WHO_AM_I_VALUE, who_am_i);
        return false;
    }
    
    ESP_LOGI(TAG, "Device found, WHO_AM_I = 0x%02X", who_am_i);
    return true;
}

esp_err_t Lsm6ds3trC::SoftwareReset() {
    // Set SW_RESET bit in CTRL3_C register
    esp_err_t ret = WriteRegister(LSM6DS3TR_C_CTRL3_C, 0x01);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Wait for reset to complete (typically 50us, we wait 10ms to be safe)
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Wait until SW_RESET bit is cleared
    uint8_t ctrl3;
    int timeout = 100;
    do {
        ret = ReadRegister(LSM6DS3TR_C_CTRL3_C, &ctrl3);
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while ((ctrl3 & 0x01) && (--timeout > 0));
    
    if (timeout == 0) {
        ESP_LOGE(TAG, "Software reset timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGI(TAG, "Software reset complete");
    return ESP_OK;
}

esp_err_t Lsm6ds3trC::Initialize() {
    // Check device connection
    if (!IsConnected()) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Software reset
    esp_err_t ret = SoftwareReset();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Enable Block Data Update (BDU) - prevents reading during update
    // Set IF_INC for auto-increment during multi-byte reads
    ret = WriteRegister(LSM6DS3TR_C_CTRL3_C, 0x44);  // BDU = 1, IF_INC = 1
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Configure accelerometer: 104Hz, ±2g
    ret = ConfigureAccelerometer(LSM6DS3TR_C_XL_ODR_104Hz, LSM6DS3TR_C_XL_FS_2G);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Configure gyroscope: 104Hz, ±250dps
    ret = ConfigureGyroscope(LSM6DS3TR_C_GY_ODR_104Hz, LSM6DS3TR_C_GY_FS_250DPS);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ESP_LOGI(TAG, "Initialization complete");
    return ESP_OK;
}

void Lsm6ds3trC::UpdateSensitivity() {
    // Read current configuration
    uint8_t ctrl1_xl, ctrl2_g;
    ReadRegister(LSM6DS3TR_C_CTRL1_XL, &ctrl1_xl);
    ReadRegister(LSM6DS3TR_C_CTRL2_G, &ctrl2_g);
    
    // Accelerometer sensitivity (mg/LSB)
    switch (ctrl1_xl & 0x0C) {
        case LSM6DS3TR_C_XL_FS_2G:
            accel_sensitivity_ = 0.061f;
            break;
        case LSM6DS3TR_C_XL_FS_4G:
            accel_sensitivity_ = 0.122f;
            break;
        case LSM6DS3TR_C_XL_FS_8G:
            accel_sensitivity_ = 0.244f;
            break;
        case LSM6DS3TR_C_XL_FS_16G:
            accel_sensitivity_ = 0.488f;
            break;
    }
    
    // Gyroscope sensitivity (mdps/LSB)
    switch (ctrl2_g & 0x0E) {
        case LSM6DS3TR_C_GY_FS_125DPS:
            gyro_sensitivity_ = 4.375f;
            break;
        case LSM6DS3TR_C_GY_FS_250DPS:
            gyro_sensitivity_ = 8.75f;
            break;
        case LSM6DS3TR_C_GY_FS_500DPS:
            gyro_sensitivity_ = 17.50f;
            break;
        case LSM6DS3TR_C_GY_FS_1000DPS:
            gyro_sensitivity_ = 35.0f;
            break;
        case LSM6DS3TR_C_GY_FS_2000DPS:
            gyro_sensitivity_ = 70.0f;
            break;
    }
}

esp_err_t Lsm6ds3trC::ConfigureAccelerometer(lsm6ds3tr_c_xl_odr_t odr, lsm6ds3tr_c_xl_fs_t fs) {
    uint8_t ctrl1_xl = (uint8_t)odr | (uint8_t)fs;
    esp_err_t ret = WriteRegister(LSM6DS3TR_C_CTRL1_XL, ctrl1_xl);
    if (ret == ESP_OK) {
        UpdateSensitivity();
        ESP_LOGI(TAG, "Accelerometer configured: ODR=0x%02X, FS=0x%02X", odr, fs);
    }
    return ret;
}

esp_err_t Lsm6ds3trC::ConfigureGyroscope(lsm6ds3tr_c_gy_odr_t odr, lsm6ds3tr_c_gy_fs_t fs) {
    uint8_t ctrl2_g = (uint8_t)odr | (uint8_t)fs;
    esp_err_t ret = WriteRegister(LSM6DS3TR_C_CTRL2_G, ctrl2_g);
    if (ret == ESP_OK) {
        UpdateSensitivity();
        ESP_LOGI(TAG, "Gyroscope configured: ODR=0x%02X, FS=0x%02X", odr, fs);
    }
    return ret;
}

esp_err_t Lsm6ds3trC::ReadAccelRaw(lsm6ds3tr_c_raw_data_t* data) {
    uint8_t buffer[6];
    esp_err_t ret = ReadRegisters(LSM6DS3TR_C_OUTX_L_XL, buffer, 6);
    if (ret != ESP_OK) {
        return ret;
    }
    
    data->x = (int16_t)((buffer[1] << 8) | buffer[0]);
    data->y = (int16_t)((buffer[3] << 8) | buffer[2]);
    data->z = (int16_t)((buffer[5] << 8) | buffer[4]);
    
    return ESP_OK;
}

esp_err_t Lsm6ds3trC::ReadGyroRaw(lsm6ds3tr_c_raw_data_t* data) {
    uint8_t buffer[6];
    esp_err_t ret = ReadRegisters(LSM6DS3TR_C_OUTX_L_G, buffer, 6);
    if (ret != ESP_OK) {
        return ret;
    }
    
    data->x = (int16_t)((buffer[1] << 8) | buffer[0]);
    data->y = (int16_t)((buffer[3] << 8) | buffer[2]);
    data->z = (int16_t)((buffer[5] << 8) | buffer[4]);
    
    return ESP_OK;
}

esp_err_t Lsm6ds3trC::ReadAccel(lsm6ds3tr_c_data_t* data) {
    lsm6ds3tr_c_raw_data_t raw;
    esp_err_t ret = ReadAccelRaw(&raw);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Convert to g (sensitivity is in mg/LSB, so divide by 1000)
    data->x = (float)raw.x * accel_sensitivity_ / 1000.0f;
    data->y = (float)raw.y * accel_sensitivity_ / 1000.0f;
    data->z = (float)raw.z * accel_sensitivity_ / 1000.0f;
    
    return ESP_OK;
}

esp_err_t Lsm6ds3trC::ReadGyro(lsm6ds3tr_c_data_t* data) {
    lsm6ds3tr_c_raw_data_t raw;
    esp_err_t ret = ReadGyroRaw(&raw);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Convert to dps (sensitivity is in mdps/LSB, so divide by 1000)
    data->x = (float)raw.x * gyro_sensitivity_ / 1000.0f;
    data->y = (float)raw.y * gyro_sensitivity_ / 1000.0f;
    data->z = (float)raw.z * gyro_sensitivity_ / 1000.0f;
    
    return ESP_OK;
}

esp_err_t Lsm6ds3trC::ReadTemperature(float* temperature) {
    uint8_t buffer[2];
    esp_err_t ret = ReadRegisters(LSM6DS3TR_C_OUT_TEMP_L, buffer, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    
    int16_t raw_temp = (int16_t)((buffer[1] << 8) | buffer[0]);
    // Temperature sensitivity: 256 LSB/°C, with 0 at 25°C
    *temperature = 25.0f + ((float)raw_temp / 256.0f);
    
    return ESP_OK;
}

esp_err_t Lsm6ds3trC::ReadAllData(lsm6ds3tr_c_sensor_data_t* data) {
    // Read all output registers in one burst (temperature + gyro + accel)
    uint8_t buffer[14];
    esp_err_t ret = ReadRegisters(LSM6DS3TR_C_OUT_TEMP_L, buffer, 14);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Temperature
    int16_t raw_temp = (int16_t)((buffer[1] << 8) | buffer[0]);
    data->temperature = 25.0f + ((float)raw_temp / 256.0f);
    
    // Gyroscope
    int16_t gx = (int16_t)((buffer[3] << 8) | buffer[2]);
    int16_t gy = (int16_t)((buffer[5] << 8) | buffer[4]);
    int16_t gz = (int16_t)((buffer[7] << 8) | buffer[6]);
    data->gyro.x = (float)gx * gyro_sensitivity_ / 1000.0f;
    data->gyro.y = (float)gy * gyro_sensitivity_ / 1000.0f;
    data->gyro.z = (float)gz * gyro_sensitivity_ / 1000.0f;
    
    // Accelerometer
    int16_t ax = (int16_t)((buffer[9] << 8) | buffer[8]);
    int16_t ay = (int16_t)((buffer[11] << 8) | buffer[10]);
    int16_t az = (int16_t)((buffer[13] << 8) | buffer[12]);
    data->accel.x = (float)ax * accel_sensitivity_ / 1000.0f;
    data->accel.y = (float)ay * accel_sensitivity_ / 1000.0f;
    data->accel.z = (float)az * accel_sensitivity_ / 1000.0f;
    
    return ESP_OK;
}

bool Lsm6ds3trC::IsAccelDataReady() {
    uint8_t status;
    if (ReadRegister(LSM6DS3TR_C_STATUS_REG, &status) != ESP_OK) {
        return false;
    }
    return (status & 0x01) != 0;  // XLDA bit
}

bool Lsm6ds3trC::IsGyroDataReady() {
    uint8_t status;
    if (ReadRegister(LSM6DS3TR_C_STATUS_REG, &status) != ESP_OK) {
        return false;
    }
    return (status & 0x02) != 0;  // GDA bit
}

esp_err_t Lsm6ds3trC::EnablePedometer(bool enable) {
    // Enable embedded functions access
    esp_err_t ret = WriteRegister(LSM6DS3TR_C_FUNC_CFG_ACCESS, 0x80);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Enable step counter and pedometer algorithm
    uint8_t ctrl10_c;
    ret = ReadRegister(LSM6DS3TR_C_CTRL10_C, &ctrl10_c);
    if (ret != ESP_OK) {
        WriteRegister(LSM6DS3TR_C_FUNC_CFG_ACCESS, 0x00);
        return ret;
    }
    
    if (enable) {
        ctrl10_c |= 0x0C;  // PEDO_EN + FUNC_EN
    } else {
        ctrl10_c &= ~0x0C;
    }
    
    ret = WriteRegister(LSM6DS3TR_C_CTRL10_C, ctrl10_c);
    
    // Disable embedded functions access
    WriteRegister(LSM6DS3TR_C_FUNC_CFG_ACCESS, 0x00);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pedometer %s", enable ? "enabled" : "disabled");
    }
    return ret;
}

esp_err_t Lsm6ds3trC::ReadStepCounter(uint16_t* steps) {
    uint8_t buffer[2];
    esp_err_t ret = ReadRegisters(LSM6DS3TR_C_STEP_COUNTER_L, buffer, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    
    *steps = (uint16_t)((buffer[1] << 8) | buffer[0]);
    return ESP_OK;
}

esp_err_t Lsm6ds3trC::ResetStepCounter() {
    // Access embedded functions
    esp_err_t ret = WriteRegister(LSM6DS3TR_C_FUNC_CFG_ACCESS, 0x80);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Set PEDO_RST_STEP bit
    uint8_t ctrl10_c;
    ret = ReadRegister(LSM6DS3TR_C_CTRL10_C, &ctrl10_c);
    if (ret != ESP_OK) {
        WriteRegister(LSM6DS3TR_C_FUNC_CFG_ACCESS, 0x00);
        return ret;
    }
    
    ret = WriteRegister(LSM6DS3TR_C_CTRL10_C, ctrl10_c | 0x02);
    
    // Disable embedded functions access
    WriteRegister(LSM6DS3TR_C_FUNC_CFG_ACCESS, 0x00);
    
    return ret;
}

esp_err_t Lsm6ds3trC::EnableTapDetection(bool enable, bool double_tap) {
    if (enable) {
        // Enable tap detection on all axes
        esp_err_t ret = WriteRegister(LSM6DS3TR_C_TAP_CFG, 0x8E);  // TAP_X/Y/Z_EN, LIR
        if (ret != ESP_OK) {
            return ret;
        }
        
        // Set tap threshold and quiet/shock time
        ret = WriteRegister(LSM6DS3TR_C_TAP_THS_6D, 0x8C);  // D4D_EN, 6D threshold
        if (ret != ESP_OK) {
            return ret;
        }
        
        // Set duration
        ret = WriteRegister(LSM6DS3TR_C_INT_DUR2, double_tap ? 0x7F : 0x06);
        if (ret != ESP_OK) {
            return ret;
        }
        
        // Route to INT1 if needed
        ESP_LOGI(TAG, "%s-tap detection enabled", double_tap ? "Double" : "Single");
    } else {
        esp_err_t ret = WriteRegister(LSM6DS3TR_C_TAP_CFG, 0x00);
        if (ret != ESP_OK) {
            return ret;
        }
        ESP_LOGI(TAG, "Tap detection disabled");
    }
    
    return ESP_OK;
}

bool Lsm6ds3trC::IsTapDetected(bool* double_tap) {
    uint8_t tap_src;
    if (ReadRegister(LSM6DS3TR_C_TAP_SRC, &tap_src) != ESP_OK) {
        return false;
    }
    
    bool tap_detected = (tap_src & 0x40) != 0;  // TAP_IA
    
    if (double_tap != nullptr) {
        *double_tap = (tap_src & 0x10) != 0;  // DOUBLE_TAP
    }
    
    return tap_detected;
}

esp_err_t Lsm6ds3trC::EnableTiltDetection(bool enable) {
    // Enable/disable tilt calculation
    uint8_t ctrl10_c;
    esp_err_t ret = ReadRegister(LSM6DS3TR_C_CTRL10_C, &ctrl10_c);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (enable) {
        ctrl10_c |= 0x20;  // TILT_EN
    } else {
        ctrl10_c &= ~0x20;
    }
    
    ret = WriteRegister(LSM6DS3TR_C_CTRL10_C, ctrl10_c);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Tilt detection %s", enable ? "enabled" : "disabled");
    }
    return ret;
}

bool Lsm6ds3trC::IsTiltDetected() {
    uint8_t func_src;
    if (ReadRegister(LSM6DS3TR_C_FUNC_SRC, &func_src) != ESP_OK) {
        return false;
    }
    
    return (func_src & 0x20) != 0;  // TILT_IA
}
