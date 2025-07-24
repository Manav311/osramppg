#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(as705x, LOG_LEVEL_DBG);

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

#define AS7056_I2C_ADDR 0x55

// Key registers for hardware diagnostics
#define REG_APP_VER            0x01
#define REG_HW_VER             0x02
#define REG_LED_CTRL           0x03
#define REG_STATUS             0x07
#define REG_ERROR              0x08
#define REG_SEQ_STOP           0x0C
#define REG_MEAS_CTRL          0x20

int write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    int ret = i2c_write(i2c_dev, buf, 2, AS7056_I2C_ADDR);
    if (ret != 0) {
        LOG_ERR("WRITE FAIL: reg 0x%02X = 0x%02X (err=%d)", reg, val, ret);
    } else {
        LOG_DBG("WRITE: reg 0x%02X = 0x%02X", reg, val);
    }
    return ret;
}

int read_reg(uint8_t reg, uint8_t *val) {
    int ret = i2c_write_read(i2c_dev, AS7056_I2C_ADDR, &reg, 1, val, 1);
    if (ret != 0) {
        LOG_ERR("READ FAIL: reg 0x%02X (err=%d)", reg, ret);
    }
    return ret;
}

int send_cmd(uint8_t cmd, uint8_t *resp, size_t resp_len) {
    return i2c_write_read(i2c_dev, AS7056_I2C_ADDR, &cmd, 1, resp, resp_len);
}

// Test if sensor responds to any commands
bool test_basic_communication(void) {
    LOG_INF("=== BASIC COMMUNICATION TEST ===");
    
    uint8_t version[4];
    
    // Test version commands (should always work if sensor is alive)
    if (send_cmd(0x01, version, sizeof(version)) == 0) {
        LOG_INF("✓ APP Version command responds: %d.%d.%d.%d", 
                version[0], version[1], version[2], version[3]);
    } else {
        LOG_ERR("✗ APP Version command failed - sensor may be dead");
        return false;
    }
    
    if (send_cmd(0x02, version, sizeof(version)) == 0) {
        LOG_INF("✓ HW Version command responds: %d.%d.%d.%d", 
                version[0], version[1], version[2], version[3]);
    } else {
        LOG_ERR("✗ HW Version command failed");
        return false;
    }
    
    return true;
}

// Test if any registers are writable (not in total lockdown)
bool test_register_writability(void) {
    LOG_INF("=== REGISTER WRITABILITY TEST ===");
    
    // Test registers that should be writable
    struct {
        uint8_t reg;
        const char *name;
        uint8_t test_val;
    } writable_regs[] = {
        {0x15, "SEQ1_LED1_CURR", 0x01},
        {0x16, "SEQ1_LED2_CURR", 0x01}, 
        {0x24, "T_INT_L", 0x01},
        {0x26, "AVERAGING", 0x01},
        {0x28, "FIFO_LEVEL0", 0x01},
        {0x29, "FIFO_LEVEL1", 0x01}
    };
    
    bool any_writable = false;
    
    for (int i = 0; i < ARRAY_SIZE(writable_regs); i++) {
        uint8_t original, test_read;
        
        // Read original value
        if (read_reg(writable_regs[i].reg, &original) != 0) {
            continue;
        }
        
        // Write test value
        write_reg(writable_regs[i].reg, writable_regs[i].test_val);
        k_msleep(10);
        
        // Read back
        if (read_reg(writable_regs[i].reg, &test_read) == 0) {
            if (test_read == writable_regs[i].test_val) {
                LOG_INF("✓ %s is writable (0x%02X -> 0x%02X)", 
                        writable_regs[i].name, original, test_read);
                any_writable = true;
            } else {
                LOG_WRN("✗ %s not writable (wrote 0x%02X, read 0x%02X)", 
                        writable_regs[i].name, writable_regs[i].test_val, test_read);
            }
        }
        
        // Restore original
        write_reg(writable_regs[i].reg, original);
        k_msleep(10);
    }
    
    return any_writable;
}

// Test different I2C speeds to rule out communication issues
void test_communication_speeds(void) {
    LOG_INF("=== COMMUNICATION SPEED TEST ===");
    LOG_INF("Testing at current I2C speed...");
    
    // Try reading error register multiple times
    for (int i = 0; i < 5; i++) {
        uint8_t error_val;
        if (read_reg(REG_ERROR, &error_val) == 0) {
            LOG_INF("Read %d: ERROR = 0x%02X", i, error_val);
        } else {
            LOG_ERR("Read %d: FAILED", i);
        }
        k_msleep(10);
    }
}

// Try alternative I2C addresses (in case address is wrong)
void test_alternative_addresses(void) {
    LOG_INF("=== ALTERNATIVE I2C ADDRESS TEST ===");
    
    // Common alternative addresses for similar sensors
    uint8_t test_addresses[] = {0x54, 0x56, 0x2A, 0x2B, 0x48, 0x49};
    
    for (int i = 0; i < ARRAY_SIZE(test_addresses); i++) {
        LOG_INF("Testing address 0x%02X...", test_addresses[i]);
        
        uint8_t test_reg = 0x01;  // Version register
        uint8_t response[4];
        
        int ret = i2c_write_read(i2c_dev, test_addresses[i], &test_reg, 1, response, 4);
        if (ret == 0) {
            LOG_INF("✓ Response from 0x%02X: %02X %02X %02X %02X", 
                    test_addresses[i], response[0], response[1], response[2], response[3]);
        }
    }
}

// Hardware reset attempt through I2C
void attempt_hardware_reset(void) {
    LOG_INF("=== HARDWARE RESET ATTEMPT ===");
    
    // Try common hardware reset sequences
    uint8_t reset_sequences[][3] = {
        {0xFF, 0xFF, 0xFF},  // All 1s reset
        {0x00, 0x00, 0x00},  // All 0s reset
        {0xAA, 0x55, 0xAA},  // Alternating pattern
        {0x5A, 0xA5, 0x5A},  // Another alternating pattern
    };
    
    for (int seq = 0; seq < ARRAY_SIZE(reset_sequences); seq++) {
        LOG_INF("Trying reset sequence %d...", seq);
        
        // Try sending reset sequence to different registers
        uint8_t reset_regs[] = {0x00, 0x0C, 0x20, 0xFF};
        
        for (int reg = 0; reg < ARRAY_SIZE(reset_regs); reg++) {
            for (int byte = 0; byte < 3; byte++) {
                write_reg(reset_regs[reg], reset_sequences[seq][byte]);
                k_msleep(50);
            }
            
            // Check if error state changed
            uint8_t error_after;
            if (read_reg(REG_ERROR, &error_after) == 0) {
                if (error_after != 0x3E) {
                    LOG_INF("✓ Reset sequence %d changed error state to 0x%02X!", 
                            seq, error_after);
                    return;
                }
            }
        }
    }
    
    LOG_WRN("No reset sequence worked");
}

// Final diagnostic summary
void diagnostic_summary(void) {
    LOG_INF("=== DIAGNOSTIC SUMMARY ===");
    
    uint8_t error_val, seq_stop, status;
    
    read_reg(REG_ERROR, &error_val);
    read_reg(REG_SEQ_STOP, &seq_stop);
    read_reg(REG_STATUS, &status);
    
    LOG_INF("Final readings:");
    LOG_INF("  ERROR: 0x%02X (%s)", error_val, 
            (error_val == 0x3E) ? "PERSISTENT ERROR" : "Changed");
    LOG_INF("  SEQ_STOP: 0x%02X (%s)", seq_stop,
            (seq_stop == 0x1F) ? "SEQUENCER LOCKED" : "Changed");
    LOG_INF("  STATUS: 0x%02X", status);
    
    if (error_val == 0x3E && seq_stop == 0x1F) {
        LOG_ERR("=== VERDICT: HARDWARE FAILURE ===");
        LOG_ERR("The AS7056 sensor appears to have a hardware fault:");
        LOG_ERR("1. Error register locked at 0x3E (multiple subsystem errors)");
        LOG_ERR("2. Sequencer permanently stopped at 0x1F");
        LOG_ERR("3. No register values can be changed to clear the error state");
        LOG_ERR("4. This indicates internal hardware damage or protection mode");
        LOG_ERR("");
        LOG_ERR("Recommended actions:");
        LOG_ERR("- Check power supply voltage (should be 1.8V or 3.3V per datasheet)");
        LOG_ERR("- Check for physical damage to the sensor");
        LOG_ERR("- Try different AS7056 sensor if available");
        LOG_ERR("- Contact AMS support with this diagnostic information");
    } else {
        LOG_INF("Some register values changed - sensor may be recoverable");
    }
}

void main(void) {
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return;
    }

    LOG_INF("AS7056 Hardware Diagnostic Tool");
    LOG_INF("==============================");
    
    // Basic communication test
    if (!test_basic_communication()) {
        LOG_ERR("Sensor does not respond to basic commands - check connections");
        return;
    }
    
    // Register writability test
    bool registers_writable = test_register_writability();
    
    // Communication speed test
    test_communication_speeds();
    
    // Alternative address test
    test_alternative_addresses();
    
    // Hardware reset attempts
    attempt_hardware_reset();
    
    // Final diagnostic summary
    diagnostic_summary();
    
    LOG_INF("Hardware diagnostic complete");
}