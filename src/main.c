#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(as705x, LOG_LEVEL_DBG);

// Replace this with the actual I2C bus your device is connected to
#define I2C_DEV DT_LABEL(DT_NODELABEL(i2c21))
#define AS705X_MCU_ADDR 0x55  // I2C address of the MCU

// Command IDs
#define CMD_GET_APP_VERSION    0x01
#define CMD_GET_CHIP_VARIANT   0x02
#define CMD_GET_STATUS         0x03
#define CMD_START_MEASUREMENT  0x10
#define CMD_STOP_MEASUREMENT   0x11
#define CMD_READ_FIFO          0x12
#define CMD_SET_CONFIGURATION  0x20

// Status bit definitions (example - check your datasheet)
#define STATUS_MEASURING       (1 << 0)
#define STATUS_FIFO_READY      (1 << 1)
#define STATUS_ERROR           (1 << 7)

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

int send_command(uint8_t cmd_id, uint8_t *resp_buf, size_t resp_len) {
    int ret = i2c_write_read(i2c_dev, AS705X_MCU_ADDR, &cmd_id, 1, resp_buf, resp_len);
    if (ret < 0) {
        LOG_ERR("I2C command 0x%02X failed (%d)", cmd_id, ret);
    }
    return ret;
}

void decode_status(uint8_t status) {
    LOG_INF("Status: 0x%02X", status);
    LOG_INF("  Bit 7: %s", (status & 0x80) ? "SET" : "CLEAR");
    LOG_INF("  Bit 4: %s", (status & 0x10) ? "SET" : "CLEAR");
    LOG_INF("  Bit 2: %s", (status & 0x04) ? "SET" : "CLEAR");
    LOG_INF("  Bit 0: %s", (status & 0x01) ? "SET" : "CLEAR");
}

int set_configuration_proper(void) {
    // More conservative configuration
    uint8_t config_payload[14] = {
        0x01, // Enable only channel 0 (single LED)
        0x1F, // LED1 current (reduced from 0x3F)
        0x00, // LED2 current (disabled)
        0x00, // LED3 current (disabled)
        0x02, // Gain (reduced from 0x04)
        0x02, // Sampling rate (25Hz, reduced from 50Hz)
        0x01, // Averaging
        0x01, // FIFO threshold
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Reserved bytes
    };

    uint8_t config_cmd[15];
    config_cmd[0] = CMD_SET_CONFIGURATION;
    memcpy(&config_cmd[1], config_payload, 14);

    int ret = i2c_write(i2c_dev, config_cmd, sizeof(config_cmd), AS705X_MCU_ADDR);
    LOG_INF("SET_CONFIGURATION returned: %d", ret);
    
    if (ret == 0) {
        LOG_INF("Sensor configured successfully");
        
        // Verify configuration took effect by checking status
        k_msleep(100); // Allow time for configuration
        uint8_t status;
        if (send_command(CMD_GET_STATUS, &status, 1) == 0) {
            LOG_INF("Status after configuration: 0x%02X", status);
            decode_status(status);
        }
    } else {
        LOG_ERR("Configuration failed: %d", ret);
    }
    
    return ret;
}

void parse_measurement(const uint8_t *data, size_t len) {
    if (len < 1) {
        LOG_ERR("Invalid FIFO response - no data");
        return;
    }

    uint8_t sample_count = data[0];
    LOG_INF("Samples in FIFO: %d", sample_count);

    if (sample_count == 0) {
        LOG_WRN("FIFO is empty - no samples available");
        return;
    }

    size_t expected_len = 1 + sample_count * 3;
    if (len < expected_len) {
        LOG_ERR("Truncated FIFO: expected %zu bytes, got %zu", expected_len, len);
        return;
    }

    for (int i = 0; i < sample_count; i++) {
        uint32_t sample = ((uint32_t)data[1 + i * 3] << 16) |
                          ((uint32_t)data[2 + i * 3] << 8) |
                          ((uint32_t)data[3 + i * 3]);
        LOG_INF("Sample[%d]: %u (0x%06X)", i, sample, sample);
    }
}

int wait_for_samples(int timeout_ms) {
    int elapsed = 0;
    const int poll_interval = 100;
    
    while (elapsed < timeout_ms) {
        uint8_t status;
        if (send_command(CMD_GET_STATUS, &status, 1) == 0) {
            LOG_DBG("Polling status: 0x%02X (elapsed: %dms)", status, elapsed);
            
            // Check if FIFO has data (you may need to adjust this condition)
            if (status != 0x95) { // Status changed from initial value
                return 0; // Success
            }
        }
        
        k_msleep(poll_interval);
        elapsed += poll_interval;
    }
    
    LOG_WRN("Timeout waiting for samples after %dms", timeout_ms);
    return -ETIMEDOUT;
}

void main(void)
{
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return;
    }

    LOG_INF("Communicating with AS705x Application Manager...");

    // Get firmware version
    uint8_t version[4];
    if (send_command(CMD_GET_APP_VERSION, version, sizeof(version)) == 0) {
        LOG_INF("Firmware version: %d.%d.%d.%d", version[0], version[1], version[2], version[3]);
    }

    // Get chip variant
    uint8_t variant;
    if (send_command(CMD_GET_CHIP_VARIANT, &variant, 1) == 0) {
        LOG_INF("Chip variant: 0x%02X (%s)", variant, variant == 0x00 ? "AS7056" : "AS7057");
    }

    // Get initial status
    uint8_t status;
    if (send_command(CMD_GET_STATUS, &status, 1) == 0) {
        LOG_INF("Initial Application Status: 0x%02X", status);
        decode_status(status);
    }

    // Stop any ongoing measurements first
    LOG_INF("Stopping any ongoing measurements...");
    send_command(CMD_STOP_MEASUREMENT, NULL, 0);
    k_msleep(100);

    // Configure the sensor
    if (set_configuration_proper() != 0) {
        LOG_ERR("Failed to configure sensor, aborting");
        return;
    }

    // Start measurement
    LOG_INF("Starting measurement...");
    if (send_command(CMD_START_MEASUREMENT, NULL, 0) == 0) {
        LOG_INF("Measurement command sent successfully");
    } else {
        LOG_ERR("Failed to start measurement");
        return;
    }

    // Give some time for the measurement to start and status to update
    k_msleep(200);

    // Check status after starting
    if (send_command(CMD_GET_STATUS, &status, 1) == 0) {
        LOG_INF("Status after start command: 0x%02X", status);
        decode_status(status);
    }

    // Wait for samples to be available
    LOG_INF("Waiting for samples...");
    if (wait_for_samples(5000) != 0) {
        LOG_WRN("No samples detected, but continuing with FIFO reads");
    }

    // Read FIFO multiple times with longer intervals
    uint8_t fifo_data[64]; // Increased buffer size
    for (int i = 0; i < 5; i++) {
        k_msleep(1000); // Longer delay between reads
        
        LOG_INF("FIFO read cycle %d:", i + 1);
        
        // Check status before reading
        if (send_command(CMD_GET_STATUS, &status, 1) == 0) {
            LOG_INF("  Status before read: 0x%02X", status);
        }
        
        // Read FIFO
        if (send_command(CMD_READ_FIFO, fifo_data, sizeof(fifo_data)) == 0) {
            parse_measurement(fifo_data, sizeof(fifo_data));
        } else {
            LOG_ERR("  FIFO read failed");
        }
    }

    // Stop measurement
    LOG_INF("Stopping measurement...");
    if (send_command(CMD_STOP_MEASUREMENT, NULL, 0) == 0) {
        LOG_INF("Measurement stopped successfully");
    }

    // Final status check
    k_msleep(100);
    if (send_command(CMD_GET_STATUS, &status, 1) == 0) {
        LOG_INF("Final status: 0x%02X", status);
        decode_status(status);
    }

    // Test invalid command
    LOG_INF("Testing invalid command...");
    uint8_t bogus_cmd = 0xFF, resp;
    if (i2c_write_read(i2c_dev, AS705X_MCU_ADDR, &bogus_cmd, 1, &resp, 1) == 0) {
        LOG_INF("Invalid command test response: 0x%02X", resp);
    } else {
        LOG_INF("Invalid command rejected (as expected)");
    }

    LOG_INF("Test sequence complete.");
}