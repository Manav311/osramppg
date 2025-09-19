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

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include "as7058_osal_chiplib.h"
#include "error_codes.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>



const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));


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

/*! Create internal instance of the device configuration */
static struct device_config g_device_config;

#define AS7058_NODE                   DT_ALIAS(as7058interrupt)



struct gpio_dt_spec as7058_sensor_spec = GPIO_DT_SPEC_GET(AS7058_NODE, gpios); // Blue LED spec

// Add this near the top with other definitions
static struct gpio_callback as7058_cb_data;

/******************************************************************************
 *                               LOCAL FUNCTIONS                              *
 ******************************************************************************/

/*! Interrupt service routine of the interrupt pin */
static void interrupt_callback()

{
    printk("interrupt_callback");
    err_code_t result;
    uint8_t pin_state = 0;

    if (NULL != g_device_config.callback) {
        do {
            /* Calls the ChipLib callback function registered by as7058_osal_register_int_handler */
            result = g_device_config.callback();

            /* Read the pin state again because it could be high in meanwhile again */
            if (ERR_SUCCESS == result) {
                result = gpio_pin_get_dt(&as7058_sensor_spec);
            }

        } while ((ERR_SUCCESS == result) && (pin_state == 0));
    }
}

/******************************************************************************
 *                             GLOBAL FUNCTIONS                               *
 ******************************************************************************/

err_code_t as7058_osal_initialize(const char *p_interface_desc)
{
    err_code_t result = ERR_SUCCESS;
    int ret;

    /* Shutdown OSAL interface in case there is one already opened */
    if (g_device_config.init_done) {
        as7058_osal_shutdown();
    }

    /* Configure I2C */
    // TODO if ((ERR_SUCCESS == result) && (RETURN_CODE_OK != i2c_init())

     if (!device_is_ready(i2c_dev)) {
        printk("I2C device not ready");
        result = ERR_SYSTEM_CONFIG;
    }
    printk("I2C device ready");

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
    gpio_init_callback(&as7058_cb_data, interrupt_callback, BIT(as7058_sensor_spec.pin));
    gpio_add_callback(as7058_sensor_spec.port, &as7058_cb_data);


    /* Configure interrupt pin: Triggering on rising edge, register interrupt_callback */
    // TODO if ((ERR_SUCCESS == result) && (RETURN_CODE_OK != int_pin_init(TRIG_RISING, interrupt_callback))
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

err_code_t as7058_osal_write_registers(uint8_t address, uint16_t number, const uint8_t *p_values)
{
    if (FALSE == g_device_config.init_done) {
        return ERR_PERMISSION;
    }

    M_CHECK_NULL_POINTER(p_values);

    /* Call the platform specifc i2c transmit function */
    // TODO if (RETURN_CODE_OK != i2c_write(g_i2c_address, address, number, p_values)

    int8_t buf[2] = { address, *p_values };
    int ret = i2c_write(i2c_dev, buf, number, g_i2c_address);
    if (ret != 0) {
        printk("Write failed: reg 0x%02X = 0x%02X (err=%d)", address, *p_values, ret);
        return ERR_DATA_TRANSFER;
    }

    printk("Write success: reg 0x%02X = 0x%02X \n", address, *p_values);


    return ERR_SUCCESS;
}

err_code_t as7058_osal_read_registers(uint8_t address, uint16_t number, uint8_t *p_values)
{
    if (FALSE == g_device_config.init_done) {
        return ERR_PERMISSION;
    }

    M_CHECK_NULL_POINTER(p_values);

     int ret = i2c_write_read(i2c_dev, g_i2c_address, &address, 1, p_values, number);
    if (ret != 0) {
        printk("Read failed: reg 0x%02X (err=%d)", address, ret);
        return ERR_DATA_TRANSFER;
    }
    printk("Read success: reg 0x%02X = 0x%02X \n", address, *p_values);

    /* Call the platform specifc i2c receive function */
    // TODO if (RETURN_CODE_OK != i2c_read(g_i2c_address, address, number, p_values)
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

    /* Disable I2C */
    // TODO i2c_shutdown();

    gpio_pin_interrupt_configure_dt(&as7058_sensor_spec,
                                               GPIO_INT_DISABLE);

    memset(&g_device_config, 0, sizeof(g_device_config));

    return ERR_SUCCESS;
}
