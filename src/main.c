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

#include <stdio.h>
#include <string.h>

// AS7058 chiplib includes
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





// Configuration constants
#define AS7058_I2C_ADDR 0x55
#define LIS12DH_I2C_ADDR 0x19
#define ACC_SAMPLE_PERIOD_US 20000  // 20ms
#define MAX_ACC_SAMPLES 20

// Global variables
static volatile uint8_t g_ready_for_execution = FALSE;
static volatile uint8_t g_keep_running = TRUE;

/* Callback function called by the AS7058 Chip Library when measurement data is available */
static void as7058_callback(err_code_t error, const uint8_t *p_fifo_data, uint16_t fifo_data_size,
                            const agc_status_t *p_agc_statuses, uint8_t agc_statuses_num,
                            as7058_status_events_t sensor_events, const void *p_cb_param)
{
    printk("as7058_callback called with %u bytes of data\n", fifo_data_size);
    M_UNUSED_PARAM(p_cb_param);

    if (error != ERR_SUCCESS) {
        printk("as7058_callback called with error %d\n", error);
        return;
    }

    // Poll accelerometer data
    vs_acc_data_t acc_data[20];
    uint8_t num_acc_data = sizeof(acc_data) / sizeof(acc_data[0]);
    err_code_t result = vs_acc_get_data(acc_data, &num_acc_data);
    if (result != ERR_SUCCESS) {
        printk("vs_acc_get_data returned error %d\n", result);
        return;
    }

    // Pass data to the HRM library
    uint8_t ready_for_execution = 0;
    result = as7058a_hrm_b0_set_input(p_fifo_data, fifo_data_size, sensor_events, p_agc_statuses, agc_statuses_num,
                                      acc_data, num_acc_data, &ready_for_execution);
    if (result != ERR_SUCCESS) {
        printk("as7058a_hrm_b0_set_input returned error %d\n", result);
        return;
    }

    if (ready_for_execution) {
        g_ready_for_execution = ready_for_execution;
        printk("HRM algorithm ready for execution\n");
    }
}

int main(void)
{
    printk("Starting AS7058 HRM System\n");
    
    // Initialize global variables
    g_keep_running = TRUE;
    g_ready_for_execution = FALSE;

    /**************************************************************************
     *                             INITIALIZATION                             *
     **************************************************************************/

    err_code_t result = as7058_initialize(as7058_callback, NULL, NULL, NULL);
    if (result != ERR_SUCCESS) {
        printk("as7058_initialize returned error %d\n", result);
        goto ERROR;
    }

    // Test basic I2C communication
    uint8_t chip_id = 0;
    result = as7058_read_register(AS7058_REGADDR_SILICON_ID, &chip_id);
    if (result == ERR_SUCCESS) {
        printk("AS7058 Silicon ID: 0x%02X (expected: 0x%02X)\n", chip_id, AS7058_SILICON_ID);
    } else {
        printk("Failed to read AS7058 Silicon ID, error: %d\n", result);
        goto ERROR;
    }

    // Register test
    uint8_t test_val = 0x55;
    result = as7058_write_register(AS7058_REGADDR_GPIO_CFG1, test_val);
    if (result == ERR_SUCCESS) {
        uint8_t read_val = 0;
        result = as7058_read_register(AS7058_REGADDR_GPIO_CFG1, &read_val);
        if (result == ERR_SUCCESS) {
            printk("Register test: wrote 0x%02X, read 0x%02X\n", test_val, read_val);
        }
    }

    // Initialize accelerometer
    result = vs_acc_initialize(NULL);
    if (result != ERR_SUCCESS) {
        printk("vs_acc_initialize returned error %d\n", result);
        goto ERROR;
    }

    // Initialize HRM library
    result = as7058a_hrm_b0_initialize();
    if (result != ERR_SUCCESS) {
        printk("as7058a_hrm_b0_initialize returned error %d\n", result);
        goto ERROR;
    }

    /**************************************************************************
     *                              CONFIGURATION                             *
     **************************************************************************/

    /* Configure register group POWER */
    const as7058_reg_group_power_t power_config = {{
        .pwr_on = 7,
        .pwr_iso = 0,
        .clk_cfg = 7,
        .ref_cfg1 = 63,
        .ref_cfg2 = 0,
        .ref_cfg3 = 0,
        .standby_on1 = 0,
        .standby_on2 = 0,
        .standby_en1 = 4,
        .standby_en2 = 2,
        .standby_en3 = 4,
        .standby_en4 = 0,
        .standby_en5 = 3,
        .standby_en6 = 16,
        .standby_en7 = 16,
        .standby_en8 = 4,
        .standby_en9 = 0,
        .standby_en10 = 3,
        .standby_en11 = 16,
        .standby_en12 = 16,
        .standby_en13 = 16,
        .standby_en14 = 16,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_PWR, power_config.reg_buffer, sizeof(as7058_reg_group_power_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_PWR returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group CONTROL */
    const as7058_reg_group_control_t control_config = {{
        .i2c_mode = 0,
        .int_cfg = 0,
        .if_cfg = 72,
        .gpio_cfg1 = 0,
        .gpio_cfg2 = 0,
        .io_cfg = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_CTRL, control_config.reg_buffer, sizeof(as7058_reg_group_control_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_CTRL returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group LED */
    const as7058_reg_group_led_t led_config = {{
        .vcsel_password = 87,
        .vcsel_cfg = 0,
        .vcsel_mode = 0,
        .led_cfg = 0,
        .led_drv1 = 0,
        .led_drv2 = 0,
        .led1_ictrl = 9,
        .led2_ictrl = 0,
        .led3_ictrl = 0,
        .led4_ictrl = 0,
        .led5_ictrl = 0,
        .led6_ictrl = 0,
        .led7_ictrl = 0,
        .led8_ictrl = 0,
        .led_irng1 = 21,
        .led_irng2 = 0,
        .led_sub1 = 1,
        .led_sub2 = 0,
        .led_sub3 = 0,
        .led_sub4 = 0,
        .led_sub5 = 0,
        .led_sub6 = 0,
        .led_sub7 = 0,
        .led_sub8 = 0,
        .lowvds_wait = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_LED, led_config.reg_buffer, sizeof(as7058_reg_group_led_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_LED returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group PD */
    const as7058_reg_group_pd_t pd_config = {{
        .pdsel_cfg = 0,
        .ppg1_pdsel1 = 2,
        .ppg1_pdsel2 = 2,
        .ppg1_pdsel3 = 0,
        .ppg1_pdsel4 = 0,
        .ppg1_pdsel5 = 0,
        .ppg1_pdsel6 = 0,
        .ppg1_pdsel7 = 0,
        .ppg1_pdsel8 = 0,
        .ppg2_pdsel1 = 0,
        .ppg2_pdsel2 = 0,
        .ppg2_pdsel3 = 0,
        .ppg2_pdsel4 = 0,
        .ppg2_pdsel5 = 0,
        .ppg2_pdsel6 = 0,
        .ppg2_pdsel7 = 0,
        .ppg2_pdsel8 = 0,
        .ppg2_afesel1 = 0,
        .ppg2_afesel2 = 0,
        .ppg2_afesel3 = 0,
        .ppg2_afesel4 = 0,
        .ppg2_afeen = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_PD, pd_config.reg_buffer, sizeof(as7058_reg_group_pd_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_PD returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group IOS */
    const as7058_reg_group_ios_t ios_config = {{
        .ios_ppg1_sub1 = 0,
        .ios_ppg1_sub2 = 0,
        .ios_ppg1_sub3 = 0,
        .ios_ppg1_sub4 = 0,
        .ios_ppg1_sub5 = 0,
        .ios_ppg1_sub6 = 0,
        .ios_ppg1_sub7 = 0,
        .ios_ppg1_sub8 = 0,
        .ios_ppg2_sub1 = 0,
        .ios_ppg2_sub2 = 0,
        .ios_ppg2_sub3 = 0,
        .ios_ppg2_sub4 = 0,
        .ios_ppg2_sub5 = 0,
        .ios_ppg2_sub6 = 0,
        .ios_ppg2_sub7 = 0,
        .ios_ppg2_sub8 = 0,
        .ios_ledoff = 0,
        .ios_cfg = 0,
        .aoc_sar_thres = 0,
        .aoc_sar_range = 0,
        .aoc_sar_ppg1 = 0,
        .aoc_sar_ppg2 = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_IOS, ios_config.reg_buffer, sizeof(as7058_reg_group_ios_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_IOS returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group PPG */
    const as7058_reg_group_ppg_t ppg_config = {{
        .ppgmod_cfg1 = 0,
        .ppgmod_cfg2 = 0,
        .ppgmod_cfg3 = 0,
        .ppgmod1_cfg1 = 167,
        .ppgmod1_cfg2 = 100,
        .ppgmod1_cfg3 = 3,
        .ppgmod2_cfg1 = 7,
        .ppgmod2_cfg2 = 87,
        .ppgmod2_cfg3 = 7,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_PPG, ppg_config.reg_buffer, sizeof(as7058_reg_group_ppg_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_PPG returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group ECG */
    const as7058_reg_group_ecg_t ecg_config = {{
        .bioz_cfg = 0,
        .bioz_excit = 0,
        .bioz_mixer = 0,
        .bioz_select = 0,
        .bioz_gain = 0,
        .ecgmod_cfg1 = 0,
        .ecgmod_cfg2 = 0,
        .ecgimux_cfg1 = 0,
        .ecgimux_cfg2 = 0,
        .ecgimux_cfg3 = 0,
        .ecgamp_cfg1 = 0,
        .ecgamp_cfg2 = 0,
        .ecgamp_cfg3 = 0,
        .ecgamp_cfg4 = 0,
        .ecgamp_cfg5 = 0,
        .ecgamp_cfg6 = 0,
        .ecgamp_cfg7 = 0,
        .ecg_bioz = 0,
        .leadoff_cfg = 0,
        .leadoff_thresl = 0,
        .leadoff_thresh = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_ECG, ecg_config.reg_buffer, sizeof(as7058_reg_group_ecg_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_ECG returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group SINC */
    const as7058_reg_group_sinc_t sinc_config = {{
        .ppg_sinc_cfga = 3,
        .ppg_sinc_cfgb = 3,
        .ppg_sinc_cfgc = 0,
        .ppg_sinc_cfgd = 0,
        .ecg1_sinc_cfga = 0,
        .ecg1_sinc_cfgb = 0,
        .ecg1_sinc_cfgc = 0,
        .ecg2_sinc_cfga = 0,
        .ecg2_sinc_cfgb = 0,
        .ecg2_sinc_cfgc = 0,
        .ecg_sinc_cfg = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_SINC, sinc_config.reg_buffer, sizeof(as7058_reg_group_sinc_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_SINC returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group SEQ - CRITICAL for FIFO mapping */
    const as7058_reg_group_seq_t seq_config = {{
        .irq_enable = 0x03,      // Enable FIFO threshold + overflow interrupts
        .ppg_sub_wait = 0,
        .ppg_sar_wait = 0,
        .ppg_led_init = 10,
        .ppg_freql = 255,
        .ppg_freqh = 4,
        .ppg1_sub_en = 0x03,     // Enable PPG1 sub-samples 1 and 2 (bits 0,1)
        .ppg2_sub_en = 0x00,     // Disable PPG2
        .ppg_mode_1 = 0,
        .ppg_mode_2 = 0,
        .ppg_mode_3 = 0,
        .ppg_mode_4 = 0,
        .ppg_mode_5 = 0,
        .ppg_mode_6 = 0,
        .ppg_mode_7 = 0,
        .ppg_mode_8 = 0,
        .ppg_cfg = 0,
        .ecg_freql = 79,
        .ecg_freqh = 0,
        .ecg1_freqdivl = 0,
        .ecg1_freqdivh = 0,
        .ecg2_freqdivl = 0,
        .ecg2_freqdivh = 0,
        .ecg_subs = 0,
        .leadoff_initl = 0,
        .leadoff_inith = 0,
        .ecg_initl = 1,
        .ecg_inith = 0,
        .sample_num = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_SEQ, seq_config.reg_buffer, sizeof(as7058_reg_group_seq_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_SEQ returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group PP */
    const as7058_reg_group_pp_t pp_config = {{
        .pp_cfg = 0,
        .ppg1_pp1 = 0,
        .ppg1_pp2 = 0,
        .ppg2_pp1 = 0,
        .ppg2_pp2 = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_PP, pp_config.reg_buffer, sizeof(as7058_reg_group_pp_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_PP returned error %d\n", result);
        goto ERROR;
    }

    /* Configure register group FIFO */
    const as7058_reg_group_fifo_t fifo_config = {{
        .fifo_threshold = 5,
        .fifo_ctrl = 0,
    }};
    result = as7058_set_reg_group(AS7058_REG_GROUP_ID_FIFO, fifo_config.reg_buffer, sizeof(as7058_reg_group_fifo_t));
    if (result != ERR_SUCCESS) {
        printk("Writing register group AS7058_REG_GROUP_ID_FIFO returned error %d\n", result);
        goto ERROR;
    }

    // Verify configuration was applied
    uint16_t fifo_threshold;
    result = as7058_ifce_get_fifo_threshold(&fifo_threshold);
    if (result == ERR_SUCCESS) {
        printk("FIFO threshold configured to: %u\n", fifo_threshold);
    }

    as7058_interrupt_t enabled_irqs;
    result = as7058_ifce_get_interrupt_enable(&enabled_irqs);
    if (result == ERR_SUCCESS) {
        printk("Enabled IRQs - FIFO threshold: %u, FIFO overflow: %u\n", 
               enabled_irqs.fifo_threshold, enabled_irqs.fifo_overflow);
    }

    // Configure AGC
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
    result = as7058_set_agc_config(&agc_config, 1);
    if (result != ERR_SUCCESS) {
        printk("as7058_set_agc_config returned error %d\n", result);
        goto ERROR;
    }

    // Configure accelerometer
    result = vs_acc_set_sample_period(ACC_SAMPLE_PERIOD_US);
    if (result != ERR_SUCCESS) {
        printk("vs_acc_set_sample_period returned error %d\n", result);
        goto ERROR;
    }

    // Set signal routing for HRM
    result = as7058a_hrm_b0_set_signal_routing(AS7058_SUB_SAMPLE_ID_PPG1_SUB1, AS7058_SUB_SAMPLE_ID_PPG1_SUB2);
    if (result != ERR_SUCCESS) {
        printk("as7058a_hrm_b0_set_signal_routing returned error %d\n", result);
        goto ERROR;
    }

    /**************************************************************************
     *                            START MEASUREMENT                           *
     **************************************************************************/

    // Get measurement configuration
    as7058_meas_config_t meas_config;
    result = as7058_get_measurement_config(&meas_config);
    if (result != ERR_SUCCESS) {
        printk("as7058_get_measurement_config returned error %d\n", result);
        goto ERROR;
    }

    // Debug measurement configuration
    printk("=== Measurement Configuration ===\n");
    printk("PPG sample period: %u us\n", meas_config.ppg_sample_period_us);
    printk("FIFO map: 0x%08X\n", meas_config.fifo_map);
    printk("SAR map: 0x%08X\n", meas_config.sar_map);
    printk("================================\n");

    // Check if FIFO map is valid
    if (meas_config.fifo_map == 0) {
        printk("ERROR: FIFO map is empty - no sub-samples enabled!\n");
        goto ERROR;
    }

    // Start HRM processing
    result = as7058a_hrm_b0_start_processing(meas_config, ACC_SAMPLE_PERIOD_US);
    if (result != ERR_SUCCESS) {
        printk("as7058a_hrm_b0_start_processing returned error %d\n", result);
        goto ERROR;
    }

    // Start accelerometer
    result = vs_acc_start();
    if (result != ERR_SUCCESS) {
        printk("vs_acc_start returned error %d\n", result);
        goto ERROR;
    }

    // Test accelerometer
    vs_acc_data_t test_acc_data[5];
    uint8_t num_test_acc = 5;
    result = vs_acc_get_data(test_acc_data, &num_test_acc);
    printk("Accelerometer test: got %u samples, result: %d\n", num_test_acc, result);

    // Check state before starting measurement
    uint8_t lib_state, is_meas_running;
    as7058_get_debug_state(&lib_state, &is_meas_running);
    printk("About to start measurement. Current state: lib_state=%d, is_meas_running=%d\n", 
           lib_state, is_meas_running);

    // Start AS7058 measurement
    result = as7058_start_measurement(AS7058_MEAS_MODE_NORMAL);
    if (result != ERR_SUCCESS) {
        printk("as7058_start_measurement failed: %d\n", result);
        goto ERROR;
    }

    // Check state after starting
    as7058_get_debug_state(&lib_state, &is_meas_running);
    printk("Measurement started. New state: lib_state=%d, is_meas_running=%d\n", 
           lib_state, is_meas_running);

    // Small delay before enabling interrupts
    k_sleep(K_MSEC(100));

    // Enable interrupts
    result = as7058_osal_enable_interrupt();
    if (result != ERR_SUCCESS) {
        printk("Failed to enable AS7058 interrupt: %d\n", result);
        goto ERROR;
    }

    printk("Measurement and interrupts started successfully\n");
    printk("The first output will be generated in approximately 10 seconds.\n");

    /**************************************************************************
     *                           DURING MEASUREMENT                           *
     **************************************************************************/

    uint32_t output_counter = 0;
    uint32_t loop_counter = 0;

    // Main measurement loop
    while (g_keep_running) {
        loop_counter++;

        // Check if HRM is ready for execution
        if (g_ready_for_execution) {
            printk("Executing HRM algorithm...\n");
            
            result = as7058a_hrm_b0_execute();
            if (ERR_SUCCESS == result) {
                bio_hrm_b0_output_t hrm_output;
                result = as7058a_hrm_b0_get_output(&hrm_output);
                if (result != ERR_SUCCESS) {
                    printk("as7058a_hrm_b0_get_output returned error %d\n", result);
                    goto ERROR;
                }

                printk("[%u] Heart Rate: %d bpm, Quality: %d\n", output_counter, 
                       hrm_output.heart_rate / 10, hrm_output.quality);
                output_counter++;
            } else if (ERR_NO_DATA == result) {
                printk("HRM algorithm: no data available\n");
            } else {
                printk("as7058a_hrm_b0_execute returned error %d\n", result);
                goto ERROR;
            }

            g_ready_for_execution = FALSE;
        }

        // Periodically check FIFO level for debugging
        if ((loop_counter % 5) == 0) {
            uint16_t fifo_level = 0;
            err_code_t check_result = as7058_ifce_get_fifo_level(&fifo_level);
            if (check_result == ERR_SUCCESS) {
                printk("FIFO level: %u (threshold: 5)\n", fifo_level);
            }
        }

        // Sleep for 1 second
        k_sleep(K_MSEC(1000));

        // Safety timeout for testing (remove in production)
        if (loop_counter > 120) {  // 2 minutes
            printk("Test timeout reached, stopping measurement\n");
            g_keep_running = FALSE;
        }
    }

    printk("Exiting main loop\n");

    /**************************************************************************
     *                            STOP MEASUREMENT                            *
     **************************************************************************/

    as7058_osal_disable_interrupt();

    result = as7058_stop_measurement();
    if (result != ERR_SUCCESS) {
        printk("as7058_stop_measurement returned error %d\n", result);
    }

    result = vs_acc_stop();
    if (result != ERR_SUCCESS) {
        printk("vs_acc_stop returned error %d\n", result);
    }

    result = as7058a_hrm_b0_stop_processing();
    if (result != ERR_SUCCESS) {
        printk("as7058a_hrm_b0_stop_processing returned error %d\n", result);
    }

ERROR:
    printk("Entering cleanup phase\n");

    /**************************************************************************
     *                            DE-INITIALIZATION                           *
     **************************************************************************/

    result = as7058_shutdown();
    if (result != ERR_SUCCESS) {
        printk("as7058_shutdown returned error %d\n", result);
    }

    result = vs_acc_shutdown();
    if (result != ERR_SUCCESS) {
        printk("vs_acc_shutdown returned error %d\n", result);
    }

    result = as7058a_hrm_b0_shutdown();
    if (result != ERR_SUCCESS) {
        printk("as7058a_hrm_b0_shutdown returned error %d\n", result);
    }

    printk("Program completed\n");

    // In Zephyr, main shouldn't return - enter infinite loop
    while (1) {
        k_sleep(K_MSEC(1000));
    }

    return result;
}