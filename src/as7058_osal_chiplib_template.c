/******************************************************************************
 * Copyright © 2022 ams-OSRAM AG                                              *
 * All rights are reserved.                                                   *
 *                                                                            *
 * FOR FULL LICENSE TEXT SEE LICENSE.TXT                                      *
 *                                                                            *
 ******************************************************************************/

/*
This template shall help to implement your own OSAL
which can be called by the AS7058 Chip Library.
All code lines which start with '// TODO' must be replaced by your own implementations.
*/


#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include "as7058_osal_chiplib.h"
#include "error_codes.h"



/******************************************************************************
 *                                DEFINITIONS                                 *
 ******************************************************************************/

/*! internal structure which saves OSAL internal parameter */
struct device_config {
    volatile uint8_t init_done;                /*!< 0 < ::as7058_osal_initialize was successful called */
    volatile as7058_osal_interrupt_t callback; /*!< saves the link to the callback function of the chiplib */
};

/******************************************************************************
 *                                  GLOBALS                                   *
 ******************************************************************************/

/*! I2C address of the AS7058 */
static const uint8_t g_i2c_address = 0x55;


#define AS7058_NODE                   DT_ALIAS(as7058interrupt)

#define LISDH12_NODE                   DT_ALIAS(lis2dh12interrupt)  



struct gpio_dt_spec as7058_sensor_spec = GPIO_DT_SPEC_GET(AS7058_NODE, gpios); // Blue LED spec
struct gpio_dt_spec lisdh12_sensor_spec = GPIO_DT_SPEC_GET(LISDH12_NODE, gpios); // Red LED spec

// Add this near the top with other definitions
static struct gpio_callback as7058_cb_data;
//static K_SEM_DEFINE(as7058_data_ready_sem, 0, 1);

/*! Create internal instance of the device configuration */
static struct device_config g_device_config;

const struct device *i2c_dev1 = DEVICE_DT_GET(DT_NODELABEL(i2c21));

/******************************************************************************
 *                               LOCAL FUNCTIONS                              *
 ******************************************************************************/


 void as7058_interrupt_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Give the semaphore to unblock the data processing thread
    //k_sem_give(&as7058_data_ready_sem);
   // printk("as7058_interrupt_handler called \n");

    err_code_t result;
    uint8_t pin_state = 1;

    if (NULL != g_device_config.callback) {
        do {
            /* Calls the ChipLib callback function registered by as7058_osal_register_int_handler */
            result = g_device_config.callback();

            /* Read the pin state again because it could be high in meanwhile again */
            if (ERR_SUCCESS == result) {
                 result = gpio_pin_get_dt(&as7058_sensor_spec);
            }

        } while ((ERR_SUCCESS == result) && pin_state);
    }
}


/*! Interrupt service routine of the interrupt pin */

/******************************************************************************
 *                             GLOBAL FUNCTIONS                               *
 ******************************************************************************/

err_code_t as7058_osal_initialize(const char *p_interface_desc)
{
    err_code_t result = ERR_SUCCESS;

   /* Shutdown OSAL interface in case there is one already opened */
    if (g_device_config.init_done) {
        as7058_osal_shutdown();
    }

    int ret;


    if (!device_is_ready(i2c_dev1)) {

        result = ERR_SYSTEM_CONFIG;
        /* handle error */
    }

    if (!gpio_is_ready_dt(&as7058_sensor_spec)) {
        printk("Error: as7058 interrupt GPIO device not ready\n");
        result = ERR_SYSTEM_CONFIG;
    }

    ret = gpio_pin_configure_dt(&as7058_sensor_spec, GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure interrupt pin\n", ret);
        result = ERR_SYSTEM_CONFIG;
    }
    
    // Setup the callback
    gpio_init_callback(&as7058_cb_data, as7058_interrupt_handler, BIT(as7058_sensor_spec.pin));
    gpio_add_callback(as7058_sensor_spec.port, &as7058_cb_data);

    // Enable the interrupt
    ret = gpio_pin_interrupt_configure_dt(&as7058_sensor_spec, GPIO_INT_EDGE_RISING);
    if (ret != 0) {
        printk("Error %d: failed to configure interrupt\n", ret);
        result = ERR_SYSTEM_CONFIG;
    }


    if (ERR_SUCCESS == result) {
        g_device_config.init_done = TRUE;
    } else {
        as7058_osal_shutdown();
    }


    return result;
}

err_code_t as7058_osal_write_registers(uint8_t address,
                                      uint16_t number,
                                      const uint8_t *p_values)
{
   if (FALSE == g_device_config.init_done) {
        return ERR_PERMISSION;
    }

    M_CHECK_NULL_POINTER(p_values);

    /* Build [reg + data] buffer */
    uint8_t buf[number + 1];
    buf[0] = address;
    memcpy(&buf[1], p_values, number);

    int ret = i2c_write(i2c_dev1, buf, number + 1, g_i2c_address);
    if (ret < 0) {
        return ERR_DATA_TRANSFER;
    }

    return ERR_SUCCESS;
}


err_code_t as7058_osal_read_registers(uint8_t address, uint16_t number, uint8_t *p_values)
{
    if (FALSE == g_device_config.init_done) {
        return ERR_PERMISSION;
    }

  //  M_CHECK_NULL_POINTER(p_values);

    /* Repeated-start: write 1 byte (reg addr), then read `number` bytes */
    int ret = i2c_write_read(i2c_dev1, g_i2c_address, &address, 1, p_values, number);
    if (ret < 0) {
        return ERR_DATA_TRANSFER;
    }

    return ERR_SUCCESS;
}

err_code_t as7058_osal_register_int_handler(as7058_osal_interrupt_t callback_function)
{
    if (FALSE == g_device_config.init_done) {
        return ERR_PERMISSION;
    }

    g_device_config.callback = callback_function;

    return ERR_SUCCESS;
}

err_code_t as7058_osal_shutdown(void)
{
    /* Clean up of system resources */

    /* Deactivate interrupt pin */
    // TODO int_pin_shutdown();

    gpio_pin_interrupt_configure_dt(&as7058_sensor_spec,
                                               GPIO_INT_DISABLE);

    /* Disable I2C */
    // TODO i2c_shutdown();

    memset(&g_device_config, 0, sizeof(g_device_config));

    return ERR_SUCCESS;
}
