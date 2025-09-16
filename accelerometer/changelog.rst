ams software changelog (www.ams.com)
-------------------------------------------------------------------------------

Overview

    This document lists all notable changes to the
    Vital Signs Accelerometer module

*******************************************************************************
Version 1.2.2
*******************************************************************************

Version comment

       Documentation corrections


Added

        n/a


Changed

        n/a


Deprecated

        n/a


Removed

        n/a


Fixed

        * Corrected the documented unit of members of structure vs_acc_data_t
        * Corrected Doxygen syntax in the description of function
          vs_acc_get_data


Known issues & limitations

        n/a


*******************************************************************************
Version 1.2.1
*******************************************************************************

Version comment

       Support for overriding I2C address at compile time


Added

        * Support for overriding I2C address at compile time by defining a
          preprocessor macro named ACC_I2C_ADDR whose definition is the desired
          I2C address


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

*******************************************************************************
Version 1.2.0
*******************************************************************************

Version comment

       Support sample period configuration of 0us


Added

        * Option to set sample period to zero to power down accelerometer


Changed

        * Updated css-sw-utilities to v4.5.0


Deprecated

        n/a


Removed

        n/a


Fixed

        n/a


Known issues & limitations

        n/a

*******************************************************************************
Version 1.1.1
*******************************************************************************

Version comment

       Fixed loading of accelerometer simulation


Added

        n/a


Changed

        n/a


Deprecated

        n/a


Removed

        n/a


Fixed

        * Fixed issue that accelerometer simulation is not loaded if there is an I2C error


Known issues & limitations

        n/a

*******************************************************************************
Version 1.1.0
*******************************************************************************

Version comment

        Added mask to signal normalized accelerometer data


Added

        n/a


Changed

        * Bit0 of each accelerometer sample is set to mark the data as normalized


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

        Separate function for configuration of sample period


Added

        * Added new function to configure sample period


Changed

        * Sample period will no longer be configured via initialize function


Deprecated

        n/a


Removed

        n/a


Fixed

        n/a


Known issues & limitations

        n/a

*******************************************************************************
Version 0.1.2
*******************************************************************************

Version comment

        Add OSAL template for Vital Signs Accelerometer


Added

        * Added OSAL template for Vital Signs Accelerometer


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

*******************************************************************************
Version 0.1.1
*******************************************************************************

Version comment

        Add type size check in header


Added

        * Check using static assertion whether vs_acc_data_t has the expected
          size


Changed

        * Updated CSS SW Utilities to v3.4.0


Deprecated

        n/a


Removed

        n/a


Fixed

        n/a


Known issues & limitations

        n/a

*******************************************************************************
Version 0.1.0
*******************************************************************************

Version comment

        Initial release


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
