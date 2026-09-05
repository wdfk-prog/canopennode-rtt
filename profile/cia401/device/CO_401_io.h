/**
 * @file CO_401_io.h
 * @brief Bounded product I/O abstraction for the Pure-C CiA 401 Device core.
 */

#ifndef CO_401_IO_H
#define CO_401_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result returned by one non-blocking product I/O operation. */
typedef enum {
    CO_401_IO_OK = 0, /**< Operation completed and the supplied value is valid/applied. */
    CO_401_IO_BUSY,   /**< Backend cannot complete now; the profile retries on a later process pass. */
    CO_401_IO_ERROR   /**< Backend operation failed; Stage 1 keeps the prior image/command and retries later. */
} CO_401_io_result_t;

/**
 * Product-owned I/O interface for CiA 401 logical process values.
 *
 * Callbacks must be bounded and non-blocking. They operate on logical CiA 401
 * process values rather than MCU peripherals: digital bank 0 corresponds to
 * 0x6000:01/0x6200:01 and analogue channel 0 corresponds to
 * 0x6401:01/0x6411:01. Physical scaling, ADC alignment, DAC conversion and
 * GPIO/expander transactions remain product responsibilities.
 */
typedef struct {
    /** Read one logical 8-bit digital-input bank into @p value. */
    CO_401_io_result_t (*readDigital8)(void *object, uint8_t bank, uint8_t *value);
    /** Apply one logical 8-bit digital-output bank command. */
    CO_401_io_result_t (*writeDigital8)(void *object, uint8_t bank, uint8_t value);
    /** Read one logical signed 16-bit analogue-input channel into @p value. */
    CO_401_io_result_t (*readAnalog16)(void *object, uint8_t channel, int16_t *value);
    /** Apply one logical signed 16-bit analogue-output channel command. */
    CO_401_io_result_t (*writeAnalog16)(void *object, uint8_t channel, int16_t value);
} CO_401_io_if_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_IO_H */
