/**
 * @file CO_401_digital.h
 * @brief Digital process-image helpers for the CiA 401 Device core.
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
 * With digital events enabled, 0x6003 filter configuration is forwarded to the
 * product, input polarity is applied before 0x6000 is updated, and logical edge
 * masks request a TPDO through the 0x6000 OD entry rather than a fixed TPDO.
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
 * With digital output semantics enabled, 0x6200 remains the complete received
 * command image. Fault selection through 0x6206/0x6207 is applied before 0x6202
 * polarity, and 0x6208 then masks the final physical write. Masked-off bits keep
 * their current backend state through the product masked-write bridge.
 *
 * @param device Successfully bound Device runtime.
 */
void CO_401_digital_applyOutputs(CO_401_device_t *device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DIGITAL_H */
