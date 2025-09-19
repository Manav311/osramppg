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

#include "vital_signs_acc_osal.h"

/******************************************************************************
 *                             GLOBAL FUNCTIONS                               *
 ******************************************************************************/

err_code_t vs_acc_osal_transfer_i2c(const char *p_config, uint8_t dev_addr, const uint8_t *p_send_data,
                                    uint16_t send_len, uint8_t *p_recv_data, uint16_t recv_len)
{
    M_UNUSED_PARAM(p_config);

    // TODO: if (RETURN_CODE_OK != i2c_transfer(dev_addr, p_send_data, send_len, p_recv_data, recv_len))
    {
        return ERR_I2C;
    }

    return ERR_SUCCESS;
}

err_code_t vs_acc_osal_get_tick(const char *p_config, uint32_t *p_ms_tick)
{
    M_UNUSED_PARAM(p_config);

    // TODO: if (RETURN_CODE_OK != get_microseconds(p_ms_tick))
    {
        return ERR_TIMER_ACCESS;
    }

    return ERR_SUCCESS;
}
