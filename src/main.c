// AS7058 Enhanced PPG System - COMPREHENSIVE DIAGNOSTIC VERSION
// Addresses persistent VCSEL errors through systematic chip reset and configuration
// Includes full register diagnostics and alternative measurement approaches

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(as7058_enhanced, LOG_LEVEL_DBG);

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));
#define AS7058_I2C_ADDR 0x55

// Essential AS7058 Register Definitions
#define REG_CLK_CFG           0x18
#define REG_REF_CFG1          0x19
#define REG_REF_CFG2          0x1A
#define REG_STANDBY_ON1       0x1C
#define REG_STANDBY_ON2       0x1D
#define REG_PWR_ON            0x2D
#define REG_PWR_ISO           0x2E

// VCSEL Safety System (CRITICAL)
#define REG_VCSEL_PASSWORD    0x40
#define REG_VCSEL_CFG         0x41
#define REG_VCSEL_MODE        0x42
#define REG_LED_CFG           0x43

// PPG Core Configuration
#define REG_PPGMOD_CFG3       0x39
#define REG_PPGMOD1_CFG1      0x3A
#define REG_PPGMOD1_CFG2      0x3B
#define REG_PPGMOD1_CFG3      0x3C

// LED and Photodiode
#define REG_LED1_ICTRL        0x46
#define REG_LED_IRNG1         0x4E
#define REG_LED_SUB1          0x50
#define REG_PPG1_PDSEL1       0x5A

// Measurement Control
#define REG_PPG_SINC_CFGA     0x6F
#define REG_PPG_SINC_CFGB     0x70
#define REG_IRQ_ENABLE        0x95
#define REG_PPG_FREQL         0x99
#define REG_PPG1_SUB_EN       0x9B
#define REG_PPG_MODE1         0x9D
#define REG_FIFO_THRESHOLD    0xCA

// Control and Status
#define REG_CHIP_CTRL         0xEF
#define REG_SEQ_START         0xF0
#define REG_STATUS            0xFA
#define REG_STATUS_SEQ        0xF2
#define REG_STATUS_VCSEL      0xF6
#define REG_FIFO_LEVEL0       0xFB
#define REG_FIFOL             0xFD
#define REG_FIFOM             0xFE
#define REG_FIFOH             0xFF

int write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    int ret = i2c_write(i2c_dev, buf, 2, AS7058_I2C_ADDR);
    if (ret != 0) {
        LOG_ERR("WRITE FAIL: reg 0x%02X = 0x%02X (err=%d)", reg, val, ret);
    } else {
        LOG_DBG("WRITE: reg 0x%02X = 0x%02X", reg, val);
    }
    return ret;
}

int read_reg(uint8_t reg, uint8_t *val) {
    int ret = i2c_write_read(i2c_dev, AS7058_I2C_ADDR, &reg, 1, val, 1);
    if (ret != 0) {
        LOG_ERR("READ FAIL: reg 0x%02X (err=%d)", reg, ret);
    }
    return ret;
}

void perform_chip_reset(void) {
    LOG_INF("=== PERFORMING COMPLETE CHIP RESET ===");
    
    // Software reset sequence
    write_reg(REG_CHIP_CTRL, 0x01);  // Trigger power-on reset
    k_msleep(500);  // Extended reset delay
    
    // Wait for chip to be ready
    uint8_t status;
    int retries = 20;
    do {
        k_msleep(50);
        read_reg(0xEC, &status);  // Read SILICON_ID
        retries--;
    } while (status != 0x92 && retries > 0);
    
    if (status == 0x92) {
        LOG_INF("Chip reset successful - Silicon ID confirmed: 0x%02X", status);
    } else {
        LOG_ERR("Chip reset failed - Silicon ID: 0x%02X", status);
    }
}

void comprehensive_vcsel_disable(void) {
    LOG_INF("=== COMPREHENSIVE VCSEL SAFETY DISABLE ===");
    
    // Step 1: Set VCSEL password (required for some operations)
    write_reg(REG_VCSEL_PASSWORD, 0x57);
    k_msleep(10);
    
    // Step 2: Complete VCSEL safety disable sequence
    write_reg(REG_VCSEL_CFG, 0xFF);    // Disable ALL VCSEL safety features
    write_reg(REG_VCSEL_MODE, 0x00);   // Force all pins to LED mode
    write_reg(REG_LED_CFG, 0x01);      // Disable LED watchdog
    k_msleep(100);
    
    // Step 3: Verify VCSEL status
    uint8_t vcsel_status, vcsel_cfg, vcsel_mode;
    read_reg(REG_STATUS_VCSEL, &vcsel_status);
    read_reg(REG_VCSEL_CFG, &vcsel_cfg);
    read_reg(REG_VCSEL_MODE, &vcsel_mode);
    
    LOG_INF("VCSEL Status: 0x%02X, CFG: 0x%02X, MODE: 0x%02X", 
            vcsel_status, vcsel_cfg, vcsel_mode);
    
    // Step 4: Clear any VCSEL error flags by reading status registers
    uint8_t temp;
    read_reg(0xF7, &temp);  // STATUS_VCSEL_VSS
    read_reg(0xF8, &temp);  // STATUS_VCSEL_VDD
    read_reg(REG_STATUS_VCSEL, &temp);
    read_reg(REG_STATUS, &temp);
    
    LOG_INF("VCSEL safety system comprehensively disabled");
}

void minimal_ppg_configuration(void) {
    LOG_INF("=== MINIMAL PPG CONFIGURATION ===");
    
    // Absolute minimal configuration to get PPG working
    struct {
        uint8_t reg;
        uint8_t val;
        const char *name;
    } minimal_config[] = {
        // Essential clocks and references
        {0x18, 0x03, "CLK_CFG - LF and HF oscillators only"},
        {0x19, 0x1F, "REF_CFG1 - Essential references only"},
        {0x1A, 0x01, "REF_CFG2 - Bypass low-pass filter"},
        
        // Power management - minimal domains
        {0x1C, 0x07, "STANDBY_ON1 - PPG essentials only"},
        {0x1D, 0x00, "STANDBY_ON2 - Disable ECG standby"},
        {0x2D, 0x07, "PWR_ON - Enable PPG domains"},
        {0x2E, 0x18, "PWR_ISO - Power isolation"},
        
        // PPG Modulator - simplest configuration
        {0x39, 0x00, "PPGMOD_CFG3 - 10MHz, default timing"},
        {0x3A, 0x80, "PPGMOD1_CFG1 - Enable MOD1, default cap"},
        {0x3B, 0x34, "PPGMOD1_CFG2 - 4uA range, 0.625 scale"},
        {0x3C, 0x04, "PPGMOD1_CFG3 - 4uA reference"},
        
        // LED configuration - minimal current
        {0x46, 0x08, "LED1_ICTRL - Low LED current"},
        {0x4E, 0x01, "LED_IRNG1 - 150mA range for LED1"},
        {0x50, 0x01, "LED_SUB1 - Select LED1"},
        
        // Photodiode - simple PD1 selection  
        {0x5A, 0x01, "PPG1_PDSEL1 - Select PD1"},
        
        // Filter - conservative settings
        {0x6F, 0x01, "PPG_SINC_CFGA - Dec=32, no oversampling"},
        {0x70, 0x01, "PPG_SINC_CFGB - 4th order CIC"},
        
        // Sequencer - minimal setup
        {0x95, 0x01, "IRQ_ENABLE - FIFO threshold only"},
        {0x99, 0xFF, "PPG_FREQL - Slow sample rate"},
        {0x9B, 0x01, "PPG1_SUB_EN - Enable subsample 1 only"},
        {0x9D, 0x00, "PPG_MODE1 - Single measurement"},
        {0xCA, 0x05, "FIFO_THRESHOLD - Low threshold"},
    };
    
    for (int i = 0; i < ARRAY_SIZE(minimal_config); i++) {
        write_reg(minimal_config[i].reg, minimal_config[i].val);
        k_msleep(10);
    }
    
    k_msleep(200);
    LOG_INF("Minimal PPG configuration complete");
}

void diagnostic_register_dump(void) {
    LOG_INF("=== DIAGNOSTIC REGISTER DUMP ===");
    
    struct {
        uint8_t reg;
        const char *name;
    } key_registers[] = {
        {0x18, "CLK_CFG"},
        {0x19, "REF_CFG1"}, 
        {0x2D, "PWR_ON"},
        {0x2E, "PWR_ISO"},
        {0x41, "VCSEL_CFG"},
        {0x42, "VCSEL_MODE"},
        {0x43, "LED_CFG"},
        {0x3A, "PPGMOD1_CFG1"},
        {0x46, "LED1_ICTRL"},
        {0x50, "LED_SUB1"},
        {0x5A, "PPG1_PDSEL1"},
        {0x9B, "PPG1_SUB_EN"},
        {0xF0, "SEQ_START"},
        {0xF2, "STATUS_SEQ"},
        {0xF6, "STATUS_VCSEL"},
        {0xFA, "STATUS"},
    };
    
    for (int i = 0; i < ARRAY_SIZE(key_registers); i++) {
        uint8_t val;
        read_reg(key_registers[i].reg, &val);
        LOG_INF("  %s (0x%02X) = 0x%02X", 
                key_registers[i].name, key_registers[i].reg, val);
    }
}

bool attempt_measurement_recovery(void) {
    LOG_INF("=== ATTEMPTING MEASUREMENT RECOVERY ===");
    
    // Try different approaches to clear the VCSEL error
    
    // Approach 1: Complete power cycle
    LOG_INF("Approach 1: Power domain cycling");
    write_reg(REG_PWR_ON, 0x00);      // Power down all domains
    write_reg(REG_PWR_ISO, 0x1F);     // Isolate all domains
    k_msleep(200);
    
    comprehensive_vcsel_disable();     // Re-disable VCSEL safety
    
    write_reg(REG_PWR_ISO, 0x18);     // Release isolation
    write_reg(REG_PWR_ON, 0x07);      // Power up PPG domains
    k_msleep(200);
    
    uint8_t status;
    read_reg(REG_STATUS, &status);
    if ((status & 0x20) == 0) {
        LOG_INF("✓ Power cycling cleared VCSEL error");
        return true;
    }
    
    // Approach 2: Sequence timing adjustment
    LOG_INF("Approach 2: Alternative timing");
    write_reg(0x96, 0x64);  // Longer subsample wait
    write_reg(0x98, 0x32);  // Longer LED init time
    write_reg(REG_PPG_FREQL, 0x7F);  // Even slower sample rate
    k_msleep(100);
    
    read_reg(REG_STATUS, &status);
    if ((status & 0x20) == 0) {
        LOG_INF("✓ Timing adjustment cleared VCSEL error");
        return true;
    }
    
    // Approach 3: LED current reduction
    LOG_INF("Approach 3: Ultra-low LED current");
    write_reg(REG_LED1_ICTRL, 0x02);  // Minimal LED current
    write_reg(REG_LED_IRNG1, 0x00);   // 25mA range
    k_msleep(100);
    
    read_reg(REG_STATUS, &status);
    if ((status & 0x20) == 0) {
        LOG_INF("✓ Low current cleared VCSEL error");
        return true;
    }
    
    LOG_WRN("All recovery approaches failed - hardware issue likely");
    return false;
}

void main(void) {
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return;
    }

    LOG_INF("AS7058 COMPREHENSIVE DIAGNOSTIC AND RECOVERY");
    LOG_INF("===============================================");
    
    // Step 1: Complete chip reset to clean slate
    perform_chip_reset();
    
    // Step 2: Comprehensive VCSEL safety disable
    comprehensive_vcsel_disable();
    
    // Step 3: Apply minimal PPG configuration
    minimal_ppg_configuration();
    
    // Step 4: Diagnostic register dump
    diagnostic_register_dump();
    
    // Step 5: Check initial status
    uint8_t initial_status, vcsel_status;
    read_reg(REG_STATUS, &initial_status);
    read_reg(REG_STATUS_VCSEL, &vcsel_status);
    LOG_INF("Initial status - MAIN: 0x%02X, VCSEL: 0x%02X", initial_status, vcsel_status);
    
    // Step 6: Attempt recovery if VCSEL error persists
    bool recovery_success = true;
    if (initial_status & 0x20) {
        LOG_WRN("VCSEL error present - attempting recovery");
        recovery_success = attempt_measurement_recovery();
    }
    
    // Step 7: Try to start measurement
    LOG_INF("=== ATTEMPTING PPG MEASUREMENT START ===");
    write_reg(REG_SEQ_START, 0x01);
    k_msleep(500);  // Longer wait for first measurement
    
    // Step 8: Monitor for a few cycles
    for (int i = 0; i < 10; i++) {
        uint8_t seq_status, main_status, vcsel_stat, fifo_level;
        read_reg(REG_STATUS_SEQ, &seq_status);
        read_reg(REG_STATUS, &main_status);
        read_reg(REG_STATUS_VCSEL, &vcsel_stat);
        read_reg(REG_FIFO_LEVEL0, &fifo_level);
        
        LOG_INF("Cycle %d: SEQ=0x%02X, MAIN=0x%02X, VCSEL=0x%02X, FIFO=%d", 
                i+1, seq_status, main_status, vcsel_stat, fifo_level);
        
        // Check for FIFO data
        if (fifo_level > 0) {
            uint8_t fifol, fifom, fifoh;
            read_reg(REG_FIFOL, &fifol);
            read_reg(REG_FIFOM, &fifom);
            read_reg(REG_FIFOH, &fifoh);
            
            uint32_t adc_data = ((fifoh << 16) | (fifom << 8) | fifol) >> 4;
            LOG_INF("✓ PPG DATA: 0x%05X (%d) - MEASUREMENT SUCCESS!", adc_data, adc_data);
        }
        
        // Restart sequence if idle
        if (seq_status == 0x00) {
            write_reg(REG_SEQ_START, 0x01);
        }
        
        k_msleep(1000);
    }
    
    // Final status report
    uint8_t final_main, final_vcsel;
    read_reg(REG_STATUS, &final_main);
    read_reg(REG_STATUS_VCSEL, &final_vcsel);
    
    LOG_INF("=== FINAL DIAGNOSTIC RESULTS ===");
    LOG_INF("Final Status - MAIN: 0x%02X, VCSEL: 0x%02X", final_main, final_vcsel);
    
    if ((final_main & 0x20) == 0) {
        LOG_INF("✓ SUCCESS: VCSEL error resolved - AS7058 operational");
    } else {
        LOG_ERR("❌ FAILURE: VCSEL error persists (0x%02X)", final_vcsel);
        LOG_ERR("Hardware issues detected:");
        if (final_vcsel & 0x08) LOG_ERR("  - VCSEL short to VDD detected");
        if (final_vcsel & 0x04) LOG_ERR("  - VCSEL short to VSS detected");
        if (final_vcsel & 0x10) LOG_ERR("  - LED watchdog timeout");
        if (final_vcsel & 0x03) LOG_ERR("  - VCSEL analog watchdog error");
        
        LOG_ERR("Recommendations:");
        LOG_ERR("  1. Check LED connections (shorts, open circuits)");
        LOG_ERR("  2. Verify power supply voltages (1.8V, LED supply)");
        LOG_ERR("  3. Check I2C communication integrity");
        LOG_ERR("  4. Inspect PCB layout for ground loops or noise");
    }
    
    LOG_INF("AS7058 comprehensive diagnostic complete");
}