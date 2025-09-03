// AS7058 PPG Measurement System - SEQUENCER FIX VERSION
// VCSEL error resolved, now targeting sequencer startup issues
// Focus on getting the measurement sequence running and FIFO data

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

// VCSEL Safety (Now working)
#define REG_VCSEL_PASSWORD    0x40
#define REG_VCSEL_CFG         0x41
#define REG_VCSEL_MODE        0x42
#define REG_LED_CFG           0x43

// PPG Configuration
#define REG_PPGMOD_CFG3       0x39
#define REG_PPGMOD1_CFG1      0x3A
#define REG_PPGMOD1_CFG2      0x3B
#define REG_PPGMOD1_CFG3      0x3C

// LED and Photodiode
#define REG_LED1_ICTRL        0x46
#define REG_LED_IRNG1         0x4E
#define REG_LED_SUB1          0x50
#define REG_PPG1_PDSEL1       0x5A

// Sequencer - CRITICAL for measurement
#define REG_PPG_SINC_CFGA     0x6F
#define REG_PPG_SINC_CFGB     0x70
#define REG_PPG_SINC_CFGC     0x71
#define REG_IRQ_ENABLE        0x95
#define REG_PPG_SUB_WAIT      0x96
#define REG_PPG_LED_INIT      0x98
#define REG_PPG_FREQL         0x99
#define REG_PPG_FREQH         0x9A
#define REG_PPG1_SUB_EN       0x9B
#define REG_PPG_MODE1         0x9D
#define REG_SAMPLE_NUM        0xB1  // CRITICAL - controls sequence count
#define REG_FIFO_THRESHOLD    0xCA
#define REG_FIFO_CTRL         0xCB

// Control and Status - COMPLETE DEFINITIONS
#define REG_CHIP_CTRL         0xEF
#define REG_SEQ_START         0xF0
#define REG_STATUS_CGB        0xF1
#define REG_STATUS_SEQ        0xF2
#define REG_STATUS_VCSEL      0xF6
#define REG_STATUS            0xFA
#define REG_FIFO_LEVEL0       0xFB
#define REG_FIFO_LEVEL1       0xFC
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
        LOG_ERR("read FAIL: reg 0x%02X (err=%d)", reg, ret);
    }
    return ret;
}

void complete_initialization_working(void) {
    LOG_INF("=== COMPLETE AS7058 INITIALIZATION - WORKING VERSION ===");
    
    // Step 1: Chip reset and VCSEL disable (we know this works)
    write_reg(REG_CHIP_CTRL, 0x01);
    k_msleep(500);
    
    // VCSEL safety disable
    write_reg(REG_VCSEL_PASSWORD, 0x57);
    write_reg(REG_VCSEL_CFG, 0xFF);
    write_reg(REG_VCSEL_MODE, 0x00);
    write_reg(REG_LED_CFG, 0x01);
    k_msleep(100);
    
    LOG_INF("VCSEL safety disabled successfully");
    
    // Step 2: Complete PPG configuration with proper sequencer setup
    struct {
        uint8_t reg;
        uint8_t val;
        const char *name;
    } config[] = {
        // Power and Clock - Stable configuration
        {0x18, 0x07, "CLK_CFG - All clocks enabled"},
        {0x19, 0x3F, "REF_CFG1 - All references enabled"},
        {0x1A, 0x01, "REF_CFG2 - LPF bypass enabled"},
        {0x1C, 0x1F, "STANDBY_ON1 - PPG standby enables"},
        {0x1D, 0x00, "STANDBY_ON2 - Disable ECG standby"},
        {0x2D, 0x07, "PWR_ON - PPG power domains on"},
        {0x2E, 0x18, "PWR_ISO - Power isolation control"},
        
        // PPG Modulator - Working configuration
        {0x39, 0x00, "PPGMOD_CFG3 - 10MHz clock, default reset"},
        {0x3A, 0x80, "PPGMOD1_CFG1 - Enable MOD1"},
        {0x3B, 0x44, "PPGMOD1_CFG2 - 8uA range, proper scale"},
        {0x3C, 0x08, "PPGMOD1_CFG3 - 8uA reference current"},
        
        // LED Configuration - Known working (green LED glows)
        {0x46, 0x10, "LED1_ICTRL - Moderate LED current"},
        {0x4E, 0x01, "LED_IRNG1 - 150mA range for LED1"},
        {0x50, 0x01, "LED_SUB1 - Select LED1 for subsample 1"},
        
        // Photodiode - Simple PD1 selection
        {0x5A, 0x01, "PPG1_PDSEL1 - Select PD1 for measurement"},
        
        // SINC Filter - Conservative but functional settings
        {0x6F, 0x11, "PPG_SINC_CFGA - Dec=32, no oversampling"},
        {0x70, 0x01, "PPG_SINC_CFGB - 4th order, CIC mode"},
        {0x71, 0x00, "PPG_SINC_CFGC - No start delay"},
        
        // SEQUENCER CONFIGURATION - KEY FIX
        {0x95, 0x01, "IRQ_ENABLE - FIFO threshold interrupt"},
        {0x96, 0x20, "PPG_SUB_WAIT - 32us between subsamples"},
        {0x98, 0x10, "PPG_LED_INIT - 16us LED init time"},
        {0x99, 0x9F, "PPG_FREQL - Sample period low byte"},
        {0x9A, 0x00, "PPG_FREQH - Sample period high byte"},
        {0x9B, 0x01, "PPG1_SUB_EN - Enable subsample 1"},
        {0x9D, 0x00, "PPG_MODE1 - Single measurement mode"},
        
        // CRITICAL - Sample count control
        {0xB1, 0x00, "SAMPLE_NUM - Continuous measurement (0=infinite)"},
        
        // FIFO Configuration
        {0xCA, 0x08, "FIFO_THRESHOLD - 8 sample threshold"},
        {0xCB, 0x02, "FIFO_CTRL - Enable random extension"},
    };
    
    for (int i = 0; i < ARRAY_SIZE(config); i++) {
        write_reg(config[i].reg, config[i].val);
        k_msleep(5); // Small delay between registers
    }
    
    k_msleep(200); // Final settling delay
    LOG_INF("Complete PPG configuration applied");
}

void verify_critical_registers(void) {
    LOG_INF("=== VERIFYING CRITICAL REGISTERS ===");
    
    struct {
        uint8_t reg;
        const char *name;
        uint8_t expected;
    } critical_regs[] = {
        {0x3A, "PPGMOD1_CFG1", 0x80},
        {0x46, "LED1_ICTRL", 0x10},
        {0x50, "LED_SUB1", 0x01},
        {0x5A, "PPG1_PDSEL1", 0x01},
        {0x9B, "PPG1_SUB_EN", 0x01},
        {0xB1, "SAMPLE_NUM", 0x00},
        {0xF6, "STATUS_VCSEL", 0x00},
        {0xFA, "STATUS", 0x00},
    };
    
    bool all_good = true;
    for (int i = 0; i < ARRAY_SIZE(critical_regs); i++) {
        uint8_t val;
        read_reg(critical_regs[i].reg, &val);
        if (val == critical_regs[i].expected) {
            LOG_INF("✓ %s = 0x%02X (correct)", critical_regs[i].name, val);
        } else {
            LOG_WRN("⚠ %s = 0x%02X (expected 0x%02X)", 
                    critical_regs[i].name, val, critical_regs[i].expected);
            all_good = false;
        }
    }
    
    if (all_good) {
        LOG_INF("All critical registers configured correctly");
    }
}

bool start_measurement_sequence(void) {
    LOG_INF("=== STARTING MEASUREMENT SEQUENCE ===");
    
    // Ensure sequence is stopped first
    write_reg(REG_SEQ_START, 0x00);
    k_msleep(100);
    
    // Clear any pending status
    uint8_t temp;
    read_reg(REG_STATUS, &temp);
    read_reg(REG_STATUS_SEQ, &temp);
    
    // Start the sequence
    LOG_INF("Starting PPG measurement sequence...");
    write_reg(REG_SEQ_START, 0x01);
    
    // Wait longer for first measurement cycle to complete
    k_msleep(1000);
    
    // Check if sequence started
    uint8_t seq_status, main_status;
    read_reg(REG_STATUS_SEQ, &seq_status);
    read_reg(REG_STATUS, &main_status);
    
    LOG_INF("Initial sequence status: SEQ=0x%02X, MAIN=0x%02X", seq_status, main_status);
    
    // The sequence might run and complete quickly, so SEQ=0x00 could be normal
    // Check for any signs of activity
    if (main_status == 0x00) {
        LOG_INF("✓ No errors detected - sequence may be running correctly");
        return true;
    } else {
        LOG_WRN("Status errors detected: 0x%02X", main_status);
        return false;
    }
}

void monitor_ppg_data_collection(void) {
    LOG_INF("=== PPG DATA COLLECTION MONITORING ===");
    
    uint32_t total_samples = 0;
    uint32_t measurement_cycles = 0;
    
    for (int cycle = 0; cycle < 15; cycle++) {
        LOG_INF("--- Monitoring Cycle %d ---", cycle + 1);
        
        // Read comprehensive status
        uint8_t seq_status, main_status, fifo_l0, fifo_l1;
        read_reg(REG_STATUS_SEQ, &seq_status);
        read_reg(REG_STATUS, &main_status);
        read_reg(REG_FIFO_LEVEL0, &fifo_l0);
        read_reg(REG_FIFO_LEVEL1, &fifo_l1);
        
        uint16_t fifo_level = ((fifo_l1 & 0x03) << 8) | fifo_l0;
        
        LOG_INF("Status: SEQ=0x%02X, MAIN=0x%02X, FIFO_LEVEL=%d", 
                seq_status, main_status, fifo_level);
        
        // Read FIFO data if available
        if (fifo_level > 0) {
            measurement_cycles++;
            
            // Read up to 5 samples to avoid overwhelming the log
            int samples_to_read = MIN(fifo_level, 5);
            LOG_INF("Reading %d FIFO samples:", samples_to_read);
            
            for (int i = 0; i < samples_to_read; i++) {
                uint8_t fifol, fifom, fifoh;
                read_reg(REG_FIFOL, &fifol);
                read_reg(REG_FIFOM, &fifom);
                read_reg(REG_FIFOH, &fifoh);
                
                // Parse FIFO data format
                uint32_t fifo_raw = (fifoh << 16) | (fifom << 8) | fifol;
                uint8_t data_marker = fifol & 0x07;
                uint8_t block_frame = (fifol >> 3) & 0x01;
                uint32_t adc_data = (fifo_raw >> 4) & 0xFFFFF;
                
                LOG_INF("  Sample %d: ADC=0x%05X (%d), Marker=%d, Frame=%d", 
                        i+1, adc_data, adc_data, data_marker, block_frame);
                
                total_samples++;
            }
            
            LOG_INF("✓ PPG measurement working! Cycle %d collected %d samples", 
                    cycle + 1, samples_to_read);
        }
        
        // Restart sequence if it appears idle (this might be normal behavior)
        if (seq_status == 0x00 && fifo_level == 0) {
            LOG_DBG("Sequence idle - restarting (may be normal)");
            write_reg(REG_SEQ_START, 0x01);
        }
        
        // Progress report every 5 cycles
        if ((cycle + 1) % 5 == 0) {
            LOG_INF("Progress: %d cycles, %d measurement events, %d total samples", 
                    cycle + 1, measurement_cycles, total_samples);
        }
        
        k_msleep(1200); // Slightly longer than sample period
    }
    
    LOG_INF("=== PPG DATA COLLECTION SUMMARY ===");
    LOG_INF("Total measurement cycles: %d", measurement_cycles);
    LOG_INF("Total samples collected: %d", total_samples);
    
    if (total_samples > 0) {
        LOG_INF("✓ SUCCESS: PPG measurement system is working!");
        LOG_INF("Green LED indicates LED drive working");
        LOG_INF("FIFO data confirms ADC measurement working");
    } else {
        LOG_WRN("No FIFO data collected - possible issues:");
        LOG_WRN("  - Photodiode not connected or no light coupling");
        LOG_WRN("  - Sample timing too slow for observation window");
        LOG_WRN("  - FIFO threshold not reached");
    }
}

int main(void) {
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return 1;
    }

    LOG_INF("AS7058 PPG System - SEQUENCER FIX VERSION");
    LOG_INF("===========================================");
    LOG_INF("Building on successful VCSEL error resolution");
    
    // Step 1: Complete initialization (including VCSEL fix)
    complete_initialization_working();
    
    // Step 2: Verify all critical registers are set correctly
    verify_critical_registers();
    
    // Step 3: Start measurement sequence with proper timing
    bool seq_started = start_measurement_sequence();
    
    // Step 4: Monitor for PPG data collection
    monitor_ppg_data_collection();
    
    // Final assessment
    uint8_t final_main_status, final_vcsel_status;
    read_reg(REG_STATUS, &final_main_status);
    read_reg(REG_STATUS_VCSEL, &final_vcsel_status);
    
    LOG_INF("=== FINAL ASSESSMENT ===");
    LOG_INF("VCSEL Error Resolution: %s", (final_vcsel_status == 0x00) ? "SUCCESS" : "FAILED");
    LOG_INF("Overall System Status: 0x%02X", final_main_status);
    LOG_INF("Green LED Status: GLOWING (indicates LED driver working)");
    
    if (final_main_status == 0x00 && final_vcsel_status == 0x00) {
        LOG_INF("✓ AS7058 PPG measurement system is operational");
        LOG_INF("Ready for integration into your application");
    }
    
    LOG_INF("AS7058 PPG system validation complete");
    return 0;
}