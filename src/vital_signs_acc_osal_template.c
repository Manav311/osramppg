/******************************************************************************
 * Copyright by ams AG                                                        *
 * All rights are reserved.                                                   *
 *                                                                            *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING      *
 * THE SOFTWARE.                                                              *
 *                                                                            *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS        *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT          *
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS          *
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT   *
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,      *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT           *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,      *
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY      *
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT        *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE      *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.       *
 ******************************************************************************/

/* This template can be used as starting point when implementing a custom OSAL for the Vital Signs Accelerometer
 * component. All lines that contain "// TODO:" need to be replaced with platform-specific code. */

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

 
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>


#include "vital_signs_acc_osal.h"

static const struct device *i2c_dev2 = DEVICE_DT_GET(DT_NODELABEL(i2c21));

#define LIS12DH_I2C_ADDR 0x19

/******************************************************************************
 *                             GLOBAL FUNCTIONS                               *
 ******************************************************************************/


 LOG_MODULE_REGISTER(as7058_ascc, LOG_LEVEL_INF);



 static err_code_t vs_acc_osal_ensure_init(void)
{

    if (!device_is_ready(i2c_dev2)) {
        LOG_ERR("I2C device not ready");
        return ERR_I2C;
    }

    
    return ERR_SUCCESS;
}

err_code_t vs_acc_osal_transfer_i2c(const char *p_config, uint8_t dev_addr, const uint8_t *p_send_data,
                                    uint16_t send_len, uint8_t *p_recv_data, uint16_t recv_len)
{
    M_UNUSED_PARAM(p_config);
    
    err_code_t result = vs_acc_osal_ensure_init();
    if (ERR_SUCCESS != result) {
        return result;
    }

    if (!device_is_ready(i2c_dev2)) {
        LOG_ERR("I2C device not ready for transfer");
        return ERR_I2C;
    }

    int ret = 0;

    /* Handle different I2C transfer scenarios */
    if (send_len > 0 && recv_len > 0) {
        /* Write-then-read operation (typical for register reads) */
        if (p_send_data == NULL || p_recv_data == NULL) {
            return ERR_POINTER;
        }
        
        ret = i2c_write_read(i2c_dev2, LIS12DH_I2C_ADDR, p_send_data, send_len, p_recv_data, recv_len);
        if (ret < 0) {
            LOG_ERR("I2C write-read failed: %d", ret);
            return ERR_I2C;
        }
    }
    else if (send_len > 0 && recv_len == 0) {
        /* Write-only operation (typical for register writes) */
        if (p_send_data == NULL) {
            return ERR_POINTER;
        }
        
        ret = i2c_write(i2c_dev2, p_send_data, send_len, LIS12DH_I2C_ADDR);
        if (ret < 0) {
            LOG_ERR("I2C write failed: %d", ret);
            return ERR_I2C;
        }
    }
    else if (send_len == 0 && recv_len > 0) {
        /* Read-only operation (less common) */
        if (p_recv_data == NULL) {
            return ERR_POINTER;
        }
        
        ret = i2c_read(i2c_dev2, p_recv_data, recv_len, LIS12DH_I2C_ADDR);
        if (ret < 0) {
            LOG_ERR("I2C read failed: %d", ret);
            return ERR_I2C;
        }
    }
    else {
        /* Invalid parameters - no data to send or receive */
        LOG_ERR("Invalid I2C transfer parameters");
        return ERR_ARGUMENT;
    }

    return ERR_SUCCESS;
}

err_code_t vs_acc_osal_get_tick(const char *p_config, uint32_t *p_ms_tick)
{
    M_UNUSED_PARAM(p_config);
    
    if (p_ms_tick == NULL) {
        return ERR_POINTER;
    }

    /* Get current uptime in milliseconds using Zephyr kernel API */
    int64_t uptime_ms = k_uptime_get();
    
    /* Convert to uint32_t (will wrap around after ~49 days) */
    *p_ms_tick = (uint32_t)(uptime_ms & 0xFFFFFFFF);
    
    return ERR_SUCCESS;
}
