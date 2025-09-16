ams software changelog (www.ams.com)
-------------------------------------------------------------------------------

Overview

    This document lists all notable changes to the
    Vital signs automatic gain control module.

*******************************************************************************
Version 2.2.0
*******************************************************************************

Added

        n/a

Changed

        * Improve performance when LED current adjustment is needed to bring
          signal within range by adding additional criteria that must be
          fulfilled before PD offset fastlock mode is exited.

Deprecated

        n/a

Removed

        n/a

Fixed

        n/a

Known issues & limitations

        n/a

*******************************************************************************
Version 2.1.0
*******************************************************************************

Added

        * Allow to set num_led_steps to zero to estimate the LED current for
          a target amplitude with PD-offset correction.

Changed

        * The LED current is now read cyclically in modes AGC_MODE_STATUS
          and in AGC_AMPL_CNTL_MODE_DISABLED.
        * Updated CSS SW Utilities to v4.8.0.
        * Performance and speed improvements for amplitude control.

Deprecated

        n/a

Removed

        n/a

Fixed

        * Fixed reset of amplitude filtering after LED current modifications.

Known issues & limitations

        n/a

*******************************************************************************
Version 2.0.0
*******************************************************************************

Version comment

        Reading of the current LED and PD offset values via the HAL

Added

        * Added new mode AGC_MODE_STATUS where the current LED and PD offset
          currents are provided but not controlled by the module
        * Added HAL functions agc_hal_get_led_current and agc_hal_get_pd_offset
        * Added a new AGC state AGC_STATE_UNDEFINED to support channels without
          linked LEDs

Changed

        * LED current is now provided in mode AGC_AMPL_CNTL_MODE_DISABLED

Deprecated

        n/a

Removed

        n/a

Fixed

        n/a

Known issues & limitations

        n/a

*******************************************************************************
Version 1.0.0
*******************************************************************************

Version comment

        Modified HAL interface of function 'agc_hal_extract_samples', fixed
        issue of slow fall back of LED current when PD offset is maximum.


Added

        n/a


Changed

        * Modified HAL function 'agc_hal_extract_samples' to signal new FIFO
          data.


Deprecated

        n/a


Removed

        n/a


Fixed

        * Modified local function 'led_current_control' to quickly step down
          the LED current in the case of a maximum PD offset.

Known issues & limitations

        n/a

*******************************************************************************
Version 0.1.1
*******************************************************************************

Version comment

        Changed start value of LED current control


Added

         When the amplitude control is enabled in the AGC algorithm, the initial value of the
         LED current shall be the mean of the configured maximum and minimum LED current.


Changed

        n/a


Deprecated

        n/a


Removed

        n/a


Fixed

        * Added math library dependency


Known issues & limitations

        n/a


*******************************************************************************
Version 0.1.0
*******************************************************************************

Version comment

        First release version of this software component.


Added

        n/a


Changed

        n/a


Deprecated

        n/a


Removed

        n/a


Fixed

        n/a


Known issues & limitations

        n/a
