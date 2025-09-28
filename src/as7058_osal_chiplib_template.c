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

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/device.h>

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

/* Pull the GPIO spec from DT: */
static const struct gpio_dt_spec sens_int = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), my_sensor_int_gpios);
/* Declare the callback object */
static struct gpio_callback sens_cb;

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

/******************************************************************************
 *                               LOCAL FUNCTIONS                              *
 ******************************************************************************/

 /* This is the ISR you want called on an edge */


/*! Interrupt service routine of the interrupt pin */


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

     if (!device_is_ready(i2c_dev)) {

        result = ERR_SYSTEM_CONFIG;
        /* handle error */
    }

    /* Configure I2C */
    // TODO if ((ERR_SUCCESS == result) && (RETURN_CODE_OK != i2c_init())
    {
       // result = ERR_SYSTEM_CONFIG;
    }


   



    /* Configure interrupt pin: Triggering on rising edge, register interrupt_callback */
    // TODO if ((ERR_SUCCESS == result) && (RETURN_CODE_OK != int_pin_init(TRIG_RISING, interrupt_callback))
    {
        //result = ERR_SYSTEM_CONFIG;
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


     /* Build [reg + data] buffer */
    uint8_t buf[number + 1];
    buf[0] = address;
    memcpy(&buf[1], p_values, number);

    int ret = i2c_write(i2c_dev, buf, number + 1, g_i2c_address);
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

    M_CHECK_NULL_POINTER(p_values);

    /* Call the platform specifc i2c receive function */
    // TODO if (RETURN_CODE_OK != i2c_read(g_i2c_address, address, number, p_values)

     /* Repeated-start: write 1 byte (reg addr), then read `number` bytes */
    int ret = i2c_write_read(i2c_dev, g_i2c_address, &address, 1, p_values, number);
    if (ret < 0) {
        return ERR_DATA_TRANSFER;
    }

    return ERR_SUCCESS;
}

err_code_t as7058_osal_register_int_handler(as7058_osal_interrupt_t callback_function)
{
    printk("as7058_osal_register_int_handler\n");
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
