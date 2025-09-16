/******************************************************************************
 * Copyright © 2023 ams-OSRAM AG                                              *
 * All rights are reserved.                                                   *
 *                                                                            *
 * FOR FULL LICENSE TEXT SEE LICENSE.TXT                                      *
 *                                                                            *
 ******************************************************************************/

#ifndef __AS7058A_SPO2_A0_H__
#define __AS7058A_SPO2_A0_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*!
 * \file
 * \brief The AS7058A SpO2 A0 algorithm processes data from the AS7058 Chip Library and returns SpO2 data as a result.
 */

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include <stdint.h>

#include "error_codes.h"

#include "as7058_app_manager_preprocessing.h"
#include "as7058_typedefs.h"
#include "bio_spo2_a0_typedefs.h"

/******************************************************************************
 *                                 FUNCTIONS                                  *
 ******************************************************************************/

/*!
 * \brief Initializes the SpO2 A0 library.
 *
 * The library transitions to Configuration state after initialization.
 *
 * \retval ::ERR_SUCCESS Initialized successfully.
 */
err_code_t as7058a_spo2_a0_initialize(void);

/*!
 * \brief Indicates to the SpO2 A0 library which sub-samples are used for acquiring samples.
 *
 * When ::as7058a_spo2_a0_set_input is called, the SpO2 A0 library extracts the samples of the given sub-samples and
 * uses them to calculate the algorithm output.
 *
 * This function can only be called when the library is in Configuration state.
 *
 * \param[in] ppg_red_sub_sample ID of the sub-sample used to acquire red PPG samples, see ::as7058_sub_sample_ids.
 * \param[in] ppg_ir_sub_sample  ID of the sub-sample used to acquire infrared PPG samples, see ::as7058_sub_sample_ids.
 * \param[in] ambient_sub_sample ID of the sub-sample used to acquire ambient light samples, see
 *                               ::as7058_sub_sample_ids.
 *
 * \retval ::ERR_SUCCESS    Updated successfully.
 * \retval ::ERR_ARGUMENT   Invalid sub-sample IDs.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_spo2_a0_set_signal_routing(as7058_sub_sample_ids_t ppg_red_sub_sample,
                                              as7058_sub_sample_ids_t ppg_ir_sub_sample,
                                              as7058_sub_sample_ids_t ambient_sub_sample);

/*!
 * \brief Configures the SpO2 A0 library.
 *
 * This function can only be called when the library is in Configuration state.
 *
 * \param[in] config Configuration structure.
 *
 * \retval ::ERR_SUCCESS    Updated successfully.
 * \retval ::ERR_ARGUMENT   Invalid arguments.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_spo2_a0_configure(bio_spo2_a0_configuration_t config);

/*!
 * \brief Sets which signal sample preprocessing features are enabled in the SpO2 A0 library.
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
err_code_t as7058a_spo2_a0_enable_preprocessing(uint32_t flags);

/*!
 * \brief Configures a signal sample preprocessing feature in the SpO2 A0 library.
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
err_code_t as7058a_spo2_a0_configure_preprocessing(as7058_appmgr_preprocessing_id_t preprocessing, const void *p_config,
                                                   uint16_t size);

/*!
 * \brief Starts a processing session in the SpO2 A0 library.
 *
 * The AS7058A AFE must be configured so that the sample frequencies of all sub-samples selected via
 * ::as7058a_spo2_a0_set_signal_routing are set to an identical value.
 *
 * This function can only be called when the library is in Configuration state. The library transitions to Processing
 * state when this function executes successfully.
 *
 * \param[in] measurement_config Measurement configuration used to acquire the data that will be provided to the
 *                               library. This value is typically obtained from the Chip Library.
 *
 * \retval ::ERR_SUCCESS    Updated successfully.
 * \retval ::ERR_CONFIG     Measurement configuration invalid or incompatible, or preprocessing feature enabled but not
 *                          configured.
 * \retval ::ERR_ARGUMENT   Invalid sample periods.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_spo2_a0_start_processing(as7058_meas_config_t measurement_config);

/*!
 * \brief Provides measurement data of the regular measurement mode to the SpO2 A0 library.
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
 *                                   passed to ::as7058a_spo2_a0_start_processing. For example, if the passed
 *                                   as7058_meas_config_t::agc_channels array contains ::AS7058_SUB_SAMPLE_ID_PPG1_SUB1
 *                                   at index 0, this array must contain the AGC status information for
 *                                   ::AS7058_SUB_SAMPLE_ID_PPG1_SUB1 at index 0. AGC status information must always be
 *                                   provided for each sub-sample with enabled AGC. This pointer is typically obtained
 *                                   from the Chip Library. Can be NULL if agc_statuses_num is zero.
 * \param[in]  agc_statuses_num      Number of items in the array containing AGC status information. Must be equivalent
 *                                   to the number of sub-samples with enabled AGC. Can be zero if fifo_data_size is
 *                                   zero.
 * \param[out] p_ready_for_execution Set to ::TRUE when the SpO2 A0 algorithm indicated that it is ready for execution,
 *                                   ::FALSE otherwise.
 *
 * \retval ::ERR_SUCCESS    Measurement data accepted.
 * \retval ::ERR_SIZE       Too many samples in the FIFO data or invalid number of AGC status information items.
 * \retval ::ERR_OVERFLOW   Number of samples received per sub-sample differs too much between sub-samples to be
 *                          handled.
 * \retval ::ERR_DATA       Data inconsistency detected.
 * \retval ::ERR_POINTER    Invalid pointer argument value.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_spo2_a0_set_input(const uint8_t *p_fifo_data, uint16_t fifo_data_size,
                                     as7058_status_events_t status_events, const agc_status_t *p_agc_statuses,
                                     uint8_t agc_statuses_num, uint8_t *p_ready_for_execution);

/*!
 * \brief Executes the SpO2 A0 algorithm.
 *
 * This function can only be called when the library is in Processing state.
 *
 * \retval ::ERR_SUCCESS    Execution successful. Algorithm output is available and can be read via
 *                          ::as7058a_spo2_a0_get_output.
 * \retval ::ERR_NO_DATA    Execution successful, but no updated algorithm output is available.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_spo2_a0_execute(void);

/*!
 * \brief Writes the SpO2 A0 algorithm output to a buffer provided by the caller.
 *
 * An algorithm output will be available approximately every second. Each algorithm output can only be read once.
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
err_code_t as7058a_spo2_a0_get_output(bio_spo2_a0_output_t *p_output);

/*!
 * \brief Stops the current processing session of the SpO2 A0 library.
 *
 * This function can only be called when the library is not in Uninitialized state.
 *
 * \retval ::ERR_SUCCESS    Stop successful.
 * \retval ::ERR_PERMISSION Invalid state.
 */
err_code_t as7058a_spo2_a0_stop_processing(void);

/*!
 * \brief De-initializes the SpO2 A0 library.
 *
 * \retval ::ERR_SUCCESS De-initialization successful.
 */
err_code_t as7058a_spo2_a0_shutdown(void);

/*!
 * \brief Gets the version of the SpO2 A0 library.
 *
 * \return Version string.
 */
const char *as7058a_spo2_a0_get_version(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __AS7058A_SPO2_A0_H__ */
