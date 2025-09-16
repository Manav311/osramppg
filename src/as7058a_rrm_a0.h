/******************************************************************************
 * Copyright © 2023 ams-OSRAM AG                                              *
 * All rights are reserved.                                                   *
 *                                                                            *
 * FOR FULL LICENSE TEXT SEE LICENSE.TXT                                      *
 *                                                                            *
 ******************************************************************************/

#ifndef __AS7058A_RRM_A0_H__
#define __AS7058A_RRM_A0_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*!
 * \file
 * \brief The AS7058A RRM A0 algorithm processes data from the AS7058 Chip Library and returns respiration rate data as
 *        a result.
 */

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include <stdint.h>

#include "error_codes.h"

#include "as7058_app_manager_preprocessing.h"
#include "as7058_typedefs.h"
#include "bio_rrm_a0_typedefs.h"
#include "vital_signs_accelerometer.h"

/******************************************************************************
 *                                 FUNCTIONS                                  *
 ******************************************************************************/

/*!
 * \brief Initializes the RRM A0 library.
 *
 * The library transitions to Configuration state after initialization.
 *
 * \retval ::ERR_SUCCESS Initialized successfully.
 */
err_code_t as7058a_rrm_a0_initialize(void);

/*!
 * \brief Indicates to the RRM A0 library which sub-sample is used for acquiring PPG samples.
 *
 * When ::as7058a_rrm_a0_set_input is called, the RRM A0 library extracts the samples of the given sub-sample and uses
 * them to calculate the algorithm output.
 *
 * This function can only be called when the library is in Configuration state.
 *
 * \param[in] ppg_sub_sample ID of the sub-sample used to acquire PPG samples, see ::as7058_sub_sample_ids.
 *
 * \retval ::ERR_SUCCESS    Updated successfully.
 * \retval ::ERR_ARGUMENT   Invalid sub-sample ID.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_rrm_a0_set_signal_routing(as7058_sub_sample_ids_t ppg_sub_sample);

/*!
 * \brief Sets which signal sample preprocessing features are enabled in the RRM A0 library.
 *
 * This function can only be called when the library is in Configuration state.
 *
 * \param[in] flags Flags of enabled preprocessing features, see ::as7058_appmgr_preprocessing_flag. A preprocessing
 *                  feature is enabled when the corresponding bit is set and disabled when the bit is not set.
 *
 * \retval ::ERR_SUCCESS    Updated successfully.
 * \retval ::ERR_ARGUMENT   Invalid flags.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_rrm_a0_enable_preprocessing(uint32_t flags);

/*!
 * \brief Configures a signal sample preprocessing feature in the RRM A0 library.
 *
 * This function can only be called when the library is in Configuration state.
 *
 * \param[in] preprocessing Identifier of the signal sample preprocessing feature to configure.
 * \param[in] p_config      Pointer to the configuration structure for the given signal sample preprocessing feature,
 *                          which can found in \ref AS7058_APPMGR_PREPROCESSING.
 * \param[in] size          Size of the configuration structure.
 *
 * \retval ::ERR_SUCCESS    Updated successfully.
 * \retval ::ERR_ARGUMENT   Invalid preprocessing feature identifier or invalid configuration structure contents.
 * \retval ::ERR_POINTER    Invalid pointer argument value.
 * \retval ::ERR_SIZE       Invalid size of the configuration structure.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_rrm_a0_configure_preprocessing(as7058_appmgr_preprocessing_id_t preprocessing, const void *p_config,
                                                  uint16_t size);

/*!
 * \brief Starts a processing session in the RRM A0 library.
 *
 * The AS7058A AFE must be configured so that the sample frequency of the sub-sample selected via
 * ::as7058a_rrm_a0_set_signal_routing is 20 Hz, 25 Hz, 50 Hz, 100 Hz, or 200 Hz. If the algorithm should also process
 * accelerometer data, the accelerometer must be configured so that its sample frequency is either 25 Hz, 50 Hz, 100 Hz,
 * or 200 Hz.
 *
 * This function can only be called when the library is in Configuration state. The library transitions to Processing
 * state when this function executes successfully.
 *
 * \param[in] measurement_config   Measurement configuration used to acquire the data that will be provided to the
 *                                 library. This value is typically obtained from the Chip Library.
 * \param[in] acc_sample_period_us Sample period of the accelerometer in microseconds. Set to zero to disable
 *                                 accelerometer data processing in the algorithm.
 *
 * \retval ::ERR_SUCCESS    Updated successfully.
 * \retval ::ERR_CONFIG     Measurement configuration invalid or incompatible, or preprocessing feature enabled but not
 *                          configured.
 * \retval ::ERR_ARGUMENT   Invalid sample periods.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_rrm_a0_start_processing(as7058_meas_config_t measurement_config, uint32_t acc_sample_period_us);

/*!
 * \brief Provides measurement data of the regular measurement mode to the RRM A0 library.
 *
 * This function can only be called when the library is in Processing state.
 *
 * \param[in]  p_fifo_data           Pointer to the start of the sensor FIFO data. This data is typically obtained from
 *                                   the Chip Library. Can be NULL if fifo_data_size is zero.
 * \param[in]  fifo_data_size        Size of the FIFO data.
 * \param[in]  status_events         Chip status events, containing information regarding interrupts that occurred on
 *                                   the chip. This value is typically obtained from the Chip Library.
 * \param[in]  p_agc_statuses        Pointer to the start of an array containing Automatic Gain Control (AGC) status
 *                                   information for each sub-sample with enabled AGC. The required order of the status
 *                                   information items is determined by the as7058_meas_config_t::agc_channels array
 *                                   passed to ::as7058a_rrm_a0_start_processing. For example, if the passed
 *                                   as7058_meas_config_t::agc_channels array contains ::AS7058_SUB_SAMPLE_ID_PPG1_SUB1
 *                                   at index 0, this array must contain the AGC status information for
 *                                   ::AS7058_SUB_SAMPLE_ID_PPG1_SUB1 at index 0. AGC status information must always be
 *                                   provided for each sub-sample with enabled AGC. This pointer is typically obtained
 *                                   from the Chip Library. Can be NULL if agc_statuses_num is zero.
 * \param[in]  agc_statuses_num      Number of items in the array containing AGC status information. Must be equivalent
 *                                   to the number of sub-samples with enabled AGC. Can be zero if fifo_data_size is
 *                                   zero.
 * \param[in]  p_acc_samples         Pointer to the start of an array containing accelerometer samples. Can be NULL if
 *                                   acc_samples_num is zero.
 * \param[in]  acc_samples_num       Number of accelerometer samples.
 * \param[out] p_ready_for_execution Set to ::TRUE when the RRM A0 algorithm indicated that it is ready for execution,
 *                                   ::FALSE otherwise.
 *
 * \retval ::ERR_SUCCESS         Measurement data accepted.
 * \retval ::ERR_SIZE            Too many samples in the FIFO data or invalid number of AGC status information items.
 * \retval ::ERR_SYNCHRONISATION Accelerator and PPG data are out of synchronization. Internal buffers are too small.
 * \retval ::ERR_DATA            Data inconsistency detected.
 * \retval ::ERR_POINTER         Invalid pointer argument value.
 * \retval ::ERR_PERMISSION      Invalid state.
 */
err_code_t as7058a_rrm_a0_set_input(const uint8_t *p_fifo_data, uint16_t fifo_data_size,
                                    as7058_status_events_t status_events, const agc_status_t *p_agc_statuses,
                                    uint8_t agc_statuses_num, const vs_acc_data_t *p_acc_samples,
                                    uint16_t acc_samples_num, uint8_t *p_ready_for_execution);

/*!
 * \brief Executes the RRM A0 algorithm.
 *
 * This function can only be called when the library is in Processing state.
 *
 * \retval ::ERR_SUCCESS    Execution successful. Algorithm output is available and can be read via
 *                          ::as7058a_rrm_a0_get_output.
 * \retval ::ERR_NO_DATA    Execution successful, but no updated algorithm output is available.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_rrm_a0_execute(void);

/*!
 * \brief Writes the RRM A0 algorithm output to a buffer provided by the caller.
 *
 * After 20 seconds, an algorithm output will be available approximately every second. Each algorithm output can only be
 * read once.
 *
 * This function can only be called when the library is in Processing state.
 *
 * \param[out] p_output Pointer to the buffer where the output shall be written to.
 *
 * \retval ::ERR_SUCCESS    Output data write successful.
 * \retval ::ERR_NO_DATA    No algorithm output available.
 * \retval ::ERR_POINTER    Invalid pointer argument value.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_rrm_a0_get_output(bio_rrm_a0_output_t *p_output);

/*!
 * \brief Stops the current processing session of the RRM A0 library.
 *
 * This function can only be called when the library is not in Uninitialized state.
 *
 * \retval ::ERR_SUCCESS    Stop successful.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_rrm_a0_stop_processing(void);

/*!
 * \brief De-initializes the RRM A0 library.
 *
 * \retval ::ERR_SUCCESS De-initialization successful.
 */
err_code_t as7058a_rrm_a0_shutdown(void);

/*!
 * \brief Gets the version of the RRM A0 library.
 *
 * \return Version string.
 */
const char *as7058a_rrm_a0_get_version(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __AS7058A_RRM_A0_H__ */
