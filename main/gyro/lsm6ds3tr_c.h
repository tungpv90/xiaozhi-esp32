#pragma once

#include <cstdint>
#include <esp_err.h>
#include <driver/i2c.h>

/**
 * @brief LSM6DS3TR-C 6-axis IMU (Accelerometer + Gyroscope) Driver
 * 
 * The LSM6DS3TR-C is a system-in-package featuring a 3D digital accelerometer
 * and a 3D digital gyroscope with an I2C/SPI interface.
 * 
 * Features:
 * - ±2/±4/±8/±16 g accelerometer full scale
 * - ±125/±250/±500/±1000/±2000 dps gyroscope full scale
 * - Embedded temperature sensor
 * - Hardware FIFO up to 8 KB
 * - Pedometer, step counter, significant motion detection
 * - Tilt, tap, double-tap detection
 */

// I2C Address (depends on SA0 pin)
#define LSM6DS3TR_C_I2C_ADDR_LOW    0x6A  // SA0 = GND
#define LSM6DS3TR_C_I2C_ADDR_HIGH   0x6B  // SA0 = VCC

// Device identification
#define LSM6DS3TR_C_WHO_AM_I_VALUE  0x6A

// Register addresses
#define LSM6DS3TR_C_FUNC_CFG_ACCESS     0x01
#define LSM6DS3TR_C_SENSOR_SYNC_TIME    0x04
#define LSM6DS3TR_C_FIFO_CTRL1          0x06
#define LSM6DS3TR_C_FIFO_CTRL2          0x07
#define LSM6DS3TR_C_FIFO_CTRL3          0x08
#define LSM6DS3TR_C_FIFO_CTRL4          0x09
#define LSM6DS3TR_C_FIFO_CTRL5          0x0A
#define LSM6DS3TR_C_ORIENT_CFG_G        0x0B
#define LSM6DS3TR_C_INT1_CTRL           0x0D
#define LSM6DS3TR_C_INT2_CTRL           0x0E
#define LSM6DS3TR_C_WHO_AM_I            0x0F
#define LSM6DS3TR_C_CTRL1_XL            0x10  // Accelerometer control
#define LSM6DS3TR_C_CTRL2_G             0x11  // Gyroscope control
#define LSM6DS3TR_C_CTRL3_C             0x12  // Control register 3
#define LSM6DS3TR_C_CTRL4_C             0x13  // Control register 4
#define LSM6DS3TR_C_CTRL5_C             0x14  // Control register 5
#define LSM6DS3TR_C_CTRL6_C             0x15  // Control register 6
#define LSM6DS3TR_C_CTRL7_G             0x16  // Gyroscope control 7
#define LSM6DS3TR_C_CTRL8_XL            0x17  // Accelerometer control 8
#define LSM6DS3TR_C_CTRL9_XL            0x18  // Accelerometer control 9
#define LSM6DS3TR_C_CTRL10_C            0x19  // Control register 10
#define LSM6DS3TR_C_WAKE_UP_SRC         0x1B
#define LSM6DS3TR_C_TAP_SRC             0x1C
#define LSM6DS3TR_C_D6D_SRC             0x1D
#define LSM6DS3TR_C_STATUS_REG          0x1E
#define LSM6DS3TR_C_OUT_TEMP_L          0x20
#define LSM6DS3TR_C_OUT_TEMP_H          0x21
#define LSM6DS3TR_C_OUTX_L_G            0x22  // Gyro X low byte
#define LSM6DS3TR_C_OUTX_H_G            0x23  // Gyro X high byte
#define LSM6DS3TR_C_OUTY_L_G            0x24  // Gyro Y low byte
#define LSM6DS3TR_C_OUTY_H_G            0x25  // Gyro Y high byte
#define LSM6DS3TR_C_OUTZ_L_G            0x26  // Gyro Z low byte
#define LSM6DS3TR_C_OUTZ_H_G            0x27  // Gyro Z high byte
#define LSM6DS3TR_C_OUTX_L_XL           0x28  // Accel X low byte
#define LSM6DS3TR_C_OUTX_H_XL           0x29  // Accel X high byte
#define LSM6DS3TR_C_OUTY_L_XL           0x2A  // Accel Y low byte
#define LSM6DS3TR_C_OUTY_H_XL           0x2B  // Accel Y high byte
#define LSM6DS3TR_C_OUTZ_L_XL           0x2C  // Accel Z low byte
#define LSM6DS3TR_C_OUTZ_H_XL           0x2D  // Accel Z high byte
#define LSM6DS3TR_C_FIFO_STATUS1        0x3A
#define LSM6DS3TR_C_FIFO_STATUS2        0x3B
#define LSM6DS3TR_C_FIFO_STATUS3        0x3C
#define LSM6DS3TR_C_FIFO_STATUS4        0x3D
#define LSM6DS3TR_C_FIFO_DATA_OUT_L     0x3E
#define LSM6DS3TR_C_FIFO_DATA_OUT_H     0x3F
#define LSM6DS3TR_C_TIMESTAMP0_REG      0x40
#define LSM6DS3TR_C_TIMESTAMP1_REG      0x41
#define LSM6DS3TR_C_TIMESTAMP2_REG      0x42
#define LSM6DS3TR_C_STEP_COUNTER_L      0x4B
#define LSM6DS3TR_C_STEP_COUNTER_H      0x4C
#define LSM6DS3TR_C_FUNC_SRC            0x53
#define LSM6DS3TR_C_TAP_CFG             0x58
#define LSM6DS3TR_C_TAP_THS_6D          0x59
#define LSM6DS3TR_C_INT_DUR2            0x5A
#define LSM6DS3TR_C_WAKE_UP_THS         0x5B
#define LSM6DS3TR_C_WAKE_UP_DUR         0x5C
#define LSM6DS3TR_C_FREE_FALL           0x5D
#define LSM6DS3TR_C_MD1_CFG             0x5E
#define LSM6DS3TR_C_MD2_CFG             0x5F

// Accelerometer Output Data Rate (ODR)
typedef enum {
    LSM6DS3TR_C_XL_ODR_OFF      = 0x00,
    LSM6DS3TR_C_XL_ODR_12_5Hz   = 0x10,
    LSM6DS3TR_C_XL_ODR_26Hz     = 0x20,
    LSM6DS3TR_C_XL_ODR_52Hz     = 0x30,
    LSM6DS3TR_C_XL_ODR_104Hz    = 0x40,
    LSM6DS3TR_C_XL_ODR_208Hz    = 0x50,
    LSM6DS3TR_C_XL_ODR_416Hz    = 0x60,
    LSM6DS3TR_C_XL_ODR_833Hz    = 0x70,
    LSM6DS3TR_C_XL_ODR_1660Hz   = 0x80,
    LSM6DS3TR_C_XL_ODR_3330Hz   = 0x90,
    LSM6DS3TR_C_XL_ODR_6660Hz   = 0xA0,
} lsm6ds3tr_c_xl_odr_t;

// Accelerometer Full Scale
typedef enum {
    LSM6DS3TR_C_XL_FS_2G    = 0x00,
    LSM6DS3TR_C_XL_FS_16G   = 0x04,
    LSM6DS3TR_C_XL_FS_4G    = 0x08,
    LSM6DS3TR_C_XL_FS_8G    = 0x0C,
} lsm6ds3tr_c_xl_fs_t;

// Gyroscope Output Data Rate (ODR)
typedef enum {
    LSM6DS3TR_C_GY_ODR_OFF      = 0x00,
    LSM6DS3TR_C_GY_ODR_12_5Hz   = 0x10,
    LSM6DS3TR_C_GY_ODR_26Hz     = 0x20,
    LSM6DS3TR_C_GY_ODR_52Hz     = 0x30,
    LSM6DS3TR_C_GY_ODR_104Hz    = 0x40,
    LSM6DS3TR_C_GY_ODR_208Hz    = 0x50,
    LSM6DS3TR_C_GY_ODR_416Hz    = 0x60,
    LSM6DS3TR_C_GY_ODR_833Hz    = 0x70,
    LSM6DS3TR_C_GY_ODR_1660Hz   = 0x80,
} lsm6ds3tr_c_gy_odr_t;

// Gyroscope Full Scale
typedef enum {
    LSM6DS3TR_C_GY_FS_250DPS    = 0x00,
    LSM6DS3TR_C_GY_FS_500DPS    = 0x04,
    LSM6DS3TR_C_GY_FS_1000DPS   = 0x08,
    LSM6DS3TR_C_GY_FS_2000DPS   = 0x0C,
    LSM6DS3TR_C_GY_FS_125DPS    = 0x02,
} lsm6ds3tr_c_gy_fs_t;

// Raw sensor data structure
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} lsm6ds3tr_c_raw_data_t;

// Sensor data in physical units
typedef struct {
    float x;  // g for accel, dps for gyro
    float y;
    float z;
} lsm6ds3tr_c_data_t;

// Full sensor reading
typedef struct {
    lsm6ds3tr_c_data_t accel;  // Accelerometer data in g
    lsm6ds3tr_c_data_t gyro;   // Gyroscope data in dps
    float temperature;          // Temperature in °C
} lsm6ds3tr_c_sensor_data_t;

/**
 * @brief LSM6DS3TR-C Driver Class
 */
class Lsm6ds3trC {
public:
    /**
     * @brief Constructor
     * @param i2c_port I2C port number
     * @param i2c_addr I2C address (0x6A or 0x6B)
     */
    Lsm6ds3trC(i2c_port_t i2c_port = I2C_NUM_0, uint8_t i2c_addr = LSM6DS3TR_C_I2C_ADDR_LOW);
    
    /**
     * @brief Destructor
     */
    ~Lsm6ds3trC();

    /**
     * @brief Initialize the sensor
     * @return ESP_OK on success
     */
    esp_err_t Initialize();

    /**
     * @brief Check if device is connected and responding
     * @return true if device responds with correct WHO_AM_I value
     */
    bool IsConnected();

    /**
     * @brief Software reset the device
     * @return ESP_OK on success
     */
    esp_err_t SoftwareReset();

    /**
     * @brief Configure accelerometer
     * @param odr Output data rate
     * @param fs Full scale range
     * @return ESP_OK on success
     */
    esp_err_t ConfigureAccelerometer(lsm6ds3tr_c_xl_odr_t odr, lsm6ds3tr_c_xl_fs_t fs);

    /**
     * @brief Configure gyroscope
     * @param odr Output data rate
     * @param fs Full scale range
     * @return ESP_OK on success
     */
    esp_err_t ConfigureGyroscope(lsm6ds3tr_c_gy_odr_t odr, lsm6ds3tr_c_gy_fs_t fs);

    /**
     * @brief Read raw accelerometer data
     * @param data Pointer to store raw data
     * @return ESP_OK on success
     */
    esp_err_t ReadAccelRaw(lsm6ds3tr_c_raw_data_t* data);

    /**
     * @brief Read raw gyroscope data
     * @param data Pointer to store raw data
     * @return ESP_OK on success
     */
    esp_err_t ReadGyroRaw(lsm6ds3tr_c_raw_data_t* data);

    /**
     * @brief Read accelerometer data in g
     * @param data Pointer to store data
     * @return ESP_OK on success
     */
    esp_err_t ReadAccel(lsm6ds3tr_c_data_t* data);

    /**
     * @brief Read gyroscope data in dps (degrees per second)
     * @param data Pointer to store data
     * @return ESP_OK on success
     */
    esp_err_t ReadGyro(lsm6ds3tr_c_data_t* data);

    /**
     * @brief Read temperature
     * @param temperature Pointer to store temperature in °C
     * @return ESP_OK on success
     */
    esp_err_t ReadTemperature(float* temperature);

    /**
     * @brief Read all sensor data (accel, gyro, temperature)
     * @param data Pointer to store sensor data
     * @return ESP_OK on success
     */
    esp_err_t ReadAllData(lsm6ds3tr_c_sensor_data_t* data);

    /**
     * @brief Check if new accelerometer data is available
     * @return true if new data is available
     */
    bool IsAccelDataReady();

    /**
     * @brief Check if new gyroscope data is available
     * @return true if new data is available
     */
    bool IsGyroDataReady();

    /**
     * @brief Enable/disable pedometer
     * @param enable true to enable
     * @return ESP_OK on success
     */
    esp_err_t EnablePedometer(bool enable);

    /**
     * @brief Read step counter value
     * @param steps Pointer to store step count
     * @return ESP_OK on success
     */
    esp_err_t ReadStepCounter(uint16_t* steps);

    /**
     * @brief Reset step counter
     * @return ESP_OK on success
     */
    esp_err_t ResetStepCounter();

    /**
     * @brief Enable/disable tap detection
     * @param enable true to enable
     * @param double_tap true for double-tap, false for single-tap
     * @return ESP_OK on success
     */
    esp_err_t EnableTapDetection(bool enable, bool double_tap = false);

    /**
     * @brief Check if tap was detected
     * @param double_tap Pointer to store if it was a double-tap (optional)
     * @return true if tap was detected
     */
    bool IsTapDetected(bool* double_tap = nullptr);

    /**
     * @brief Enable/disable tilt detection
     * @param enable true to enable
     * @return ESP_OK on success
     */
    esp_err_t EnableTiltDetection(bool enable);

    /**
     * @brief Check if tilt was detected
     * @return true if tilt was detected
     */
    bool IsTiltDetected();

private:
    i2c_port_t i2c_port_;
    uint8_t i2c_addr_;
    float accel_sensitivity_;  // Sensitivity in g/LSB
    float gyro_sensitivity_;   // Sensitivity in dps/LSB

    /**
     * @brief Write a byte to a register
     */
    esp_err_t WriteRegister(uint8_t reg, uint8_t value);

    /**
     * @brief Read a byte from a register
     */
    esp_err_t ReadRegister(uint8_t reg, uint8_t* value);

    /**
     * @brief Read multiple bytes from registers
     */
    esp_err_t ReadRegisters(uint8_t start_reg, uint8_t* buffer, size_t length);

    /**
     * @brief Update sensitivity values based on current configuration
     */
    void UpdateSensitivity();
};
