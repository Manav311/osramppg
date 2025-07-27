// AS7056 Enhanced Sequence Control & Data Acquisition
// Addresses the sequence going IDLE issue and enables continuous measurement

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(as7056_enhanced, LOG_LEVEL_DBG);

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));
#define AS7056_I2C_ADDR 0x55

// Register definitions from your working dump
#define REG_CONTROL           0x10
#define REG_CGB_CFG           0x11
#define REG_REF_CFGA          0x15
#define REG_REF_CFGB          0x16
#define REG_MOD1_CFGA         0x1B
#define REG_MOD1_CFGB         0x1C
#define REG_MOD1_CFGC         0x1D
#define REG_MOD1_CFGD         0x1E
#define REG_MOD1_CFGE         0x1F
#define REG_MOD1_CFGF         0x20

// Sequence enable registers (from your dump)
#define REG_MOD1_SEQ1_SUB_EN  0x4C
#define REG_MOD1_SEQ2_SUB_EN  0x4D
#define REG_MOD2_SEQ1_SUB_EN  0x4E
#define REG_MOD2_SEQ2_SUB_EN  0x4F

// LED current registers
#define REG_SEQ1_LED1_CURR    0x29
#define REG_SEQ2_LED1_CURR    0x2A
#define REG_SEQ1_LED2_CURR    0x2B
#define REG_SEQ2_LED2_CURR    0x2C

// Sequence configuration
#define REG_SEQ_CONFIG        0x43
#define REG_SEQ_FREQL         0x46
#define REG_SEQ_FREQH         0x47
#define REG_SEQ1_FREQDIVL     0x48
#define REG_SEQ1_FREQDIVH     0x49
#define REG_SEQ2_FREQDIVL     0x4A
#define REG_SEQ2_FREQDIVH     0x4B

// Photodiode configuration
#define REG_PD_SEQ1_SUB1      0x53
#define REG_PD_SEQ2_SUB1      0x5B

// Control registers
#define REG_IRQ_ENABLE        0x3F
#define REG_SEQ_START         0xF0
#define REG_STATUS            0xFA
#define REG_STATUS_SEQ        0xF5
#define REG_FIFO_LEVEL0       0xFB
#define REG_FIFO_LEVEL1       0xFC
#define REG_FIFOL             0xFD
#define REG_FIFOM             0xFE
#define REG_FIFOH             0xFF

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

void complete_initialization_from_dump(void) {
    LOG_INF("=== COMPLETE INITIALIZATION FROM WORKING DUMP ===");
    
    // Apply ALL configuration values from your working dump
    struct {
        uint8_t reg;
        uint8_t val;
        const char *name;
    } config[] = {
        // Core configuration
        {0x10, 0x00, "CONTROL"},
        {0x11, 0x07, "CGB_CFG"},
        {0x12, 0x00, "INT_CFG"},
        {0x13, 0x40, "CSXN_CFG"},
        {0x14, 0x40, "IO_CFG"},
        {0x15, 0xED, "REF_CFGA"},
        {0x16, 0x06, "REF_CFGB"},
        
        // Modulation configuration
        {0x19, 0x00, "MOD_CFGA"},
        {0x1A, 0x00, "MOD_CFGB"},
        {0x1B, 0x16, "MOD1_CFGA"},
        {0x1C, 0x04, "MOD1_CFGB"},
        {0x1D, 0x27, "MOD1_CFGC"},
        {0x1E, 0x07, "MOD1_CFGD"},
        {0x1F, 0x1F, "MOD1_CFGE"},
        {0x20, 0x1F, "MOD1_CFGF"},
        {0x21, 0x06, "MOD2_CFGA"},
        {0x22, 0x04, "MOD2_CFGB"},
        {0x23, 0x00, "MOD2_CFGC"},
        {0x24, 0x00, "MOD2_CFGD"},
        {0x25, 0x00, "MOD2_CFGE"},
        {0x26, 0x00, "MOD2_CFGF"},
        
        // VCSEL and LED configuration
        {0x28, 0x00, "VCSEL_CFG"},
        {0x29, 0x00, "SEQ1_LED1_CURR"},
        {0x2A, 0x1F, "SEQ2_LED1_CURR"},
        {0x2B, 0x0C, "SEQ1_LED2_CURR"},
        {0x2C, 0x00, "SEQ2_LED2_CURR"},
        {0x2D, 0x00, "SEQ1_LED3_CURR"},
        {0x2E, 0x00, "SEQ2_LED3_CURR"},
        
        // LED sequence configuration
        {0x2F, 0x20, "LED_SEQ1_SUB12"},
        {0x30, 0x00, "LED_SEQ1_SUB34"},
        {0x31, 0x00, "LED_SEQ1_SUB56"},
        {0x32, 0x00, "LED_SEQ1_SUB78"},
        {0x33, 0x10, "LED_SEQ2_SUB12"},
        {0x34, 0x00, "LED_SEQ2_SUB34"},
        {0x35, 0x00, "LED_LOWVDS_WAIT"},
        
        // IRQ configuration - IMPORTANT!
        {0x3F, 0xFF, "IRQ_ENABLE"},
        
        // Sequence timing and control
        {0x40, 0x00, "SEQ_SAMPLE"},
        {0x41, 0x1E, "SEQ_SUB_WAIT"},
        {0x42, 0x01, "SEQ_MODCONF"},
        {0x43, 0x88, "SEQ_CONFIG"},
        {0x44, 0x00, "SEQ_SAR_WAIT"},
        {0x45, 0x0A, "SEQ_LED_INIT"},
        {0x46, 0x3F, "SEQ_FREQL"},
        {0x47, 0x01, "SEQ_FREQH"},
        {0x48, 0x00, "SEQ1_FREQDIVL"},
        {0x49, 0x00, "SEQ1_FREQDIVH"},
        {0x4A, 0x09, "SEQ2_FREQDIVL"},
        {0x4B, 0x00, "SEQ2_FREQDIVH"},
        
        // CRITICAL: Sequence enable registers
        {0x4C, 0x01, "MOD1_SEQ1_SUB_EN"},
        {0x4D, 0x01, "MOD1_SEQ2_SUB_EN"},
        {0x4E, 0x00, "MOD2_SEQ1_SUB_EN"},
        {0x4F, 0x00, "MOD2_SEQ2_SUB_EN"},
        
        // Photodiode configuration
        {0x53, 0x10, "PD_SEQ1_SUB1"},
        {0x5B, 0x20, "PD_SEQ2_SUB1"},
        
        // SINC filter configuration
        {0x61, 0x64, "SEQ1_SINC_CFGA"},
        {0x62, 0x01, "SEQ1_SINC_CFGB"},
        {0x63, 0x00, "SEQ1_SINC_CFGC"},
        {0x64, 0x64, "SEQ2_SINC_CFGA"},
        {0x65, 0x01, "SEQ2_SINC_CFGB"},
        {0x66, 0x00, "SEQ2_SINC_CFGC"},
        
        // FIFO configuration
        {0xD0, 0x0F, "FIFO_THRESHOLD"},
        {0xD1, 0x00, "FIFO_CTRL"},
    };
    
    LOG_INF("Writing %d configuration registers...", ARRAY_SIZE(config));
    
    for (int i = 0; i < ARRAY_SIZE(config); i++) {
        write_reg(config[i].reg, config[i].val);
        if (i % 10 == 9) {
            k_msleep(10); // Small delay every 10 writes
        }
    }
    
    k_msleep(50); // Let configuration settle
    LOG_INF("Complete configuration applied");
}

bool start_continuous_sequence(void) {
    LOG_INF("=== STARTING CONTINUOUS SEQUENCE ===");
    
    // Check initial status
    uint8_t status_seq, status_main;
    read_reg(REG_STATUS_SEQ, &status_seq);
    read_reg(REG_STATUS, &status_main);
    LOG_INF("Pre-start status - SEQ: 0x%02X, MAIN: 0x%02X", status_seq, status_main);
    
    // Ensure sequence is stopped first
    write_reg(REG_SEQ_START, 0x00);
    k_msleep(100);
    
    // Check that critical enable bits are set
    uint8_t seq1_en, seq2_en;
    read_reg(REG_MOD1_SEQ1_SUB_EN, &seq1_en);
    read_reg(REG_MOD1_SEQ2_SUB_EN, &seq2_en);
    LOG_INF("Sequence enables - SEQ1: 0x%02X, SEQ2: 0x%02X", seq1_en, seq2_en);
    
    if (seq1_en != 0x01 || seq2_en != 0x01) {
        LOG_WRN("Sequence enable registers not set correctly, fixing...");
        write_reg(REG_MOD1_SEQ1_SUB_EN, 0x01);
        write_reg(REG_MOD1_SEQ2_SUB_EN, 0x01);
        k_msleep(50);
    }
    
    // Start the sequence
    LOG_INF("Starting measurement sequence...");
    write_reg(REG_SEQ_START, 0x01);
    k_msleep(200);
    
    // Check sequence status after start
    read_reg(REG_STATUS_SEQ, &status_seq);
    read_reg(REG_STATUS, &status_main);
    LOG_INF("Post-start status - SEQ: 0x%02X, MAIN: 0x%02X", status_seq, status_main);
    
    // Continuous restart to keep sequence running
    for (int retry = 0; retry < 3; retry++) {
        if (status_seq != 0x00) {
            LOG_INF("✓ Sequence active on attempt %d (status: 0x%02X)", retry + 1, status_seq);
            return true;
        }
        
        LOG_INF("Retry %d: Restarting sequence...", retry + 1);
        write_reg(REG_SEQ_START, 0x00);
        k_msleep(50);
        write_reg(REG_SEQ_START, 0x01);
        k_msleep(200);
        
        read_reg(REG_STATUS_SEQ, &status_seq);
        read_reg(REG_STATUS, &status_main);
        LOG_INF("After retry %d - SEQ: 0x%02X, MAIN: 0x%02X", retry + 1, status_seq, status_main);
    }
    
    // If still not working, try alternative sequence start methods
    LOG_INF("Trying alternative sequence activation...");
    
    // Method 1: Pulse the sequence start multiple times
    for (int i = 0; i < 5; i++) {
        write_reg(REG_SEQ_START, 0x01);
        k_msleep(50);
        write_reg(REG_SEQ_START, 0x00);
        k_msleep(50);
    }
    write_reg(REG_SEQ_START, 0x01);
    k_msleep(200);
    
    read_reg(REG_STATUS_SEQ, &status_seq);
    if (status_seq != 0x00) {
        LOG_INF("✓ Pulse method worked! SEQ Status: 0x%02X", status_seq);
        return true;
    }
    
    LOG_WRN("Sequence activation challenging - will monitor anyway");
    return false;
}

void enhanced_data_monitoring(void) {
    LOG_INF("=== ENHANCED DATA MONITORING ===");
    
    for (int cycle = 0; cycle < 20; cycle++) {
        LOG_INF("--- Enhanced Monitor Cycle %d ---", cycle + 1);
        
        // Read all status registers
        uint8_t status_seq, status_main, status_led, fifo_l0, fifo_l1;
        read_reg(REG_STATUS_SEQ, &status_seq);
        read_reg(REG_STATUS, &status_main);
        read_reg(0xF6, &status_led);  // STATUS_LED
        read_reg(REG_FIFO_LEVEL0, &fifo_l0);
        read_reg(REG_FIFO_LEVEL1, &fifo_l1);
        
        LOG_INF("Status: SEQ=0x%02X, MAIN=0x%02X, LED=0x%02X", 
                status_seq, status_main, status_led);
        LOG_INF("FIFO: L0=0x%02X, L1=0x%02X", fifo_l0, fifo_l1);
        
        // If sequence is idle, try to restart it
        if (status_seq == 0x00) {
            LOG_INF("Sequence idle - restarting...");
            write_reg(REG_SEQ_START, 0x01);
            k_msleep(100);
            read_reg(REG_STATUS_SEQ, &status_seq);
            LOG_INF("After restart: SEQ=0x%02X", status_seq);
        }
        
        // Check for any FIFO data
        if (fifo_l0 > 0 || fifo_l1 > 0) {
            uint8_t fifol, fifom, fifoh;
            read_reg(REG_FIFOL, &fifol);
            read_reg(REG_FIFOM, &fifom);
            read_reg(REG_FIFOH, &fifoh);
            
            uint32_t fifo_data = (fifoh << 16) | (fifom << 8) | fifol;
            LOG_INF("✓ FIFO DATA: 0x%06X (%d) - MEASUREMENT WORKING!", 
                    fifo_data, fifo_data);
        }
        
        k_msleep(500);
    }
}

void main(void) {
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return;
    }

    LOG_INF("AS7056 Enhanced Sequence Control");
    LOG_INF("===============================");
    
    // Step 1: Apply complete configuration from working dump
    complete_initialization_from_dump();
    
    // Step 2: Start continuous sequence with enhanced retry logic
    bool seq_started = start_continuous_sequence();
    
    // Step 3: Enhanced monitoring with automatic restart
    enhanced_data_monitoring();
    
    LOG_INF("Enhanced monitoring complete");
    
    if (seq_started) {
        LOG_INF("✓ Sequence was successfully activated at some point");
    } else {
        LOG_WRN("⚠️  Sequence activation was inconsistent");
        LOG_INF("This may be normal for single-shot or triggered mode");
        LOG_INF("If FIFO data appeared, the sensor is working correctly");
    }
}