/**
 * @file CO_profile_master_RTT.h
 * @brief Owner-safe CANopenNode RT-Thread transport for remote Device Profile controllers.
 */

#ifndef CO_PROFILE_MASTER_RTT_H_
#define CO_PROFILE_MASTER_RTT_H_

#include <stddef.h>
#include <stdint.h>

#include <rtthread.h>

#include "CANopen.h"
#include "CO_profile_controller.h"
#include "CO_profile_transport.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CANopenNodeRTT;
typedef struct CANopenNodeRTT CANopenNodeRTT;

/** Persistent transport configuration copied during attach. */
typedef struct {
    uint8_t sdoClientIndex; /**< Exclusive SDO client index; index 0 is unavailable while Gateway ASCII SDO is built. */
    uint16_t sdoTimeoutMs; /**< Protocol timeout passed to each remote SDO transaction. */
} CO_profile_master_RTT_config_t;

/** Internal request kind retained in the caller-owned transport runtime. */
typedef enum {
    CO_PROFILE_MASTER_RTT_REQUEST_NONE = 0,
    CO_PROFILE_MASTER_RTT_REQUEST_UPLOAD,
    CO_PROFILE_MASTER_RTT_REQUEST_DOWNLOAD,
    CO_PROFILE_MASTER_RTT_REQUEST_NMT
} CO_profile_master_RTT_request_kind_t;

/** Internal mainline-owned phase for one serialized remote operation. */
typedef enum {
    CO_PROFILE_MASTER_RTT_PHASE_IDLE = 0,
    CO_PROFILE_MASTER_RTT_PHASE_PENDING,
    CO_PROFILE_MASTER_RTT_PHASE_UPLOAD,
    CO_PROFILE_MASTER_RTT_PHASE_DOWNLOAD
} CO_profile_master_RTT_phase_t;

/**
 * @brief Caller-owned profile Master transport runtime.
 *
 * @warning This is source-level runtime storage, not a stable binary ABI. All users must be rebuilt with the same
 *          package headers. One runtime serializes all profile-controller SDO/NMT operations through one SDO client.
 */
typedef struct {
    CANopenNodeRTT *app; /**< Attached CANopenNode RT-Thread application. */
    CO_profile_master_RTT_config_t config; /**< Copied immutable transport policy. */
    CO_t *co; /**< Current communication-generation CANopenNode object. */
    CO_SDOclient_t *client; /**< Current exclusive SDO client selected by @ref config. */

    struct rt_mutex callMutex; /**< Serializes public blocking controller operations. */
    struct rt_mutex requestMutex; /**< Protects request/result publication across caller and mainline threads. */
    struct rt_semaphore completionSem; /**< Wakes the single blocked caller on terminal completion. */
    struct rt_semaphore admissionDrainSem; /**< Wakes teardown after admitted callers leave the IPC lifetime. */
    rt_atomic_t admissionState; /**< Active caller count; negative after teardown atomically closes admission. */

    CO_profile_master_RTT_request_kind_t requestKind; /**< Active/pending operation type. */
    CO_profile_master_RTT_phase_t phase; /**< Mainline-owned operation phase. */
    uint8_t remoteNodeId; /**< Remote server/target Node-ID for the current operation. */
    uint16_t index; /**< Remote SDO index for the current operation. */
    uint8_t subIndex; /**< Remote SDO sub-index for the current operation. */
    uint8_t requestSize; /**< Download size or expected upload size. */
    uint8_t requestData[CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX]; /**< Copied download payload. */
    uint8_t uploadData[CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX]; /**< Bounded upload staging buffer. */
    uint8_t uploadSize; /**< Number of staged upload bytes. */
    rt_bool_t uploadOverflow; /**< True when the remote upload exceeded the expected bounded scalar. */
    CO_NMT_command_t nmtCommand; /**< NMT command when @ref requestKind is NMT. */
    CO_profile_transfer_result_t result; /**< Terminal result published before completionSem release. */
    CO_profile_transport_t portableTransport; /**< Stack-neutral transport view backed by this runtime. */

    rt_bool_t attached; /**< True after lifecycle registration succeeds. */
    rt_bool_t ipcInitialized; /**< True while the four RT-Thread IPC objects are valid. */
    rt_bool_t communicationReady; /**< True while the current CANopen generation is bound and usable. */
    rt_bool_t mainlineReady; /**< True after co_main has serviced the current communication generation. */
} CO_profile_master_RTT_t;

/**
 * @brief Attach one profile Master transport before CANopenNode application initialization.
 *
 * The selected SDO client is exclusive to this transport while the application is running. The attach call creates no
 * RT objects and performs no CANopen access; lifecycle initialization owns the IPC objects and each communication bind
 * resolves the current CANopenNode SDO client again after Communication Reset.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @param runtime Zero-initialized caller-owned transport runtime.
 * @param config SDO client selection and protocol timeout.
 * @return RT_EOK on success, -RT_EINVAL for invalid arguments, -RT_EBUSY after runtime startup/duplicate attach,
 *         or the lifecycle registration error.
 */
rt_err_t CO_profileMasterRTT_attach(CANopenNodeRTT *app, CO_profile_master_RTT_t *runtime,
                                    const CO_profile_master_RTT_config_t *config);

/**
 * @brief Read one bounded scalar from a remote Object Dictionary through the mainline-owned SDO client.
 *
 * The call blocks the calling normal RT-Thread thread until the SDO transaction reaches a terminal result. Admission
 * requires the current communication generation to have been serviced by co_main at least once; callers receive
 * -RT_EBUSY during startup/reset/teardown windows where no mainline owner can complete the request. It must not be
 * called from the CANopen mainline thread, an ISR, or scheduler-locked context. No second caller-side timeout is
 * applied: CANopenNode's SDO protocol timeout owns normal remote timeout completion, while reset/teardown completes the
 * request as CANCELED. The backend configures the selected client with the CiA 301 predefined default SDO COB-IDs
 * derived from @p remoteNodeId; custom SDO COB-ID connections are outside this adapter contract.
 *
 * @param runtime Attached transport runtime.
 * @param remoteNodeId Remote Node-ID in range 1..127.
 * @param index Remote Object Dictionary index.
 * @param subIndex Remote Object Dictionary sub-index.
 * @param expectedSize Exact expected scalar size in bytes, range 1..8.
 * @param result Terminal result. Uploaded bytes are valid only when status is CO_PROFILE_TRANSFER_OK.
 * @return RT_EOK after a terminal result, or an RT-Thread admission/IPC error before a request is accepted.
 */
rt_err_t CO_profileMasterRTT_sdoUpload(CO_profile_master_RTT_t *runtime, uint8_t remoteNodeId,
                                       uint16_t index, uint8_t subIndex, uint8_t expectedSize,
                                       CO_profile_transfer_result_t *result);

/**
 * @brief Write one bounded scalar to a remote Object Dictionary through the mainline-owned SDO client.
 *
 * The input bytes are copied before the caller blocks, so the caller may reuse its buffer after this function returns.
 * The call has the same thread-context and timeout contract as CO_profileMasterRTT_sdoUpload().
 *
 * @param runtime Attached transport runtime.
 * @param remoteNodeId Remote Node-ID in range 1..127.
 * @param index Remote Object Dictionary index.
 * @param subIndex Remote Object Dictionary sub-index.
 * @param data CANopen little-endian encoded scalar bytes.
 * @param size Number of bytes at @p data, range 1..8.
 * @param result Terminal result.
 * @return RT_EOK after a terminal result, or an RT-Thread admission/IPC error before a request is accepted.
 */
rt_err_t CO_profileMasterRTT_sdoDownload(CO_profile_master_RTT_t *runtime, uint8_t remoteNodeId,
                                         uint16_t index, uint8_t subIndex, const void *data, uint8_t size,
                                         CO_profile_transfer_result_t *result);

/**
 * @brief Send one remote NMT command from the CANopen mainline thread.
 *
 * This function is available in every build. When simple NMT Master support is not compiled, the terminal result is
 * LOCAL_ERROR/-RT_ENOSYS. Node-ID zero is accepted as the CANopen NMT broadcast address; 1..127 addresses one node.
 *
 * @param runtime Attached transport runtime.
 * @param command One of the five CiA 301 NMT command values declared by CO_NMT_command_t.
 * @param nodeId Remote Node-ID 1..127 or zero for broadcast.
 * @param result Terminal local dispatch result.
 * @return RT_EOK after a terminal result, -RT_EINVAL for an invalid command/Node-ID, or an RT-Thread
 *         admission/IPC error before a request is accepted.
 */
rt_err_t CO_profileMasterRTT_nmtCommand(CO_profile_master_RTT_t *runtime, CO_NMT_command_t command,
                                        uint8_t nodeId, CO_profile_transfer_result_t *result);

/**
 * @brief Return the portable Controller transport view backed by this RT-Thread runtime.
 *
 * The returned binding remains owned by @p runtime and is valid from successful attach until the runtime storage is
 * discarded. Profile-specific portable clients may retain this pointer; backend readiness is still enforced by each
 * transport operation across Communication Reset and teardown.
 *
 * @param runtime Attached profile Master runtime.
 * @return Portable transport binding, or NULL for invalid/unattached input.
 */
CO_profile_transport_t *CO_profileMasterRTT_transport(CO_profile_master_RTT_t *runtime);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_PROFILE_MASTER_RTT_H_ */
