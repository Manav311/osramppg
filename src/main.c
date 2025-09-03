// AS7058 PPG Data Processing System - OPTIMIZED VERSION
// Focuses on efficient raw PPG data collection and processing
// Streamlined for continuous measurement and data analysis

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(as7058_ppg, LOG_LEVEL_INF);

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));
#define AS7058_I2C_ADDR 0x55

// Essential Register Definitions
#define REG_CHIP_CTRL         0xEF
#define REG_VCSEL_PASSWORD    0x40
#define REG_VCSEL_CFG         0x41
#define REG_VCSEL_MODE        0x42
#define REG_LED_CFG           0x43
#define REG_SEQ_START         0xF0
#define REG_STATUS            0xFA
#define REG_STATUS_SEQ        0xF2
#define REG_FIFO_LEVEL0       0xFB
#define REG_FIFO_LEVEL1       0xFC
#define REG_FIFOL             0xFD
#define REG_FIFOM             0xFE
#define REG_FIFOH             0xFF

// PPG Data Structure
typedef struct {
    uint32_t adc_value;
    uint8_t data_marker;
    uint8_t block_frame;
    uint32_t timestamp_ms;
    uint8_t sample_quality;
} ppg_sample_t;

// PPG Statistics
typedef struct {
    uint32_t total_samples;
    uint32_t min_value;
    uint32_t max_value;
    uint64_t sum_value;
    uint32_t avg_value;
    uint32_t range;
    uint32_t last_10_samples[10];
    uint8_t last_sample_index;
} ppg_stats_t;

static ppg_stats_t ppg_stats = {0};

int write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    int ret = i2c_write(i2c_dev, buf, 2, AS7058_I2C_ADDR);
    if (ret != 0) {
        LOG_ERR("Write failed: reg 0x%02X = 0x%02X (err=%d)", reg, val, ret);
    }
    return ret;
}

int read_reg(uint8_t reg, uint8_t *val) {
    int ret = i2c_write_read(i2c_dev, AS7058_I2C_ADDR, &reg, 1, val, 1);
    if (ret != 0) {
        LOG_ERR("Read failed: reg 0x%02X (err=%d)", reg, ret);
    }
    return ret;
}

void initialize_as7058_optimized(void) {
    LOG_INF("=== AS7058 OPTIMIZED INITIALIZATION ===");
    
    // Reset and disable VCSEL safety (proven working configuration)
    write_reg(REG_CHIP_CTRL, 0x01);
    k_msleep(500);
    
    write_reg(REG_VCSEL_PASSWORD, 0x57);
    write_reg(REG_VCSEL_CFG, 0xFF);
    write_reg(REG_VCSEL_MODE, 0x00);
    write_reg(REG_LED_CFG, 0x01);
    k_msleep(100);
    
    // Optimized PPG configuration (based on working settings)
    struct {
        uint8_t reg;
        uint8_t val;
    } config[] = {
        {0x18, 0x07}, // CLK_CFG
        {0x19, 0x3F}, // REF_CFG1
        {0x1A, 0x01}, // REF_CFG2
        {0x1C, 0x1F}, // STANDBY_ON1
        {0x1D, 0x00}, // STANDBY_ON2
        {0x2D, 0x07}, // PWR_ON
        {0x2E, 0x18}, // PWR_ISO
        {0x39, 0x00}, // PPGMOD_CFG3
        {0x3A, 0x80}, // PPGMOD1_CFG1
        {0x3B, 0x44}, // PPGMOD1_CFG2 - 8uA range
        {0x3C, 0x08}, // PPGMOD1_CFG3
        {0x46, 0x10}, // LED1_ICTRL - moderate current
        {0x4E, 0x01}, // LED_IRNG1
        {0x50, 0x01}, // LED_SUB1
        {0x5A, 0x01}, // PPG1_PDSEL1
        {0x6F, 0x11}, // PPG_SINC_CFGA
        {0x70, 0x01}, // PPG_SINC_CFGB
        {0x71, 0x00}, // PPG_SINC_CFGC
        {0x95, 0x01}, // IRQ_ENABLE
        {0x96, 0x20}, // PPG_SUB_WAIT
        {0x98, 0x10}, // PPG_LED_INIT
        {0x99, 0x4F}, // PPG_FREQL - faster sampling (80ms periods)
        {0x9A, 0x00}, // PPG_FREQH
        {0x9B, 0x01}, // PPG1_SUB_EN
        {0x9D, 0x00}, // PPG_MODE1
        {0xB1, 0x00}, // SAMPLE_NUM - continuous
        {0xCA, 0x10}, // FIFO_THRESHOLD - 16 samples
        {0xCB, 0x02}, // FIFO_CTRL
    };
    
    for (int i = 0; i < ARRAY_SIZE(config); i++) {
        write_reg(config[i].reg, config[i].val);
        k_usleep(2000); // 2ms delays for faster initialization
    }
    
    k_msleep(100);
    LOG_INF("AS7058 optimized configuration complete");
    
    // Initialize statistics
    ppg_stats.min_value = UINT32_MAX;
    ppg_stats.max_value = 0;
    ppg_stats.sum_value = 0;
    ppg_stats.total_samples = 0;
}

void start_ppg_measurement(void) {
    write_reg(REG_SEQ_START, 0x00);
    k_msleep(50);
    write_reg(REG_SEQ_START, 0x01);
    LOG_INF("PPG measurement started");
}

bool read_ppg_sample(ppg_sample_t *sample) {
    uint8_t fifol, fifom, fifoh;
    
    if (read_reg(REG_FIFOL, &fifol) != 0) return false;
    if (read_reg(REG_FIFOM, &fifom) != 0) return false;
    if (read_reg(REG_FIFOH, &fifoh) != 0) return false;
    
    // Parse FIFO data format
    uint32_t fifo_raw = (fifoh << 16) | (fifom << 8) | fifol;
    sample->adc_value = (fifo_raw >> 4) & 0xFFFFF; // 20-bit ADC data
    sample->data_marker = fifol & 0x07;
    sample->block_frame = (fifol >> 3) & 0x01;
    sample->timestamp_ms = k_uptime_get_32();
    
    // Basic quality assessment
    sample->sample_quality = (sample->adc_value > 100000 && sample->adc_value < 900000) ? 1 : 0;
    
    return true;
}

void update_ppg_statistics(const ppg_sample_t *sample) {
    ppg_stats.total_samples++;
    ppg_stats.sum_value += sample->adc_value;
    
    if (sample->adc_value < ppg_stats.min_value) {
        ppg_stats.min_value = sample->adc_value;
    }
    if (sample->adc_value > ppg_stats.max_value) {
        ppg_stats.max_value = sample->adc_value;
    }
    
    ppg_stats.range = ppg_stats.max_value - ppg_stats.min_value;
    ppg_stats.avg_value = (uint32_t)(ppg_stats.sum_value / ppg_stats.total_samples);
    
    // Keep last 10 samples for trend analysis
    ppg_stats.last_10_samples[ppg_stats.last_sample_index] = sample->adc_value;
    ppg_stats.last_sample_index = (ppg_stats.last_sample_index + 1) % 10;
}

void print_ppg_sample(const ppg_sample_t *sample, uint32_t sample_number) {
    LOG_INF("PPG[%04d]: ADC=%06d, Marker=%d, Frame=%d, Time=%dms, Quality=%s",
            sample_number,
            sample->adc_value,
            sample->data_marker,
            sample->block_frame,
            sample->timestamp_ms,
            sample->sample_quality ? "Good" : "Poor");
}

void print_ppg_statistics(void) {
    if (ppg_stats.total_samples == 0) return;
    
    LOG_INF("=== PPG STATISTICS ===");
    LOG_INF("Total samples: %d", ppg_stats.total_samples);
    LOG_INF("Min ADC: %d", ppg_stats.min_value);
    LOG_INF("Max ADC: %d", ppg_stats.max_value);
    LOG_INF("Avg ADC: %d", ppg_stats.avg_value);
    LOG_INF("Range: %d", ppg_stats.range);
    
    // Calculate recent trend from last 10 samples
    if (ppg_stats.total_samples >= 10) {
        uint32_t recent_sum = 0;
        for (int i = 0; i < 10; i++) {
            recent_sum += ppg_stats.last_10_samples[i];
        }
        uint32_t recent_avg = recent_sum / 10;
        LOG_INF("Recent 10-sample avg: %d", recent_avg);
        
        // Simple trend detection
        int trend_up = 0, trend_down = 0;
        for (int i = 1; i < 10; i++) {
            int prev_idx = (ppg_stats.last_sample_index + 10 - 10 + i - 1) % 10;
            int curr_idx = (ppg_stats.last_sample_index + 10 - 10 + i) % 10;
            if (ppg_stats.last_10_samples[curr_idx] > ppg_stats.last_10_samples[prev_idx]) {
                trend_up++;
            } else if (ppg_stats.last_10_samples[curr_idx] < ppg_stats.last_10_samples[prev_idx]) {
                trend_down++;
            }
        }
        
        if (trend_up > trend_down + 2) {
            LOG_INF("Trend: RISING");
        } else if (trend_down > trend_up + 2) {
            LOG_INF("Trend: FALLING");
        } else {
            LOG_INF("Trend: STABLE");
        }
    }
}

void continuous_ppg_processing(void) {
    LOG_INF("=== CONTINUOUS PPG DATA PROCESSING ===");
    
    uint32_t sample_count = 0;
    uint32_t batch_count = 0;
    uint32_t last_stats_time = k_uptime_get_32();
    
    while (true) {
        // Check FIFO level
        uint8_t fifo_l0, fifo_l1;
        if (read_reg(REG_FIFO_LEVEL0, &fifo_l0) != 0 || 
            read_reg(REG_FIFO_LEVEL1, &fifo_l1) != 0) {
            k_msleep(100);
            continue;
        }
        
        uint16_t fifo_level = ((fifo_l1 & 0x03) << 8) | fifo_l0;
        
        if (fifo_level > 0) {
            // Process available samples
            int samples_to_read = MIN(fifo_level, 20); // Process in batches of 20
            
            for (int i = 0; i < samples_to_read; i++) {
                ppg_sample_t sample;
                if (read_ppg_sample(&sample)) {
                    sample_count++;
                    update_ppg_statistics(&sample);
                    
                    // Print every 10th sample for readability
                    if (sample_count % 10 == 0) {
                        print_ppg_sample(&sample, sample_count);
                    }
                }
            }
            
            batch_count++;
            
            // Print statistics every 5 seconds
            uint32_t current_time = k_uptime_get_32();
            if (current_time - last_stats_time >= 5000) {
                LOG_INF("--- Batch %d processed (%d samples total) ---", 
                        batch_count, sample_count);
                print_ppg_statistics();
                last_stats_time = current_time;
            }
        } else {
            // No data available, restart sequence if needed
            uint8_t seq_status;
            read_reg(REG_STATUS_SEQ, &seq_status);
            if (seq_status == 0x00) {
                write_reg(REG_SEQ_START, 0x01);
            }
        }
        
        // Adaptive sleep based on FIFO level
        if (fifo_level > 100) {
            k_msleep(50);  // Fast processing when FIFO is full
        } else if (fifo_level > 10) {
            k_msleep(100); // Medium processing
        } else {
            k_msleep(200); // Slower when little data
        }
        
        // Safety exit after collecting substantial data
        if (sample_count >= 1000) {
            LOG_INF("Collected %d samples - stopping for analysis", sample_count);
            break;
        }
    }
    
    // Final statistics
    LOG_INF("=== FINAL PPG DATA SUMMARY ===");
    print_ppg_statistics();
    LOG_INF("Data collection rate: %.2f samples/second", 
            (float)sample_count / (k_uptime_get_32() / 1000.0f));
}

int main(void) {
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return 1;
    }

    LOG_INF("AS7058 PPG Data Processing System");
    LOG_INF("==================================");
    
    // Initialize AS7058 with optimized settings
    initialize_as7058_optimized();
    
    // Start PPG measurement
    start_ppg_measurement();
    
    // Wait for system to stabilize
    k_msleep(1000);
    
    // Begin continuous data processing
    continuous_ppg_processing();
    
    LOG_INF("PPG data processing complete");
    return 0;
}