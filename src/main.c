// AS7058 HRM System - Zephyr RTOS Port
// Integrates PPG data collection with HRM algorithm processing
// Based on OSRAM sample code adapted for embedded Zephyr environment

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>

// AS7058 chiplib includes (assuming these are available in your environment)
#include "as7058_bioz_measurement.h"
#include "as7058_extract.h"
#include "as7058_interface.h"
#include "as7058_osal_chiplib.h"
#include "as7058_typedefs.h"
#include "as7058_chiplib.h"
#include "as7058a_hrm_b0.h"
#include "vital_signs_accelerometer.h"
#include "error_codes.h"
#include "std_inc.h"

LOG_MODULE_REGISTER(as7058_hrm_system, LOG_LEVEL_INF);

// Device configuration
const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));
#define AS7058_NODE                   DT_ALIAS(as7058interrupt)

#define LISDH12_NODE                   DT_ALIAS(lis2dh12interrupt)  


struct gpio_dt_spec as7058_sensor_spec = GPIO_DT_SPEC_GET(AS7058_NODE, gpios); // Blue LED spec
struct gpio_dt_spec lisdh12_sensor_spec = GPIO_DT_SPEC_GET(LISDH12_NODE, gpios); // Red LED spec


#define AS7058_I2C_ADDR 0x55
#define LIS12DH_I2C_ADDR 0x19

// Accelerometer configuration
#define ACC_SAMPLE_PERIOD_US 40000
#define MAX_ACC_SAMPLES 20

// System state flags
static volatile bool g_ready_for_execution = false;
static volatile bool g_keep_running = true;
static volatile bool g_measurement_active = false;

// Data buffers and synchronization
static K_SEM_DEFINE(hrm_data_sem, 0, 1);
static K_MUTEX_DEFINE(system_mutex);

// Ring buffers for data storage
#define PPG_BUFFER_SIZE 1024
#define ACC_BUFFER_SIZE 512
static uint8_t ppg_buffer_data[PPG_BUFFER_SIZE];
static uint8_t acc_buffer_data[ACC_BUFFER_SIZE];
static struct ring_buf ppg_ring_buf;
static struct ring_buf acc_ring_buf;

// Thread stacks and priorities
#define HRM_THREAD_STACK_SIZE 4096
#define ACC_THREAD_STACK_SIZE 2048
#define PPG_THREAD_STACK_SIZE 2048

K_THREAD_STACK_DEFINE(hrm_thread_stack, HRM_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(acc_thread_stack, ACC_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(ppg_thread_stack, PPG_THREAD_STACK_SIZE);

static struct k_thread hrm_thread_data;
static struct k_thread acc_thread_data;
static struct k_thread ppg_thread_data;

// HRM output statistics
typedef struct {
    uint32_t total_outputs;
    uint32_t valid_hr_count;
    uint32_t hr_sum;
    uint16_t last_hr_bpm;
    uint8_t last_quality;
    uint16_t min_hr;
    uint16_t max_hr;
    uint16_t avg_hr;
} hrm_stats_t;

static hrm_stats_t hrm_stats = {0};

// Essential Register I/O functions
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

// AS7058 chiplib callback - called when PPG data is available
static void as7058_data_callback(err_code_t error, const uint8_t *p_fifo_data, uint16_t fifo_data_size,
                                const agc_status_t *p_agc_statuses, uint8_t agc_statuses_num,
                                as7058_status_events_t sensor_events, const void *p_cb_param)
{
    if (error != ERR_SUCCESS) {
        LOG_ERR("AS7058 callback error: %d", error);
        return;
    }

    if (!g_measurement_active) {
        return;
    }

    // Store PPG data in ring buffer
    if (p_fifo_data && fifo_data_size > 0) {
        uint32_t bytes_written = ring_buf_put(&ppg_ring_buf, p_fifo_data, fifo_data_size);
        if (bytes_written != fifo_data_size) {
            LOG_WRN("PPG buffer overflow: wrote %d of %d bytes", bytes_written, fifo_data_size);
        }
    }

    // Get accelerometer data
    vs_acc_data_t acc_data[MAX_ACC_SAMPLES];
    uint8_t num_acc_data = MAX_ACC_SAMPLES;
    err_code_t acc_result = vs_acc_get_data(acc_data, &num_acc_data);
    
    if (acc_result == ERR_SUCCESS && num_acc_data > 0) {
        // Store accelerometer data
        uint32_t acc_bytes = num_acc_data * sizeof(vs_acc_data_t);
        uint32_t acc_written = ring_buf_put(&acc_ring_buf, (uint8_t*)acc_data, acc_bytes);
        if (acc_written != acc_bytes) {
            LOG_WRN("ACC buffer overflow: wrote %d of %d bytes", acc_written, acc_bytes);
        }
    }

    // Pass data to HRM library
    uint8_t ready_for_execution = 0;
    err_code_t hrm_result = as7058a_hrm_b0_set_input(p_fifo_data, fifo_data_size, sensor_events, 
                                                     p_agc_statuses, agc_statuses_num,
                                                     acc_data, num_acc_data, &ready_for_execution);
    
    if (hrm_result != ERR_SUCCESS) {
        LOG_ERR("HRM set_input error: %d", hrm_result);
        return;
    }

    if (ready_for_execution) {
        g_ready_for_execution = true;
        k_sem_give(&hrm_data_sem);
    }
}

// Initialize AS7058 with optimized HRM configuration
void initialize_as7058_hrm(void) {
    LOG_INF("=== AS7058 HRM SYSTEM INITIALIZATION ===");
    
    // Basic chip reset and VCSEL safety disable
    write_reg(0xEF, 0x01); // CHIP_CTRL reset
    k_msleep(500);
    
    write_reg(0x40, 0x57); // VCSEL_PASSWORD
    write_reg(0x41, 0x00); // VCSEL_CFG
    write_reg(0x42, 0x00); // VCSEL_MODE
    write_reg(0x43, 0x00); // LED_CFG
    k_msleep(100);

    // Configure register groups using chiplib functions
    
    // POWER configuration
    const as7058_reg_group_power_t power_config = {{
        .pwr_on = 7, .pwr_iso = 0, .clk_cfg = 7, .ref_cfg1 = 63, .ref_cfg2 = 0,
        .ref_cfg3 = 0, .standby_on1 = 0, .standby_on2 = 0, .standby_en1 = 4,
        .standby_en2 = 2, .standby_en3 = 4, .standby_en4 = 0, .standby_en5 = 3,
        .standby_en6 = 16, .standby_en7 = 16, .standby_en8 = 4, .standby_en9 = 0,
        .standby_en10 = 3, .standby_en11 = 16, .standby_en12 = 16, 
        .standby_en13 = 16, .standby_en14 = 16,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_PWR, power_config.reg_buffer, 
                        sizeof(as7058_reg_group_power_t));

    // CONTROL configuration
    const as7058_reg_group_control_t control_config = {{
        .i2c_mode = 0, .int_cfg = 0, .if_cfg = 72, .gpio_cfg1 = 0, 
        .gpio_cfg2 = 0, .io_cfg = 0,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_CTRL, control_config.reg_buffer,
                        sizeof(as7058_reg_group_control_t));

    // LED configuration for HRM
    const as7058_reg_group_led_t led_config = {{
        .vcsel_password = 87, .vcsel_cfg = 0, .vcsel_mode = 0, .led_cfg = 0,
        .led_drv1 = 0, .led_drv2 = 0, .led1_ictrl = 9, .led2_ictrl = 9,
        .led3_ictrl = 9, .led4_ictrl = 9, .led5_ictrl = 0, .led6_ictrl = 0,
        .led7_ictrl = 0, .led8_ictrl = 0, .led_irng1 = 21, .led_irng2 = 0,
        .led_sub1 = 1, .led_sub2 = 1, .led_sub3 = 1, .led_sub4 = 1,
        .led_sub5 = 0, .led_sub6 = 0, .led_sub7 = 0, .led_sub8 = 0,
        .lowvds_wait = 0,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_LED, led_config.reg_buffer,
                        sizeof(as7058_reg_group_led_t));

    // PD (Photodiode) configuration
    const as7058_reg_group_pd_t pd_config = {{
        .pdsel_cfg = 0, .ppg1_pdsel1 = 2, .ppg1_pdsel2 = 2, .ppg1_pdsel3 = 0,
        .ppg1_pdsel4 = 0, .ppg1_pdsel5 = 0, .ppg1_pdsel6 = 0, .ppg1_pdsel7 = 0,
        .ppg1_pdsel8 = 0, .ppg2_pdsel1 = 0, .ppg2_pdsel2 = 0, .ppg2_pdsel3 = 0,
        .ppg2_pdsel4 = 0, .ppg2_pdsel5 = 0, .ppg2_pdsel6 = 0, .ppg2_pdsel7 = 0,
        .ppg2_pdsel8 = 0, .ppg2_afesel1 = 0, .ppg2_afesel2 = 0, .ppg2_afesel3 = 0,
        .ppg2_afesel4 = 0, .ppg2_afeen = 0,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_PD, pd_config.reg_buffer,
                        sizeof(as7058_reg_group_pd_t));

    // PPG modulator configuration
    const as7058_reg_group_ppg_t ppg_config = {{
        .ppgmod_cfg1 = 0, .ppgmod_cfg2 = 0, .ppgmod_cfg3 = 0,
        .ppgmod1_cfg1 = 167, .ppgmod1_cfg2 = 100, .ppgmod1_cfg3 = 3,
        .ppgmod2_cfg1 = 7, .ppgmod2_cfg2 = 87, .ppgmod2_cfg3 = 7,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_PPG, ppg_config.reg_buffer,
                        sizeof(as7058_reg_group_ppg_t));

    // SINC filter configuration
    const as7058_reg_group_sinc_t sinc_config = {{
        .ppg_sinc_cfga = 3, .ppg_sinc_cfgb = 3, .ppg_sinc_cfgc = 0, .ppg_sinc_cfgd = 0,
        .ecg1_sinc_cfga = 0, .ecg1_sinc_cfgb = 0, .ecg1_sinc_cfgc = 0,
        .ecg2_sinc_cfga = 0, .ecg2_sinc_cfgb = 0, .ecg2_sinc_cfgc = 0,
        .ecg_sinc_cfg = 0,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_SINC, sinc_config.reg_buffer,
                        sizeof(as7058_reg_group_sinc_t));

    // Sequencer configuration for HRM
    const as7058_reg_group_seq_t seq_config = {{
        .irq_enable = 7, .ppg_sub_wait = 0, .ppg_sar_wait = 0, .ppg_led_init = 10,
        .ppg_freql = 255, .ppg_freqh = 4, .ppg1_sub_en = 3, .ppg2_sub_en = 0,
        .ppg_mode_1 = 0, .ppg_mode_2 = 0, .ppg_mode_3 = 0, .ppg_mode_4 = 0,
        .ppg_mode_5 = 0, .ppg_mode_6 = 0, .ppg_mode_7 = 0, .ppg_mode_8 = 0,
        .ppg_cfg = 0, .ecg_freql = 79, .ecg_freqh = 0, .ecg1_freqdivl = 0,
        .ecg1_freqdivh = 0, .ecg2_freqdivl = 0, .ecg2_freqdivh = 0, .ecg_subs = 0,
        .leadoff_initl = 0, .leadoff_inith = 0, .ecg_initl = 1, .ecg_inith = 0,
        .sample_num = 0,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_SEQ, seq_config.reg_buffer,
                        sizeof(as7058_reg_group_seq_t));

    // FIFO configuration
    const as7058_reg_group_fifo_t fifo_config = {{
        .fifo_threshold = 5, .fifo_ctrl = 0,
    }};
    as7058_set_reg_group(AS7058_REG_GROUP_ID_FIFO, fifo_config.reg_buffer,
                        sizeof(as7058_reg_group_fifo_t));

    // Configure AGC for HRM
    const agc_configuration_t agc_config = {
        .mode = AGC_MODE_DEFAULT,
        .led_control_mode = AGC_AMPL_CNTL_MODE_AUTO,
        .channel = AS7058_SUB_SAMPLE_ID_PPG1_SUB1,
        .led_current_min = 5,
        .led_current_max = 30,
        .rel_amplitude_min_x100 = 5,
        .rel_amplitude_max_x100 = 25,
        .rel_amplitude_motion_x100 = 50,
        .num_led_steps = 6,
        .threshold_min = 250000,
        .threshold_max = 770000,
    };
    as7058_set_agc_config(&agc_config, 1);

    LOG_INF("AS7058 HRM configuration complete");
}

// Initialize system components
err_code_t initialize_hrm_system(void) {
    LOG_INF("Initializing HRM system components...");

    // Initialize ring buffers
    ring_buf_init(&ppg_ring_buf, sizeof(ppg_buffer_data), ppg_buffer_data);
    ring_buf_init(&acc_ring_buf, sizeof(acc_buffer_data), acc_buffer_data);

    // Initialize AS7058 chiplib
    err_code_t result = as7058_initialize(as7058_data_callback, NULL, NULL, NULL);
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058_initialize failed: %d", result);
        return result;
    }

    // Initialize accelerometer driver
    result = vs_acc_initialize(NULL);
    if (result != ERR_SUCCESS) {
        LOG_ERR("vs_acc_initialize failed: %d", result);
        return result;
    }

    // Initialize HRM library
    result = as7058a_hrm_b0_initialize();
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058a_hrm_b0_initialize failed: %d", result);
        return result;
    }

    // Configure accelerometer sample period
    result = vs_acc_set_sample_period(ACC_SAMPLE_PERIOD_US);
    if (result != ERR_SUCCESS) {
        LOG_ERR("vs_acc_set_sample_period failed: %d", result);
        return result;
    }

    // Set HRM signal routing (green PPG and ambient light)
    result = as7058a_hrm_b0_set_signal_routing(AS7058_SUB_SAMPLE_ID_PPG1_SUB1, 
                                              AS7058_SUB_SAMPLE_ID_PPG1_SUB2);
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058a_hrm_b0_set_signal_routing failed: %d", result);
        return result;
    }

    LOG_INF("HRM system initialization complete");
    return ERR_SUCCESS;
}

// Start HRM measurement
err_code_t start_hrm_measurement(void) {
    LOG_INF("Starting HRM measurement...");

    // Get measurement configuration
    as7058_meas_config_t meas_config;
    err_code_t result = as7058_get_measurement_config(&meas_config);
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058_get_measurement_config failed: %d", result);
        return result;
    }

    // Start HRM processing session
    result = as7058a_hrm_b0_start_processing(meas_config, ACC_SAMPLE_PERIOD_US);
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058a_hrm_b0_start_processing failed: %d", result);
        return result;
    }

    // Start accelerometer
    result = vs_acc_start();
    if (result != ERR_SUCCESS) {
        LOG_ERR("vs_acc_start failed: %d", result);
        return result;
    }

    // Start AS7058 measurement
    result = as7058_start_measurement(AS7058_MEAS_MODE_NORMAL);
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058_start_measurement failed: %d", result);
        return result;
    }

    g_measurement_active = true;
    LOG_INF("HRM measurement started successfully");
    return ERR_SUCCESS;
}

// Stop HRM measurement
err_code_t stop_hrm_measurement(void) {
    LOG_INF("Stopping HRM measurement...");
    
    g_measurement_active = false;
    
    // Stop AS7058 measurement
    err_code_t result = as7058_stop_measurement();
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058_stop_measurement failed: %d", result);
    }

    // Stop accelerometer
    result = vs_acc_stop();
    if (result != ERR_SUCCESS) {
        LOG_ERR("vs_acc_stop failed: %d", result);
    }

    // Stop HRM processing
    result = as7058a_hrm_b0_stop_processing();
    if (result != ERR_SUCCESS) {
        LOG_ERR("as7058a_hrm_b0_stop_processing failed: %d", result);
    }

    LOG_INF("HRM measurement stopped");
    return ERR_SUCCESS;
}

// Update HRM statistics
void update_hrm_stats(const bio_hrm_b0_output_t *hrm_output) {
    hrm_stats.total_outputs++;
    hrm_stats.last_hr_bpm = hrm_output->heart_rate / 10; // Convert from 0.1 bpm to bpm
    hrm_stats.last_quality = hrm_output->quality;

    if (hrm_output->heart_rate > 0) {
        hrm_stats.valid_hr_count++;
        hrm_stats.hr_sum += hrm_stats.last_hr_bpm;
        
        if (hrm_stats.min_hr == 0 || hrm_stats.last_hr_bpm < hrm_stats.min_hr) {
            hrm_stats.min_hr = hrm_stats.last_hr_bpm;
        }
        if (hrm_stats.last_hr_bpm > hrm_stats.max_hr) {
            hrm_stats.max_hr = hrm_stats.last_hr_bpm;
        }
        
        hrm_stats.avg_hr = hrm_stats.hr_sum / hrm_stats.valid_hr_count;
    }
}

// Print HRM statistics
void print_hrm_stats(void) {
    LOG_INF("=== HRM STATISTICS ===");
    LOG_INF("Total outputs: %d", hrm_stats.total_outputs);
    LOG_INF("Valid HR readings: %d", hrm_stats.valid_hr_count);
    LOG_INF("Last HR: %d bpm (quality: %d)", hrm_stats.last_hr_bpm, hrm_stats.last_quality);
    if (hrm_stats.valid_hr_count > 0) {
        LOG_INF("HR range: %d - %d bpm", hrm_stats.min_hr, hrm_stats.max_hr);
        LOG_INF("Average HR: %d bpm", hrm_stats.avg_hr);
    }
}

// HRM processing thread
void hrm_processing_thread(void *p1, void *p2, void *p3) {
    LOG_INF("HRM processing thread started");
    uint32_t output_counter = 0;

    while (g_keep_running) {
        // Wait for HRM data ready signal
        if (k_sem_take(&hrm_data_sem, K_MSEC(500)) == 0) {
            if (g_ready_for_execution && g_measurement_active) {
                // Execute HRM algorithm
                err_code_t result = as7058a_hrm_b0_execute();
                
                if (result == ERR_SUCCESS) {
                    // Get HRM output
                    bio_hrm_b0_output_t hrm_output;
                    result = as7058a_hrm_b0_get_output(&hrm_output);
                    
                    if (result == ERR_SUCCESS) {
                        update_hrm_stats(&hrm_output);
                        
                        LOG_INF("[%d] HR: %d bpm, Quality: %d", 
                               output_counter++, 
                               hrm_output.heart_rate / 10, 
                               hrm_output.quality);
                        
                        // Print stats every 10 outputs
                        if (output_counter % 10 == 0) {
                            print_hrm_stats();
                        }
                    }
                } else if (result != ERR_NO_DATA) {
                    LOG_ERR("HRM execute error: %d", result);
                }
                
                g_ready_for_execution = false;
            }
        }
    }
    
    LOG_INF("HRM processing thread stopped");
}

// Accelerometer monitoring thread
void acc_monitoring_thread(void *p1, void *p2, void *p3) {
    LOG_INF("Accelerometer monitoring thread started");
    
    while (g_keep_running) {
        if (g_measurement_active && i2c_dev && device_is_ready(i2c_dev)) {
            struct sensor_value accel[3];
            int ret = sensor_sample_fetch(i2c_dev);
            if (ret == 0) {
                sensor_channel_get(i2c_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
                
                // Log accelerometer data periodically
                static uint32_t acc_log_counter = 0;
                if (++acc_log_counter % 50 == 0) {  // Log every 50th sample
                    LOG_INF("ACC: X=%.2f, Y=%.2f, Z=%.2f", 
                           sensor_value_to_double(&accel[0]),
                           sensor_value_to_double(&accel[1]),
                           sensor_value_to_double(&accel[2]));
                }
            }
        }
        
        k_msleep(ACC_SAMPLE_PERIOD_US / 1000); // Convert µs to ms
    }
    
    LOG_INF("Accelerometer monitoring thread stopped");
}

// PPG data monitoring thread
void ppg_monitoring_thread(void *p1, void *p2, void *p3) {
    LOG_INF("PPG monitoring thread started");
    uint8_t ppg_data[64];
    
    while (g_keep_running) {
        if (g_measurement_active) {
            // Check for PPG data in ring buffer
            uint32_t bytes_available = ring_buf_size_get(&ppg_ring_buf);
            if (bytes_available >= sizeof(ppg_data)) {
                uint32_t bytes_read = ring_buf_get(&ppg_ring_buf, ppg_data, sizeof(ppg_data));
                if (bytes_read > 0) {
                    static uint32_t ppg_log_counter = 0;
                    if (++ppg_log_counter % 20 == 0) {  // Log every 20th batch
                        LOG_INF("PPG data: %d bytes processed (buffer: %d/%d)", 
                               bytes_read, bytes_available, PPG_BUFFER_SIZE);
                    }
                }
            }
        }
        
        k_msleep(100);  // Check every 100ms
    }
    
    LOG_INF("PPG monitoring thread stopped");
}

// System shutdown
void shutdown_hrm_system(void) {
    LOG_INF("Shutting down HRM system...");
    
    g_keep_running = false;
    g_measurement_active = false;
    
    // Stop measurement
    stop_hrm_measurement();
    
    // Shutdown components
    as7058_shutdown();
    vs_acc_shutdown();
    as7058a_hrm_b0_shutdown();
    
    // Print final statistics
    print_hrm_stats();
    
    LOG_INF("HRM system shutdown complete");
}

// Main function
int main(void) {
    LOG_INF("AS7058 HRM System - Zephyr RTOS");
    LOG_INF("================================");
    
    // Check I2C device
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return -1;
    }
    
    // Initialize HRM system
    err_code_t result = initialize_hrm_system();
    if (result != ERR_SUCCESS) {
        LOG_ERR("HRM system initialization failed: %d", result);
        return -1;
    }
    
    // Configure AS7058 hardware
    initialize_as7058_hrm();
    
    // Create processing threads
    k_thread_create(&hrm_thread_data, hrm_thread_stack, HRM_THREAD_STACK_SIZE,
                   hrm_processing_thread, NULL, NULL, NULL, 
                   K_PRIO_COOP(5), 0, K_NO_WAIT);
    
    k_thread_create(&acc_thread_data, acc_thread_stack, ACC_THREAD_STACK_SIZE,
                   acc_monitoring_thread, NULL, NULL, NULL,
                   K_PRIO_COOP(7), 0, K_NO_WAIT);
    
    k_thread_create(&ppg_thread_data, ppg_thread_stack, PPG_THREAD_STACK_SIZE,
                   ppg_monitoring_thread, NULL, NULL, NULL,
                   K_PRIO_COOP(8), 0, K_NO_WAIT);
    
    // Start HRM measurement
    result = start_hrm_measurement();
    if (result != ERR_SUCCESS) {
        LOG_ERR("Failed to start HRM measurement: %d", result);
        shutdown_hrm_system();
        return -1;
    }
    
    LOG_INF("HRM system running... First output in ~10 seconds");
    
    // Main loop - monitor system for specified duration
    uint32_t measurement_duration_s = 300; // 5 minutes
    uint32_t start_time = k_uptime_get_32();
    uint32_t last_status_time = start_time;
    
    while (g_keep_running) {
        uint32_t current_time = k_uptime_get_32();
        uint32_t elapsed_time = (current_time - start_time) / 1000; // Convert to seconds
        
        // Check if measurement duration exceeded
        if (elapsed_time >= measurement_duration_s) {
            LOG_INF("Measurement duration (%d seconds) completed", measurement_duration_s);
            break;
        }
        
        // Print system status every 30 seconds
        if (current_time - last_status_time >= 30000) {
            LOG_INF("=== SYSTEM STATUS (Runtime: %d/%d seconds) ===", 
                   elapsed_time, measurement_duration_s);
            LOG_INF("Measurement active: %s", g_measurement_active ? "YES" : "NO");
            LOG_INF("PPG buffer usage: %d/%d bytes", 
                   ring_buf_size_get(&ppg_ring_buf), PPG_BUFFER_SIZE);
            LOG_INF("ACC buffer usage: %d/%d bytes", 
                   ring_buf_size_get(&acc_ring_buf), ACC_BUFFER_SIZE);
            
            // Print intermediate HRM stats
            if (hrm_stats.total_outputs > 0) {
                print_hrm_stats();
            } else {
                LOG_INF("Waiting for HRM algorithm to stabilize...");
            }
            
            last_status_time = current_time;
        }
        
        // Check system health
        if (!g_measurement_active) {
            LOG_WRN("Measurement stopped unexpectedly - attempting restart");
            start_hrm_measurement();
        }
        
        // Sleep for main loop
        k_msleep(5000); // Check every 5 seconds
    }
    
    // Shutdown system
    shutdown_hrm_system();
    
    // Wait for threads to complete
    k_thread_join(&hrm_thread_data, K_FOREVER);
    k_thread_join(&acc_thread_data, K_FOREVER);
    k_thread_join(&ppg_thread_data, K_FOREVER);
    
    LOG_INF("=== FINAL HRM MEASUREMENT SUMMARY ===");
    print_hrm_stats();
    LOG_INF("System runtime: %d seconds", (k_uptime_get_32() - start_time) / 1000);
    
    return 0;
}

// Additional utility functions for system management

// Signal handler equivalent for Zephyr (console command or button press)
void request_system_stop(void) {
    LOG_INF("Stop request received");
    g_keep_running = false;
}

// System health check function
bool check_system_health(void) {
    bool healthy = true;
    
    // Check I2C communication
    uint8_t test_reg;
    if (read_reg(0xFA, &test_reg) != 0) { // STATUS register
        LOG_ERR("I2C communication failed");
        healthy = false;
    }
    
    // Check buffer overflow conditions
    if (ring_buf_size_get(&ppg_ring_buf) > (PPG_BUFFER_SIZE * 0.9)) {
        LOG_WRN("PPG buffer near full");
        healthy = false;
    }
    
    if (ring_buf_size_get(&acc_ring_buf) > (ACC_BUFFER_SIZE * 0.9)) {
        LOG_WRN("ACC buffer near full");
        healthy = false;
    }
    
    return healthy;
}

// Emergency recovery function
void emergency_recovery(void) {
    LOG_WRN("Initiating emergency recovery");
    
    // Stop current measurement
    stop_hrm_measurement();
    k_msleep(1000);
    
    // Clear buffers
    ring_buf_reset(&ppg_ring_buf);
    ring_buf_reset(&acc_ring_buf);
    
    // Reset flags
    g_ready_for_execution = false;
    g_measurement_active = false;
    
    // Re-initialize hardware
    initialize_as7058_hrm();
    k_msleep(2000);
    
    // Restart measurement
    if (start_hrm_measurement() == ERR_SUCCESS) {
        LOG_INF("Emergency recovery successful");
    } else {
        LOG_ERR("Emergency recovery failed");
        g_keep_running = false;
    }
}

// Performance monitoring function
void log_performance_metrics(void) {
    static uint32_t last_metric_time = 0;
    static uint32_t last_hrm_outputs = 0;
    
    uint32_t current_time = k_uptime_get_32();
    if (current_time - last_metric_time >= 60000) { // Every minute
        uint32_t hrm_rate = (hrm_stats.total_outputs - last_hrm_outputs);
        
        LOG_INF("=== PERFORMANCE METRICS ===");
        LOG_INF("HRM output rate: %d outputs/minute", hrm_rate);
        LOG_INF("Data processing efficiency: %d%%", 
               g_measurement_active ? 
               (hrm_stats.valid_hr_count * 100 / MAX(hrm_stats.total_outputs, 1)) : 0);
        
        // Memory usage (approximate)
        LOG_INF("Memory usage - PPG: %d%%, ACC: %d%%",
               (ring_buf_size_get(&ppg_ring_buf) * 100 / PPG_BUFFER_SIZE),
               (ring_buf_size_get(&acc_ring_buf) * 100 / ACC_BUFFER_SIZE));
        
        last_metric_time = current_time;
        last_hrm_outputs = hrm_stats.total_outputs;
    }
}

// Console command handlers (if shell is enabled)
#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

static int cmd_hrm_start(const struct shell *sh, size_t argc, char **argv) {
    if (g_measurement_active) {
        shell_print(sh, "HRM measurement already active");
        return 0;
    }
    
    err_code_t result = start_hrm_measurement();
    shell_print(sh, "Start HRM: %s", (result == ERR_SUCCESS) ? "OK" : "FAILED");
    return 0;
}

static int cmd_hrm_stop(const struct shell *sh, size_t argc, char **argv) {
    if (!g_measurement_active) {
        shell_print(sh, "HRM measurement not active");
        return 0;
    }
    
    err_code_t result = stop_hrm_measurement();
    shell_print(sh, "Stop HRM: %s", (result == ERR_SUCCESS) ? "OK" : "FAILED");
    return 0;
}

static int cmd_hrm_status(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "=== HRM SYSTEM STATUS ===");
    shell_print(sh, "Measurement active: %s", g_measurement_active ? "YES" : "NO");
    shell_print(sh, "System running: %s", g_keep_running ? "YES" : "NO");
    shell_print(sh, "Ready for execution: %s", g_ready_for_execution ? "YES" : "NO");
    shell_print(sh, "Runtime: %d seconds", k_uptime_get_32() / 1000);
    
    if (hrm_stats.total_outputs > 0) {
        shell_print(sh, "Total HRM outputs: %d", hrm_stats.total_outputs);
        shell_print(sh, "Last HR: %d bpm (quality: %d)", 
                   hrm_stats.last_hr_bpm, hrm_stats.last_quality);
        if (hrm_stats.valid_hr_count > 0) {
            shell_print(sh, "Average HR: %d bpm", hrm_stats.avg_hr);
        }
    }
    
    return 0;
}

static int cmd_hrm_reset(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "Resetting HRM system...");
    emergency_recovery();
    return 0;
}

static int cmd_hrm_exit(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "Requesting system shutdown...");
    request_system_stop();
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(hrm_cmds,
    SHELL_CMD(start, NULL, "Start HRM measurement", cmd_hrm_start),
    SHELL_CMD(stop, NULL, "Stop HRM measurement", cmd_hrm_stop),
    SHELL_CMD(status, NULL, "Show HRM system status", cmd_hrm_status),
    SHELL_CMD(reset, NULL, "Reset HRM system", cmd_hrm_reset),
    SHELL_CMD(exit, NULL, "Exit HRM system", cmd_hrm_exit),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(hrm, &hrm_cmds, "HRM system commands", NULL);
#endif // CONFIG_SHELL