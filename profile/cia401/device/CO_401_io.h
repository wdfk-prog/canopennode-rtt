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
    CO_401_IO_ERROR   /**< Backend operation failed; the prior process state is retained for retry. */
} CO_401_io_result_t;

/**
 * Product-owned I/O interface for CiA 401 logical process values.
 *
 * Callbacks must be bounded and non-blocking. Digital callbacks exchange the
 * physical bank value immediately before input polarity or after output profile
 * processing; analogue callbacks exchange the logical CiA 401 process value.
 * Digital bank 0 corresponds to 0x6000:01/0x6200:01 and analogue channel 0 to
 * 0x6401:01/0x6411:01. Scaling, ADC/DAC conversion and peripheral transactions
 * remain product responsibilities.
 */
typedef struct {
    /** Read one physical 8-bit digital-input bank before CiA 401 polarity processing. */
    CO_401_io_result_t (*readDigital8)(void *object, uint8_t bank, uint8_t *value);
    /** Apply one physical 8-bit digital-output bank after CiA 401 output processing. */
    CO_401_io_result_t (*writeDigital8)(void *object, uint8_t bank, uint8_t value);
    /** Read one logical signed 16-bit analogue-input channel into @p value. */
    CO_401_io_result_t (*readAnalog16)(void *object, uint8_t channel, int16_t *value);
    /** Apply one logical signed 16-bit analogue-output channel command. */
    CO_401_io_result_t (*writeAnalog16)(void *object, uint8_t channel, int16_t value);
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    /** Apply the 0x6003 per-line filter-enable mask to one product input bank. */
    CO_401_io_result_t (*setDigitalInputFilter8)(void *object, uint8_t bank, uint8_t enabledMask);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    /**
     * Apply physical output bits selected by @p mask for 0x6206 keep-current and
     * 0x6208 output-filter semantics. Bits clear in @p mask must retain their
     * current physical state, including before the first normal output write.
     */
    CO_401_io_result_t (*writeDigital8Masked)(void *object, uint8_t bank, uint8_t value, uint8_t mask);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
} CO_401_io_if_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_IO_H */
