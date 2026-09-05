/**
 * @file CO_401_digital.h
 * @brief Stage-1 digital process-image helpers for the CiA 401 Device core.
 */

#ifndef CO_401_DIGITAL_H
#define CO_401_DIGITAL_H

#include "CO_401_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Refresh enabled 0x6000 banks; an unbound or incomplete runtime is a no-op.
 *
 * Direct callers must use the same serialization contract as
 * CO_401_device_process(): serialize against concurrent SDO/PDO access to the
 * mapped OD entries, and do not recursively acquire that serialization from an
 * IOIF callback.
 *
 * @param device Successfully bound Device runtime.
 */
void CO_401_digital_refreshInputs(CO_401_device_t *device);

/**
 * @brief Apply enabled 0x6200 command banks; an unbound or incomplete runtime is a no-op.
 *
 * Direct callers must use the same serialization contract as
 * CO_401_device_process(): serialize against concurrent SDO/PDO access to the
 * mapped OD entries, and do not recursively acquire that serialization from an
 * IOIF callback.
 *
 * @param device Successfully bound Device runtime.
 */
void CO_401_digital_applyOutputs(CO_401_device_t *device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DIGITAL_H */
