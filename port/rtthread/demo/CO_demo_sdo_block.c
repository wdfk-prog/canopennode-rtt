/**
 * @file CO_demo_sdo_block.c
 * @brief Test-only variable-length DOMAIN backend for SDO server block validation.
 */

#include "CO_demo_sdo_block.h"

#include "OD.h"

#include <string.h>

/** Deterministic non-empty baseline avoids ambiguous zero-length DOMAIN uploads. */
#define CO_DEMO_SDO_BLOCK_BASELINE_BYTE 0x5AU

/** Restore the one-buffer fixture to a complete deterministic baseline. */
static void CO_demo_sdo_block_reset_payload(CO_demo_sdo_block_t *demo)
{
    demo->data[0] = CO_DEMO_SDO_BLOCK_BASELINE_BYTE;
    demo->validLength = 1U;
    demo->writeInProgress = false;
}

/** Read the last completely committed DOMAIN payload. */
static ODR_t CO_demo_sdo_block_read(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead)
{
    CO_demo_sdo_block_t *demo;
    OD_size_t remaining;
    OD_size_t copySize;

    if ((stream == NULL) || (buf == NULL) || (countRead == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    *countRead = 0U;
    demo = (CO_demo_sdo_block_t *)stream->object;
    if (demo == NULL) {
        return ODR_DEV_INCOMPAT;
    }
    if (demo->writeInProgress) {
        /* A prior aborted large download may already have changed staged bytes.
         * Do not expose those bytes as a valid application payload. */
        return ODR_NO_DATA;
    }
    if ((demo->validLength == 0U) || (demo->validLength > CO_DEMO_SDO_BLOCK_MAX_PAYLOAD_SIZE)
        || (stream->dataOffset > demo->validLength)) {
        return ODR_NO_DATA;
    }

    stream->dataLength = demo->validLength;
    remaining = demo->validLength - stream->dataOffset;
    copySize = remaining < count ? remaining : count;
    if (copySize > 0U) {
        (void)memcpy(buf, &demo->data[stream->dataOffset], copySize);
        *countRead = copySize;
        stream->dataOffset += copySize;
    }

    if (stream->dataOffset < demo->validLength) {
        return ODR_PARTIAL;
    }

    stream->dataOffset = 0U;
    return ODR_OK;
}

/** Stage one chunk and commit only when CANopenNode marks the transfer complete. */
static ODR_t CO_demo_sdo_block_write(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
    CO_demo_sdo_block_t *demo;
    OD_size_t endOffset;

    if ((stream == NULL) || (buf == NULL) || (countWritten == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    *countWritten = 0U;
    demo = (CO_demo_sdo_block_t *)stream->object;
    if (demo == NULL) {
        return ODR_DEV_INCOMPAT;
    }
    if ((stream->dataLength > CO_DEMO_SDO_BLOCK_MAX_PAYLOAD_SIZE)
        || (stream->dataOffset > CO_DEMO_SDO_BLOCK_MAX_PAYLOAD_SIZE)
        || (count > (CO_DEMO_SDO_BLOCK_MAX_PAYLOAD_SIZE - stream->dataOffset))) {
        return ODR_DATA_LONG;
    }

    endOffset = stream->dataOffset + count;
    if ((stream->dataLength != 0U) && (endOffset > stream->dataLength)) {
        return ODR_DATA_LONG;
    }
    if (stream->dataOffset == 0U) {
        /* Starting at offset zero begins a new transaction and intentionally
         * supersedes any dirty payload left by an earlier aborted download. */
        demo->writeInProgress = true;
    }

    if (count > 0U) {
        (void)memcpy(&demo->data[stream->dataOffset], buf, count);
        *countWritten = count;
    }

    if ((stream->dataLength == 0U) || (endOffset < stream->dataLength)) {
        stream->dataOffset = endOffset;
        return ODR_PARTIAL;
    }
    if (endOffset == 0U) {
        return ODR_DATA_SHORT;
    }

    demo->validLength = endOffset;
    demo->writeInProgress = false;
    stream->dataOffset = 0U;
    return ODR_OK;
}

void CO_demo_sdo_block_init(CO_demo_sdo_block_t *demo)
{
    if (demo == NULL) {
        return;
    }

    (void)memset(demo, 0, sizeof(*demo));
    /* A one-byte baseline lets the Host save/restore the original fixture with
     * the same block API before it writes the first test payload. */
    CO_demo_sdo_block_reset_payload(demo);
}

bool_t CO_demo_sdo_block_bind(CO_demo_sdo_block_t *demo, CO_t *co)
{
    if ((demo == NULL) || (co == NULL)) {
        return false;
    }

    if (demo->writeInProgress) {
        /* Communication reset destroys the old SDO transaction. The one-buffer
         * test backend cannot distinguish committed bytes from partial staging,
         * so normalize to a known valid value before rebinding. */
        CO_demo_sdo_block_reset_payload(demo);
    }

    demo->extension.object = demo;
    demo->extension.read = CO_demo_sdo_block_read;
    demo->extension.write = CO_demo_sdo_block_write;
    return OD_extension_init(OD_ENTRY_H2304_sdo_server_block_test, &demo->extension) == ODR_OK;
}
