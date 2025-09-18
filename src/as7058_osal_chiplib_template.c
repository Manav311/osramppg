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


K_SEM_DEFINE(button_sem, 0, 1);

static struct gpio_callback button_cb_data;
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(AS7058_NODE, gpios, {0}); // Button spec

/*! Create internal instance of the device configuration */
static struct device_config g_device_config;

const struct device *i2c_dev1 = DEVICE_DT_GET(DT_NODELABEL(i2c21));

/******************************************************************************
 *                               LOCAL FUNCTIONS                              *
 ******************************************************************************/

/*! Interrupt service routine of the interrupt pin */
static void interrupt_callback()
{
    err_code_t result;
    uint8_t pin_state = 1;

    

    if (NULL != g_device_config.callback) {
        do {
            /* Calls the ChipLib callback function registered by as7058_osal_register_int_handler */
            result = g_device_config.callback();

            /* Read the pin state again because it could be high in meanwhile again */
            if (ERR_SUCCESS == result) {
                 result = gpio_pin_get_dt(&button);
            }

        } while ((ERR_SUCCESS == result) && pin_state);
    }
}


void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {

	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
}
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
    

     if (!device_is_ready(i2c_dev1)) {

         result = ERR_SYSTEM_CONFIG;
        /* handle error */
    }
  // Button initialization
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device %s is not ready\n", button.port->name);
        result = ERR_SYSTEM_CONFIG;
    }

    int ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("Error: failed to configure button pin\n");
        result = ERR_SYSTEM_CONFIG;
    }

      gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

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
    if (false == g_device_config.init_done) {
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
    if (false == g_device_config.init_done) {
        return ERR_PERMISSION;
    }
   M_CHECK_NULL_POINTER(p_values);



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

    /* Disable I2C */
    // TODO i2c_shutdown();

    memset(&g_device_config, 0, sizeof(g_device_config));

    return ERR_SUCCESS;
}
