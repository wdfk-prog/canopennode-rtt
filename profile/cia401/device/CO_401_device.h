/**
 * @file CO_401_device.h
 * @brief Public Pure-C CiA 401 generic I/O Device core.
 *
 * The core owns no heap memory and has no RT-Thread or BSP dependency. Generated
 * Object Dictionary entries are the source of truth and must exist before the
 * runtime binds them. The mandatory core supports 8-bit digital and 16-bit
 * analogue process images; optional digital/analogue profile semantics are selected by Kconfig.
 */

#ifndef CO_401_DEVICE_H
#define CO_401_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "301/CO_ODinterface.h"
#include "CO_401_defs.h"
#include "CO_401_device_od.h"
#include "CO_401_io.h"
#include "CO_401_objects.h"
#include "CO_401_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fail-closed result of Device configuration and generated-OD validation. */
typedef enum {
    CO_401_INIT_OK = 0,
    CO_401_INIT_BAD_ARGUMENT,
    CO_401_INIT_CONFIG,
    CO_401_INIT_IO_IF,
    CO_401_INIT_OD_MISSING,
    CO_401_INIT_OD_UNEXPECTED,
    CO_401_INIT_OD_TYPE,
    CO_401_INIT_OD_LENGTH,
    CO_401_INIT_OD_ACCESS,
    CO_401_INIT_OD_SUB_COUNT,
    CO_401_INIT_OD_SUB_VALUE,
    CO_401_INIT_DEVICE_TYPE
} CO_401_init_error_t;

/** Location associated with a CiA 401 initialization failure. */
typedef struct {
    CO_401_init_error_t error;
    uint16_t index; /**< Absolute OD index associated with the failure, after logical-device translation. */
    uint8_t subIndex; /**< OD sub-index associated with the failure. */
    uint8_t logicalDevice; /**< Zero-based logical-device slot associated with the failure. */
} CO_401_init_diag_t;

/** Immutable product configuration copied into the Device runtime during initialization. */
typedef struct {
    const CO_401_io_if_t *io; /**< Persistent callback table for the enabled capabilities. */
    void *ioObject;           /**< Product-owned callback context. */
    uint8_t digitalInputBanks;   /**< Number of canonical 0x6000 8-bit banks; zero disables the capability.
                                   *   With digital events enabled, the highest sub-index must fit
                                   *   in CANopenNode's OD_FLAGS_PDO_SIZE request bitmap. */
    uint8_t digitalOutputBanks;  /**< Number of canonical 0x6200 8-bit banks; zero disables the capability. */
    uint8_t analogInputChannels; /**< Number of canonical 0x6401 INTEGER16 channels; zero disables the capability. */
    uint8_t analogOutputChannels; /**< Number of canonical 0x6411 INTEGER16 channels; zero disables the capability. */
    uint8_t logicalDevice; /**< Zero-based CiA 301 slot selecting the 0x6000 + slot*0x800 profile block. */
} CO_401_device_config_t;

/**
 * @brief Caller-owned CiA 401 runtime state.
 *
 * @warning This structure is source-level runtime storage, not a stable cross-version binary ABI.
 *          All allocators and users must be rebuilt with the same header and feature macros when
 *          the package version or CiA 401 configuration changes.
 */
typedef struct {
    OD_t *od;                    /**< Generated Object Dictionary supplied by the application. */
    CO_401_device_config_t config; /**< Copied immutable process-image/I/O configuration. */
    CO_401_capabilities_t capabilities; /**< Capability set derived from non-zero configured counts. */
    CO_401_device_od_t bound;    /**< Cached generated-OD entries after a successful bind. */
    bool odBound;                /**< True only while the complete enabled OD contract is valid. */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    OD_extension_t digitalInput8Extension; /**< TPDO request flags for Object 0x6000. */
    OD_extension_t digitalInputFilter8Extension; /**< Forwarding hook for Object 0x6003 writes. */
    bool digitalInputFilterDirty; /**< Product filter bridge must be refreshed on the next process pass. */
    /** Event-triggered 0x6000 retry bits, retained until a matching TPDO is accepted by transport. */
    uint8_t digitalInputEventTpdoPending[(OD_FLAGS_PDO_SIZE > 0U) ? OD_FLAGS_PDO_SIZE : 1U];
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    OD_extension_t digitalOutput8Extension; /**< OD I/O hook retained for Object 0x6200 mapped writes. */
    bool digitalOutputFaultActive; /**< Product/internal fault source for 0x6206/0x6207 fail-safe processing. */
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
    bool nmtOperational; /**< Previous/current NMT Operational state supplied by the lifecycle bridge. */
    bool errorEventPending; /**< One bounded profile error/warning event is waiting for the bridge. */
    uint16_t pendingErrorCode; /**< CiA profile emergency/error code for the pending bridge event. */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    OD_extension_t analogInput16Extension; /**< TPDO request and SDO-read hook for Object 0x6401. */
    OD_extension_t analogInterruptSourceExtension; /**< SDO-read hook for Object 0x6422. */
    int16_t analogLastCommunicated[CO_401_PROCESS_IMAGE_COUNT_MAX]; /**< Delta-event reference values. */
    bool analogLastCommunicatedValid[CO_401_PROCESS_IMAGE_COUNT_MAX]; /**< Valid reference flags. */
    /** Event-triggered 0x6401 retry bits, sized to the configured OD TPDO-request capacity. */
    uint8_t analogInputEventTpdoPending[(OD_FLAGS_PDO_SIZE > 0U) ? OD_FLAGS_PDO_SIZE : 1U];
    void *sdoReadMatchObject; /**< Transport-owned context used only to classify SDO OD reads. */
    bool (*sdoReadMatch)(void *object, const OD_stream_t *stream); /**< True only for an active SDO server read. */
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    bool analogOutputFaultActive; /**< Product/internal fault source for 0x6443/0x6444 fail-safe processing. */
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    bool nmtStopped; /**< Independent Stop Remote Node fault source; never overwrites product/internal faults. */
#endif
#if !defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    OD_extension_t digitalOutput8Extension; /**< Supervision gate for network writes to Object 0x6200. */
#endif /* !PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
    OD_extension_t analogOutput16Extension; /**< Supervision gate for network writes to Object 0x6411. */
    bool outputSupervisionReady; /**< True after the first qualifying Heartbeat or node-guard supervision event. */
    void *outputSupervisionProbeObject; /**< Transport-owned context for the synchronous startup-gate probe. */
    bool (*outputSupervisionProbe)(void *object); /**< True after the qualifying supervision event is observable. */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    bool communicationFaultActive; /**< Adapter-owned bus-off / Heartbeat / life-guard output-fault source. */
    bool failSafeOutputApplyComplete; /**< True when every required fail-safe backend write completed this pass. */
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
    uint8_t logicalDevice; /**< Zero-based logical-device slot owning this CiA 401 runtime. */
    uint16_t odBase; /**< Resolved base of this logical device's standardized application-profile block. */
} CO_401_device_t;

/**
 * @brief Initialize one local CiA 401 generic I/O Device and bind its generated OD.
 *
 * @param device Caller-owned runtime storage.
 * @param od Generated CANopenNode Object Dictionary.
 * @param config Product capability and IOIF configuration.
 * @param diag Optional failure location.
 * @return CO_401_INIT_OK on success; otherwise the runtime remains fail-closed.
 */
CO_401_init_error_t CO_401_device_init(CO_401_device_t *device, OD_t *od,
                                        const CO_401_device_config_t *config, CO_401_init_diag_t *diag);

/**
 * @brief Validate and cache the complete enabled generated-OD contract.
 *
 * Validation is transactional: the public cache is replaced only after every
 * enabled/disabled capability, the selected standalone/multi-device Device type
 * contract and every required ARRAY/VAR contract has passed. Logical-device slots translate
 * canonical 0x6000..0x67FF indices by 0x800 per slot; the global 0x1000 object is never translated.
 * Enabled digital/analogue event options also install OD extensions used for mapped-write handling,
 * SDO-read classification and TPDO requests. Analogue
 * SDO state commits when the SDO server completes its OD read; TPDO state commits only
 * after successful transport submission. Binding must therefore complete
 * before PDO initialization caches OD extension I/O and TPDO flags.
 *
 * @param device Initialized Device runtime containing OD/config information.
 * @param diag Optional failure location.
 * @return CO_401_INIT_OK when the generated OD exactly matches the configuration.
 */
CO_401_init_error_t CO_401_device_bindOD(CO_401_device_t *device, CO_401_init_diag_t *diag);

/**
 * @brief Latch the CiA 401 output-supervision prerequisite as established.
 *
 * After power-on or NMT Reset Application, network writes to 0x6200/0x6411 are
 * rejected with ODR_DATA_DEV_STATE until the first Heartbeat from the device
 * that sets the outputs, or until the NMT master node-guards this device for the
 * first time. This state intentionally survives NMT Reset Communication and is
 * cleared only by reinitializing the Device runtime for application reset.
 *
 * The caller must serialize this state change with OD access and
 * CO_401_device_process().
 *
 * @param device Device runtime; NULL is ignored.
 */
void CO_401_device_notifyOutputSupervision(CO_401_device_t *device);

/**
 * @brief Configure the bounded probe used by the 0x6200/0x6411 startup write gate.
 *
 * While the output-supervision latch is still false, the OD output-write hook calls
 * @p probe synchronously before deciding whether to reject the current network write.
 * Returning true latches supervision immediately, so a qualifying Heartbeat/node-guard
 * event that happened before the write is visible without waiting for a profile worker.
 *
 * The probe may execute while CANopenNode already owns the OD lock and may be sampled
 * concurrently by transport integration. It must therefore be read-only/reentrant,
 * bounded and non-blocking, must not acquire the OD/lifecycle locks, and must not
 * re-enter the Device/adapter. The producer is responsible for making its monotonic
 * "first supervision observed" state safely visible across contexts.
 * Passing NULL clears the probe but does not clear an already latched ready state.
 *
 * @param device Device runtime; NULL is ignored.
 * @param object Transport/product-owned probe context, or NULL when clearing.
 * @param probe Function returning true once the qualifying event has been processed; NULL clears it.
 */
void CO_401_device_setOutputSupervisionProbe(CO_401_device_t *device, void *object,
                                              bool (*probe)(void *object));

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
/**
 * @brief Select or clear the CiA 401 digital-output fault state.
 *
 * When active, 0x6206 selects per-bit keep-current versus 0x6207 error-value
 * behavior. Object 0x6200 remains the logical command image and is not replaced
 * by the fail-safe value. The product masked-write callback preserves physical
 * keep-current bits. This setter owns only the product/internal fault source;
 * NMT Stop is tracked independently by CO_401_device_setNmtStopped().
 *
 * The caller must serialize this state change with CO_401_device_process() and
 * direct output-helper calls.
 *
 * @param device Device runtime; NULL is ignored.
 * @param active True while digital outputs shall use fault behavior.
 */
void CO_401_device_setDigitalOutputFault(CO_401_device_t *device, bool active);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
/**
 * @brief Set the independent NMT Stop Remote Node output-fault source.
 *
 * Output fail-safe processing is active while this source or the corresponding
 * product/internal fault source is active. Clearing NMT Stop therefore never
 * clears a product fault owned by CO_401_device_setDigitalOutputFault() or
 * CO_401_device_setAnalogOutputFault().
 *
 * The caller must serialize this state change with CO_401_device_process() and
 * direct output-helper calls.
 *
 * @param device Device runtime; NULL is ignored.
 * @param stopped True while the node is in NMT Stopped state.
 */
void CO_401_device_setNmtStopped(CO_401_device_t *device, bool stopped);

/**
 * @brief Set the independent communication-fault output-fail-safe source.
 *
 * The RT adapter uses this source for CiA 401 communication/device failures
 * such as CAN bus-off and Heartbeat/life-guard timeout. It is ORed with NMT Stop
 * and product/internal fault sources, so clearing one owner never clears another.
 *
 * The caller must serialize this state change with CO_401_device_process() and
 * direct output-helper calls.
 *
 * @param device Device runtime; NULL is ignored.
 * @param active True while the adapter-owned communication fault is active.
 */
void CO_401_device_setCommunicationFault(CO_401_device_t *device, bool active);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */


/** Standard Pure-C profile error/warning bridge event consumed by an application adapter. */
typedef struct {
    uint16_t errorCode; /**< CiA profile emergency/error code, including the 0x0080 disabled-input warning. */
} CO_401_error_event_t;

/**
 * @brief Notify the profile of the current NMT Operational state.
 *
 * A false-to-true transition checks Object 0x6423. When analogue interrupts are
 * globally disabled, one 0x0080 warning is latched for the external EMCY bridge.
 * Repeated calls with @p operational true do not enqueue duplicate warnings.
 *
 * The caller must serialize this function with CO_401_device_process() and OD access.
 *
 * @param device Device runtime; NULL is ignored.
 * @param operational True while the node is in NMT Operational state.
 */
void CO_401_device_setNmtOperational(CO_401_device_t *device, bool operational);

/**
 * @brief Pop one pending CiA 401 profile error/warning event.
 *
 * The bounded single-slot bridge prevents an unconsumed warning from growing an
 * internal queue. The caller must serialize this operation with producers of the
 * profile event state.
 *
 * @param device Device runtime.
 * @param event Destination for the pending profile event.
 * @return true when one event was returned and consumed; false for invalid arguments
 *         or when no event is pending.
 */
bool CO_401_device_takeErrorEvent(CO_401_device_t *device, CO_401_error_event_t *event);

/**
 * @brief Invalidate communication-generation-local NMT warning state.
 *
 * Communication Reset must call this after stopping current-generation profile
 * work and before publishing the next generation. The old NMT edge and an
 * unconsumed 0x0080 warning belong to the retired generation and are discarded,
 * so the next Operational observation is evaluated as a fresh transition.
 *
 * @param device Device runtime; NULL is ignored.
 */
void CO_401_device_resetCommunicationState(CO_401_device_t *device);

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
/**
 * @brief Retire one successfully submitted event-driven 0x6000 TPDO retry.
 *
 * Failed submissions leave the retry marker set so the next process pass reasserts
 * OD_requestTPDO(). Call this only after transport accepts a TPDO that maps the
 * corresponding digital-input sub-index.
 *
 * The caller must serialize this operation with CO_401_digital_refreshInputs(),
 * CO_401_device_process(), and any other access to this Device runtime. A TX ISR or
 * asynchronous transport callback must not retire the marker concurrently with input refresh.
 *
 * @param device Device runtime; NULL is ignored.
 * @param subIndex Object 0x6000 sub-index in range 1..digitalInputBanks.
 */
void CO_401_device_retireDigitalInputTpdoEvent(CO_401_device_t *device, uint8_t subIndex);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
/**
 * @brief Configure the transport classifier used to recognize SDO-originated OD reads.
 *
 * The Pure-C core never infers the caller from thread identity or CAN payloads. The transport
 * integration supplies a matcher for its active SDO-server OD_IO streams. Local OD_get_*()
 * reads and TPDO serialization therefore remain side-effect free, while expedited, segmented
 * and block SDO uploads share the same read-completion path.
 *
 * The caller must serialize updates with SDO/PDO processing and OD access.
 *
 * @param device Device runtime; NULL is ignored.
 * @param object Transport-owned matcher context, or NULL when clearing.
 * @param matcher Function returning true only when @p stream belongs to an active SDO read; NULL clears the matcher.
 */
void CO_401_device_setSdoReadMatcher(CO_401_device_t *device, void *object,
                                     bool (*matcher)(void *object, const OD_stream_t *stream));

/**
 * @brief Commit a successfully communicated 0x6401 analogue-input value.
 *
 * SDO reads use this function directly when their OD read completes. A successful
 * TPDO must use CO_401_device_commitAnalogInputTpdoCommunication() so the event-driven
 * TPDO retry marker is retired only after transport acceptance. Local OD reads and
 * PDO serialization alone must not call either commit function.
 *
 * The caller must serialize this function with CO_401_device_process() and OD access.
 *
 * @param device Device runtime; NULL is ignored.
 * @param subIndex Object 0x6401 sub-index in range 1..analogInputChannels.
 * @param value Exact INTEGER16 value that was communicated.
 */
void CO_401_device_commitAnalogInputCommunication(CO_401_device_t *device, uint8_t subIndex, int16_t value);

/**
 * @brief Retire one successfully submitted event-driven 0x6401 TPDO retry.
 *
 * Use this when the accepted TPDO maps only part of the INTEGER16 object. The
 * event retry is transport state and may be retired after successful submission,
 * but an incomplete wire value must not become the CiA 401 last communicated
 * value used by the delta-event objects.
 *
 * The caller must serialize this function with CO_401_device_process() and OD access.
 *
 * @param device Device runtime; NULL is ignored.
 * @param subIndex Object 0x6401 sub-index in range 1..analogInputChannels.
 */
void CO_401_device_retireAnalogInputTpdoEvent(CO_401_device_t *device, uint8_t subIndex);

/**
 * @brief Commit one successfully submitted 0x6401 TPDO value and retire its event retry.
 *
 * Call this only after the transport accepts the exact TPDO frame. Failed submission
 * leaves the retry marker set so the next process pass requests the event-driven TPDO
 * again. Object 0x6422 remains independently latched until it is read by SDO or
 * transmitted by PDO.
 *
 * The caller must serialize this function with CO_401_device_process() and OD access.
 *
 * @param device Device runtime; NULL is ignored.
 * @param subIndex Object 0x6401 sub-index in range 1..analogInputChannels.
 * @param value Exact INTEGER16 value contained in the accepted TPDO.
 */
void CO_401_device_commitAnalogInputTpdoCommunication(CO_401_device_t *device, uint8_t subIndex, int16_t value);

/**
 * @brief Consume the 0x6422 source bits contained in one successful network transfer.
 *
 * Only bits present in @p communicatedBits are cleared, so an event latched after
 * the communicated snapshot is preserved. SDO reads consume the snapshot at OD-read
 * completion; TPDO consumes it only after successful transmission. The caller must
 * serialize this function with CO_401_device_process() and OD access.
 *
 * @param device Device runtime; NULL is ignored.
 * @param subIndex Object 0x6422 source-bank sub-index.
 * @param communicatedBits Exact source-bit snapshot contained in the transfer.
 */
void CO_401_device_commitAnalogSourceCommunication(CO_401_device_t *device, uint8_t subIndex,
                                                    uint32_t communicatedBits);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
/**
 * @brief Select or clear the CiA 401 analogue-output device-failure state.
 *
 * When active, Object 0x6443 selects keep-current or the Object 0x6444 error value
 * per channel. Error values bypass normal offset/scaling and Object 0x6411 remains
 * the unchanged command image. This setter owns only the product/internal fault
 * source; NMT Stop is tracked independently by CO_401_device_setNmtStopped().
 *
 * The caller must serialize this state change with CO_401_device_process() and
 * direct output-helper calls.
 *
 * @param device Device runtime; NULL is ignored.
 * @param active True while analogue outputs shall use fault behavior.
 */
void CO_401_device_setAnalogOutputFault(CO_401_device_t *device, bool active);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

/**
 * @brief Execute one bounded process-image exchange with the product IOIF.
 *
 * The caller must serialize this function against concurrent SDO/PDO access to
 * the same mapped OD entries. IOIF callbacks execute inside that caller-owned
 * serialization window and must not recursively acquire it.
 *
 * Successful DI/AI reads refresh 0x6000/0x6401. DO/AO command values are read
 * from 0x6200/0x6411 and offered to the backend on each pass. Network writes to
 * those output objects remain blocked until CO_401_device_notifyOutputSupervision()
 * establishes the CiA 401 startup prerequisite. BUSY/ERROR input operations
 * preserve the last OD input image; output commands remain in the OD and are
 * retried on later passes. Optional Stage-2 digital input events apply
 * 0x6002/0x6003/0x6005..0x6008 and request TPDO transmission through the mapped
 * 0x6000 OD entry. Optional digital output semantics keep the full 0x6200 command
 * image, then apply 0x6206/0x6207, 0x6202 and finally the 0x6208 mask to the
 * physical output path. While any fail-safe source is active,
 * failSafeOutputApplyComplete is true only when every required fail-safe backend
 * write completed with CO_401_IO_OK during this pass; keep-current/filter no-op
 * selections require no backend write and therefore remain complete.
 *
 * @param device Successfully bound Device runtime.
 */
void CO_401_device_process(CO_401_device_t *device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DEVICE_H */
