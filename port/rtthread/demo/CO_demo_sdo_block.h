/**
 * @file CO_demo_sdo_block.h
 * @brief Test-only variable-length DOMAIN backend for SDO server block validation.
 */

#ifndef CO_DEMO_SDO_BLOCK_H_
#define CO_DEMO_SDO_BLOCK_H_

#include "CANopen.h"

#if !defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST)
#error "CO_demo_sdo_block requires PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST"
#endif /* !defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Maximum payload accepted by the SDO Server block-transfer DOMAIN fixture. */
#define CO_DEMO_SDO_BLOCK_MAX_PAYLOAD_SIZE 2048U

/** Long-lived state for the test-only SDO server block-transfer DOMAIN. */
typedef struct {
    uint8_t data[CO_DEMO_SDO_BLOCK_MAX_PAYLOAD_SIZE]; /**< Staged and committed DOMAIN payload bytes. */
    OD_size_t validLength; /**< Length of the last complete payload available for upload. */
    bool_t writeInProgress; /**< True after a partial download has modified staged bytes. */
    OD_extension_t extension; /**< OD 0x2304 extension owned for the application lifetime. */
} CO_demo_sdo_block_t;

/**
 * @brief Initialize the bounded DOMAIN fixture to a deterministic safe value.
 *
 * @param demo SDO block test state.
 */
void CO_demo_sdo_block_init(CO_demo_sdo_block_t *demo);

/**
 * @brief Bind the DOMAIN read/write extension to OD 0x2304.
 *
 * A complete payload survives communication reset. If the old SDO server was
 * destroyed while a partial download was in progress, the fixture is reset to
 * its deterministic baseline because the one-buffer test backend cannot prove
 * which staged bytes belong to a valid transaction.
 *
 * @param demo SDO block test state.
 * @param co Current CANopenNode object; used to validate dispatcher binding.
 * @return true when the generated OD entry exists and the extension is bound.
 */
bool_t CO_demo_sdo_block_bind(CO_demo_sdo_block_t *demo, CO_t *co);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_SDO_BLOCK_H_ */
