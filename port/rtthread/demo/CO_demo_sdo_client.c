/**
 * @file CO_demo_sdo_client.c
 * @brief Test-only non-blocking SDO client transaction driver implementation.
 */

#include "CO_demo_sdo_client.h"

#include "OD.h"
#include "co_rtt_log.h"

#include <stddef.h>
#include <string.h>

#if (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE) == 0)
#error "PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST requires CO_CONFIG_SDO_CLI_ENABLE"
#endif /* (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE) == 0) */

#if (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED) == 0)
#error "PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST requires CO_CONFIG_SDO_CLI_SEGMENTED"
#endif /* (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED) == 0) */

#if (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL) == 0)
#error "PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST requires CO_CONFIG_SDO_CLI_LOCAL"
#endif /* (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL) == 0) */

/** Maximum deterministic DOWNLOAD payload accepted by the J04/J06 test wrapper. */
#define CO_DEMO_SDO_CLIENT_MAX_PAYLOAD_SIZE 4096U
/** Request block transfer through CANopenNode when the client feature is compiled. */
#define CO_DEMO_SDO_CLIENT_FLAG_BLOCK 0x01U
/** Request flag mask understood by this test wrapper. */
#define CO_DEMO_SDO_CLIENT_SUPPORTED_FLAGS CO_DEMO_SDO_CLIENT_FLAG_BLOCK
/** Small staging buffer keeps stack use bounded while servicing a 32-byte SDO FIFO. */
#define CO_DEMO_SDO_CLIENT_CHUNK_SIZE 16U
/** FNV-1a 32-bit offset basis used for deterministic payload verification. */
#define CO_DEMO_SDO_CLIENT_FNV_OFFSET 2166136261UL
/** FNV-1a 32-bit prime used for deterministic payload verification. */
#define CO_DEMO_SDO_CLIENT_FNV_PRIME 16777619UL

/** Internal non-blocking transaction phases. */
typedef enum {
    CO_DEMO_SDO_CLIENT_PHASE_IDLE = 0, /**< Waiting for a new request sequence. */
    CO_DEMO_SDO_CLIENT_PHASE_UPLOAD = 1, /**< CANopenNode upload is active. */
    CO_DEMO_SDO_CLIENT_PHASE_DOWNLOAD = 2 /**< CANopenNode download is active. */
} CO_demo_sdo_client_phase_t;

/** Update one FNV-1a checksum with transferred bytes. */
static uint32_t CO_demo_sdo_client_checksum_update(uint32_t checksum, const uint8_t *data, size_t size)
{
    size_t i;

    for (i = 0U; i < size; i++) {
        checksum ^= (uint32_t)data[i];
        checksum *= CO_DEMO_SDO_CLIENT_FNV_PRIME;
    }
    return checksum;
}

/** Return one deterministic payload byte for a seed and absolute offset. */
static uint8_t CO_demo_sdo_client_pattern_byte(uint32_t seed, uint32_t offset)
{
    uint8_t seedByte = (uint8_t)(seed >> ((offset & 0x03U) * 8U));
    return (uint8_t)(seedByte ^ (uint8_t)offset);
}

/** Publish the coherent mainline-owned status snapshot into OD 0x2303. */
static void CO_demo_sdo_client_publish(const CO_demo_sdo_client_t *demo)
{
    OD_RAM.x2303_sdo_client_test.active_seq = demo->activeSeq;
    OD_RAM.x2303_sdo_client_test.complete_seq = demo->completeSeq;
    OD_RAM.x2303_sdo_client_test.result = demo->result;
    OD_RAM.x2303_sdo_client_test.abort_code = (uint32_t)demo->abortCode;
    OD_RAM.x2303_sdo_client_test.transferred_size = demo->transferredSize;
    OD_RAM.x2303_sdo_client_test.result_value = demo->resultValue;
    OD_RAM.x2303_sdo_client_test.checksum = demo->checksum;
}

/** Decode up to four uploaded bytes as a CANopen little-endian U32. */
static uint32_t CO_demo_sdo_client_decode_result_value(const CO_demo_sdo_client_t *demo)
{
    uint32_t value = 0U;
    uint8_t i;

    for (i = 0U; i < demo->resultByteCount; i++) {
        value |= ((uint32_t)demo->resultBytes[i]) << (8U * i);
    }
    return value;
}

/** Finish the active transaction and publish one terminal result. */
static void CO_demo_sdo_client_finish(CO_demo_sdo_client_t *demo, CO_SDOclient_t *client,
                                      CO_demo_sdo_client_result_t result, CO_SDO_abortCode_t abortCode)
{
    if (client != NULL) {
        CO_SDOclientClose(client);
    }
    demo->phase = (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_IDLE;
    demo->result = (int32_t)result;
    demo->abortCode = abortCode;
    demo->checksum = demo->checksumState;
    demo->completeSeq = demo->activeSeq;
    CO_demo_sdo_client_publish(demo);
}

/** Map CANopenNode process completion into the test result contract. */
static void CO_demo_sdo_client_finish_process_error(CO_demo_sdo_client_t *demo, CO_SDOclient_t *client,
                                                    CO_SDO_abortCode_t abortCode)
{
    const CO_demo_sdo_client_result_t result = abortCode == CO_SDO_AB_TIMEOUT
        ? CO_DEMO_SDO_CLIENT_RESULT_TIMEOUT : CO_DEMO_SDO_CLIENT_RESULT_ABORT;
    CO_demo_sdo_client_finish(demo, client, result, abortCode);
}

/** Latch a newly committed OD request before any CANopen operation begins. */
static void CO_demo_sdo_client_latch_request(CO_demo_sdo_client_t *demo, uint32_t requestSeq)
{
    demo->lastConsumedSeq = requestSeq;
    demo->activeSeq = requestSeq;
    demo->command = OD_RAM.x2303_sdo_client_test.command;
    demo->remoteNode = OD_RAM.x2303_sdo_client_test.remote_node;
    demo->index = OD_RAM.x2303_sdo_client_test.index;
    demo->subIndex = OD_RAM.x2303_sdo_client_test.sub_index;
    demo->payloadSize = OD_RAM.x2303_sdo_client_test.payload_size;
    demo->probeValue = OD_RAM.x2303_sdo_client_test.probe_value;
    demo->flags = OD_RAM.x2303_sdo_client_test.flags;
    demo->transferredSize = 0U;
    demo->resultValue = 0U;
    demo->checksum = 0U;
    demo->byteOffset = 0U;
    demo->checksumState = CO_DEMO_SDO_CLIENT_FNV_OFFSET;
    demo->resultByteCount = 0U;
    (void)memset(demo->resultBytes, 0, sizeof(demo->resultBytes));
    demo->result = (int32_t)CO_DEMO_SDO_CLIENT_RESULT_NONE;
    demo->abortCode = CO_SDO_AB_NONE;
    CO_demo_sdo_client_publish(demo);
}

/** Start the latched request using CANopenNode's native SDO client. */
static bool_t CO_demo_sdo_client_start(CO_demo_sdo_client_t *demo, CO_t *co, uint8_t localNodeId)
{
    CO_SDO_return_t sdoRet;
    CO_SDOclient_t *client;
    bool_t blockEnable;

    if ((demo->flags & (uint8_t)~CO_DEMO_SDO_CLIENT_SUPPORTED_FLAGS) != 0U) {
        CO_demo_sdo_client_finish(demo, NULL, CO_DEMO_SDO_CLIENT_RESULT_UNSUPPORTED, CO_SDO_AB_NONE);
        return false;
    }
#if (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK) == 0)
    if ((demo->flags & CO_DEMO_SDO_CLIENT_FLAG_BLOCK) != 0U) {
        CO_demo_sdo_client_finish(demo, NULL, CO_DEMO_SDO_CLIENT_RESULT_UNSUPPORTED, CO_SDO_AB_NONE);
        return false;
    }
#endif /* (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK) == 0) */
    if ((demo->remoteNode == 0U) || (demo->remoteNode > 127U)) {
        CO_demo_sdo_client_finish(demo, NULL, CO_DEMO_SDO_CLIENT_RESULT_SETUP_ERROR, CO_SDO_AB_NONE);
        return false;
    }

    blockEnable = (demo->flags & CO_DEMO_SDO_CLIENT_FLAG_BLOCK) != 0U;
    if ((demo->command != (uint8_t)CO_DEMO_SDO_CLIENT_COMMAND_UPLOAD)
        && (demo->command != (uint8_t)CO_DEMO_SDO_CLIENT_COMMAND_DOWNLOAD)) {
        CO_demo_sdo_client_finish(demo, NULL, CO_DEMO_SDO_CLIENT_RESULT_SETUP_ERROR, CO_SDO_AB_NONE);
        return false;
    }
    if ((demo->command == (uint8_t)CO_DEMO_SDO_CLIENT_COMMAND_DOWNLOAD)
        && ((demo->payloadSize == 0U) || (demo->payloadSize > CO_DEMO_SDO_CLIENT_MAX_PAYLOAD_SIZE))) {
        CO_demo_sdo_client_finish(demo, NULL, CO_DEMO_SDO_CLIENT_RESULT_UNSUPPORTED, CO_SDO_AB_NONE);
        return false;
    }

    client = &co->SDOclient[0];
    sdoRet = CO_SDOclient_setup(client, CO_CAN_ID_SDO_CLI + demo->remoteNode,
                                CO_CAN_ID_SDO_SRV + demo->remoteNode, demo->remoteNode);
    if (sdoRet != CO_SDO_RT_ok_communicationEnd) {
        CO_RTT_LOG_E("SDO client test setup failed: seq=%lu node=%u ret=%d", (unsigned long)demo->activeSeq,
                     demo->remoteNode, (int)sdoRet);
        CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_SETUP_ERROR, CO_SDO_AB_NONE);
        return false;
    }

    if (demo->command == (uint8_t)CO_DEMO_SDO_CLIENT_COMMAND_UPLOAD) {
        sdoRet = CO_SDOclientUploadInitiate(client, demo->index, demo->subIndex,
                                            (uint16_t)PKG_CANOPENNODE_APP_SDO_CLI_TIMEOUT_MS, blockEnable);
        if (sdoRet != CO_SDO_RT_ok_communicationEnd) {
            CO_RTT_LOG_E("SDO client test upload initiate failed: seq=%lu node=%u index=0x%04x:%02x ret=%d",
                         (unsigned long)demo->activeSeq, demo->remoteNode, demo->index, demo->subIndex, (int)sdoRet);
            CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_SETUP_ERROR, CO_SDO_AB_NONE);
            return false;
        }
        demo->phase = (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_UPLOAD;
    } else {
        sdoRet = CO_SDOclientDownloadInitiate(client, demo->index, demo->subIndex, demo->payloadSize,
                                              (uint16_t)PKG_CANOPENNODE_APP_SDO_CLI_TIMEOUT_MS, blockEnable);
        if (sdoRet != CO_SDO_RT_ok_communicationEnd) {
            CO_RTT_LOG_E("SDO client test download initiate failed: seq=%lu node=%u index=0x%04x:%02x ret=%d",
                         (unsigned long)demo->activeSeq, demo->remoteNode, demo->index, demo->subIndex, (int)sdoRet);
            CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_SETUP_ERROR, CO_SDO_AB_NONE);
            return false;
        }
        demo->phase = (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_DOWNLOAD;
    }

    (void)localNodeId;
    CO_RTT_LOG_I("SDO client test started: seq=%lu command=%u local=%u remote=%u index=0x%04x:%02x size=%lu block=%u",
                 (unsigned long)demo->activeSeq, demo->command, localNodeId, demo->remoteNode, demo->index,
                 demo->subIndex, (unsigned long)demo->payloadSize, blockEnable ? 1U : 0U);
    CO_demo_sdo_client_publish(demo);
    return true;
}

/** Drain all currently available upload bytes without blocking. */
static void CO_demo_sdo_client_drain_upload(CO_demo_sdo_client_t *demo, CO_SDOclient_t *client)
{
    uint8_t buffer[CO_DEMO_SDO_CLIENT_CHUNK_SIZE];
    size_t readSize;

    do {
        size_t i;
        readSize = CO_SDOclientUploadBufRead(client, buffer, sizeof(buffer));
        for (i = 0U; i < readSize; i++) {
            if (demo->resultByteCount < sizeof(demo->resultBytes)) {
                demo->resultBytes[demo->resultByteCount++] = buffer[i];
            }
        }
        demo->checksumState = CO_demo_sdo_client_checksum_update(demo->checksumState, buffer, readSize);
        demo->byteOffset += (uint32_t)readSize;
    } while (readSize == sizeof(buffer));
}

/** Process one upload iteration and consume bytes before the SDO FIFO can fill. */
static void CO_demo_sdo_client_process_upload(CO_demo_sdo_client_t *demo, CO_SDOclient_t *client,
                                              uint32_t timeDifferenceUs, uint32_t *timerNextUs)
{
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
    size_t sizeIndicated = 0U;
    size_t sizeTransferred = 0U;
    CO_SDO_return_t sdoRet = CO_SDOclientUpload(client, timeDifferenceUs, false, &abortCode,
                                                &sizeIndicated, &sizeTransferred, timerNextUs);

    if (sdoRet != CO_SDO_RT_blockUploadInProgress) {
        CO_demo_sdo_client_drain_upload(demo, client);
    }
    demo->transferredSize = demo->byteOffset;

    if (sdoRet < CO_SDO_RT_ok_communicationEnd) {
        CO_demo_sdo_client_finish_process_error(demo, client, abortCode);
        return;
    }
    if (sdoRet == CO_SDO_RT_ok_communicationEnd) {
        if ((demo->byteOffset != (uint32_t)sizeTransferred)
            || ((sizeIndicated != 0U) && (demo->byteOffset != (uint32_t)sizeIndicated))) {
            CO_RTT_LOG_E("SDO client test upload size mismatch: seq=%lu read=%lu transferred=%lu indicated=%lu",
                         (unsigned long)demo->activeSeq, (unsigned long)demo->byteOffset,
                         (unsigned long)sizeTransferred, (unsigned long)sizeIndicated);
            CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_INTERNAL_ERROR, CO_SDO_AB_NONE);
            return;
        }
        demo->resultValue = CO_demo_sdo_client_decode_result_value(demo);
        CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_SUCCESS, CO_SDO_AB_NONE);
    }
}

/** Feed one bounded chunk of deterministic DOWNLOAD data into the client FIFO. */
static void CO_demo_sdo_client_fill_download(CO_demo_sdo_client_t *demo, CO_SDOclient_t *client)
{
    uint8_t buffer[CO_DEMO_SDO_CLIENT_CHUNK_SIZE];
    size_t requested;
    size_t written;
    size_t i;

    if (demo->byteOffset >= demo->payloadSize) {
        return;
    }

    requested = (size_t)(demo->payloadSize - demo->byteOffset);
    if (requested > sizeof(buffer)) {
        requested = sizeof(buffer);
    }
    for (i = 0U; i < requested; i++) {
        const uint32_t offset = demo->byteOffset + (uint32_t)i;
        if (demo->payloadSize == sizeof(uint32_t)) {
            buffer[i] = (uint8_t)(demo->probeValue >> (8U * offset));
        } else {
            buffer[i] = CO_demo_sdo_client_pattern_byte(demo->probeValue, offset);
        }
    }

    written = CO_SDOclientDownloadBufWrite(client, buffer, requested);
    demo->checksumState = CO_demo_sdo_client_checksum_update(demo->checksumState, buffer, written);
    demo->byteOffset += (uint32_t)written;
}

/** Process one download iteration, refilling the FIFO as segments are transmitted. */
static void CO_demo_sdo_client_process_download(CO_demo_sdo_client_t *demo, CO_SDOclient_t *client,
                                                uint32_t timeDifferenceUs, uint32_t *timerNextUs)
{
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
    size_t sizeTransferred = 0U;
    CO_SDO_return_t sdoRet;

    CO_demo_sdo_client_fill_download(demo, client);
    sdoRet = CO_SDOclientDownload(client, timeDifferenceUs, false, demo->byteOffset < demo->payloadSize,
                                  &abortCode, &sizeTransferred, timerNextUs);
    demo->transferredSize = (uint32_t)sizeTransferred;

    if (sdoRet < CO_SDO_RT_ok_communicationEnd) {
        CO_demo_sdo_client_finish_process_error(demo, client, abortCode);
        return;
    }
    if (sdoRet == CO_SDO_RT_ok_communicationEnd) {
        if ((demo->byteOffset != demo->payloadSize) || (sizeTransferred != (size_t)demo->payloadSize)) {
            CO_RTT_LOG_E("SDO client test download size mismatch: seq=%lu generated=%lu transferred=%lu expected=%lu",
                         (unsigned long)demo->activeSeq, (unsigned long)demo->byteOffset,
                         (unsigned long)sizeTransferred, (unsigned long)demo->payloadSize);
            CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_INTERNAL_ERROR, CO_SDO_AB_NONE);
            return;
        }
        demo->transferredSize = demo->payloadSize;
        demo->resultValue = demo->payloadSize == sizeof(uint32_t) ? demo->probeValue : 0U;
        CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_SUCCESS, CO_SDO_AB_NONE);
    }
}

void CO_demo_sdo_client_init(CO_demo_sdo_client_t *demo)
{
    if (demo == NULL) {
        return;
    }

    (void)memset(demo, 0, sizeof(*demo));
    demo->phase = (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_IDLE;
    demo->lastConsumedSeq = OD_RAM.x2303_sdo_client_test.request_seq;
    demo->activeSeq = demo->lastConsumedSeq;
    demo->completeSeq = demo->lastConsumedSeq;
    demo->result = (int32_t)CO_DEMO_SDO_CLIENT_RESULT_NONE;
    demo->abortCode = CO_SDO_AB_NONE;
    CO_demo_sdo_client_publish(demo);
}

bool_t CO_demo_sdo_client_bind(CO_demo_sdo_client_t *demo, CO_t *co)
{
    if ((demo == NULL) || (co == NULL) || (co->SDOclient == NULL)) {
        return false;
    }

    /* Communication reset must never replay a request committed to the old stack. */
    demo->phase = (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_IDLE;
    demo->lastConsumedSeq = OD_RAM.x2303_sdo_client_test.request_seq;
    CO_demo_sdo_client_publish(demo);
    return true;
}

void CO_demo_sdo_client_process(CO_demo_sdo_client_t *demo, CO_t *co, uint8_t localNodeId,
                                uint32_t timeDifferenceUs, CO_NMT_reset_cmd_t resetStatus,
                                uint32_t *timerNextUs)
{
    CO_SDOclient_t *client;
    uint32_t requestSeq;

    if ((demo == NULL) || (co == NULL) || (co->SDOclient == NULL)) {
        return;
    }
    client = &co->SDOclient[0];

    /* CO_process() has already reported the reset while the old SDO client is
     * still valid. Close it now, publish cancellation, and consume the request
     * before the runtime recreates the CANopenNode object. */
    if (resetStatus != CO_RESET_NOT) {
        if (demo->phase != (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_IDLE) {
            CO_demo_sdo_client_finish(demo, client, CO_DEMO_SDO_CLIENT_RESULT_RESET_CANCELLED, CO_SDO_AB_NONE);
        }
        demo->lastConsumedSeq = OD_RAM.x2303_sdo_client_test.request_seq;
        CO_demo_sdo_client_publish(demo);
        return;
    }

    if (demo->phase == (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_IDLE) {
        requestSeq = OD_RAM.x2303_sdo_client_test.request_seq;
        if (requestSeq == demo->lastConsumedSeq) {
            return;
        }
        CO_demo_sdo_client_latch_request(demo, requestSeq);
        if (!CO_demo_sdo_client_start(demo, co, localNodeId)) {
            return;
        }
    }

    if (demo->phase == (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_UPLOAD) {
        CO_demo_sdo_client_process_upload(demo, client, timeDifferenceUs, timerNextUs);
    } else if (demo->phase == (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_DOWNLOAD) {
        CO_demo_sdo_client_process_download(demo, client, timeDifferenceUs, timerNextUs);
    }

    CO_demo_sdo_client_publish(demo);
}

void CO_demo_sdo_client_reset(CO_demo_sdo_client_t *demo)
{
    if (demo == NULL) {
        return;
    }

    /* The normal reset path publishes cancellation in process() while the old
     * client is valid. This fallback still consumes the request if reset occurs
     * during a runtime rollback before another mainline iteration is possible. */
    if (demo->phase != (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_IDLE) {
        demo->phase = (uint8_t)CO_DEMO_SDO_CLIENT_PHASE_IDLE;
        demo->result = (int32_t)CO_DEMO_SDO_CLIENT_RESULT_RESET_CANCELLED;
        demo->abortCode = CO_SDO_AB_NONE;
        demo->checksum = demo->checksumState;
        demo->completeSeq = demo->activeSeq;
    }
    demo->lastConsumedSeq = OD_RAM.x2303_sdo_client_test.request_seq;
    CO_demo_sdo_client_publish(demo);
}
