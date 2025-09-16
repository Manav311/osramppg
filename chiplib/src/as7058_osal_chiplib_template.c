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

/******************************************************************************
 *                               LOCAL FUNCTIONS                              *
 ******************************************************************************/

/*! Interrupt service routine of the interrupt pin */
static void interrupt_callback()
{
    err_code_t result;
    uint8_t pin_state = 0;

    if (NULL != g_device_config.callback) {
        do {
            /* Calls the ChipLib callback function registered by as7058_osal_register_int_handler */
            result = g_device_config.callback();

            /* Read the pin state again because it could be high in meanwhile again */
            if (ERR_SUCCESS == result) {
                // TODO result = get_int_pin_state(&pin_state);
            }

        } while ((ERR_SUCCESS == result) && pin_state);
    }
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

    /* Configure I2C */
    // TODO if ((ERR_SUCCESS == result) && (RETURN_CODE_OK != i2c_init())
    {
        result = ERR_SYSTEM_CONFIG;
    }

    /* Configure interrupt pin: Triggering on rising edge, register interrupt_callback */
    // TODO if ((ERR_SUCCESS == result) && (RETURN_CODE_OK != int_pin_init(TRIG_RISING, interrupt_callback))
    {
        result = ERR_SYSTEM_CONFIG;
    }

    if (ERR_SUCCESS == result) {
        g_device_config.init_done = TRUE;
    } else {
        as7058_osal_shutdown();
    }

    return result;
}

err_code_t as7058_osal_write_registers(uint8_t address, uint16_t number, uint8_t *p_values)
{
    if (FALSE == g_device_config.init_done) {
        return ERR_PERMISSION;
    }

    M_CHECK_NULL_POINTER(p_values);

    /* Call the platform specifc i2c transmit function */
    // TODO if (RETURN_CODE_OK != i2c_write(g_i2c_address, address, number, p_values)
    {
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
    {
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
