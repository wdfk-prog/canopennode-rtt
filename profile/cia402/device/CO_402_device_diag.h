/**
 * @file CO_402_device_diag.h
 * @brief Pure-C per-axis CiA 402 diagnostic latch and deferred event contract.
 *
 * Diagnostics deliberately stops at an axis-local pending event. CANopen EMCY
 * publication belongs to the platform adapter after the OD lock is released.
 */

#ifndef CO_402_DEVICE_DIAG_H
#define CO_402_DEVICE_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#ifndef CO_402_CONFIG_DIAGNOSTICS
#define CO_402_CONFIG_DIAGNOSTICS 0
#endif /* !defined(CO_402_CONFIG_DIAGNOSTICS) */

/** First manufacturer-specific CANopenNode error-status bit reserved for local CiA 402 axes. */
#define CO_402_DEVICE_DIAG_ERROR_BIT_BASE 0x40U

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CO_402_device;
struct CO_402_device_axis;

/** Origin which transferred one local axis into fault ownership. */
typedef enum {
    CO_402_FAULT_ORIGIN_PRODUCT = 0,   /**< Product/hardware fault reported by the product diagnostic source. */
    CO_402_FAULT_ORIGIN_CONTROLWORD_OD,/**< Controlword could not be sampled from the generated OD. */
    CO_402_FAULT_ORIGIN_PDS_OPERATION, /**< A non-blocking PDS DriveIF transition returned ERROR. */
    CO_402_FAULT_ORIGIN_MODE_OPERATION,/**< Mode entry/exit/active-mode processing returned ERROR. */
    CO_402_FAULT_ORIGIN_FAULT_RESET,   /**< The accepted Fault Reset DriveIF transaction returned ERROR. */
    CO_402_FAULT_ORIGIN_INTERNAL_STATE /**< The supervisor detected an invalid internal PDS state. */
} CO_402_device_fault_origin_t;

/** One product-owned mapping from a fault origin to OD and CANopen EMCY semantics. */
typedef struct {
    uint16_t pdsErrorCode; /**< Value written to this logical device's Error code object (0x603F + offset). */
    uint16_t emcyCode;     /**< CANopen EMCY error code passed unchanged to CO_errorReport(). */
    uint32_t infoCode;     /**< Product/manufacturer detail placed in EMCY bytes 4..7. */
} CO_402_device_fault_info_t;

/**
 * @brief Product diagnostic source and fault-code mapper.
 *
 * For CO_402_FAULT_ORIGIN_PRODUCT, return true only while a product fault is
 * currently present and fill @p info. For every internal origin used by the
 * profile, return true with the product-approved mapping. Returning false for
 * an internal origin records a diagnostic contract failure; the PDS still
 * remains fail-closed and the core never invents a fallback error code.
 *
 * The callback runs synchronously from the Device supervisor while the platform
 * may hold lifecycle/OD locks. It must be bounded and non-blocking, and must not
 * re-enter CANopen OD or EMCY APIs; EMCY publication is deferred to the adapter.
 */
typedef struct {
    bool (*getFaultInfo)(void *object, CO_402_device_fault_origin_t origin,
                         CO_402_device_fault_info_t *info);
} CO_402_device_diag_if_t;

/** Deferred event type consumed by a platform EMCY bridge. */
typedef enum {
    CO_402_DIAG_EVENT_NONE = 0,
    CO_402_DIAG_EVENT_REPORT,
    CO_402_DIAG_EVENT_RESET
} CO_402_device_diag_event_type_t;

/** One axis-local deferred diagnostic event. */
typedef struct {
    CO_402_device_diag_event_type_t type; /**< REPORT or RESET. */
    uint8_t logicalDevice;                /**< Zero-based local logical device/axis source. */
    uint8_t errorBit;                     /**< Manufacturer-specific CANopenNode status bit for this axis. */
    CO_402_device_fault_info_t fault;     /**< Latched report mapping, retained for the corresponding reset. */
} CO_402_device_diag_event_t;

/** Persistent per-axis diagnostic state retained across CANopen Communication Reset. */
typedef struct {
    bool active;                          /**< True while one fault remains latched until successful Fault Reset. */
    bool contractFailed;                  /**< True after an unmapped/invalid fault or Error-code OD write failure. */
    CO_402_device_fault_origin_t origin;  /**< Origin associated with the currently active fault. */
    CO_402_device_fault_info_t fault;     /**< First active fault snapshot for this axis. */
    bool pending;                         /**< True while @ref event still awaits platform consumption. */
    CO_402_device_diag_event_t event;     /**< Single-slot edge event; supervisor flushes once per worker pass. */
} CO_402_device_diag_runtime_t;

#if CO_402_CONFIG_DIAGNOSTICS

/**
 * @brief Check whether a diagnostic interface provides its mandatory callback.
 * @param diag Product diagnostic interface to validate.
 * @return true when @p diag can map/poll faults; false otherwise.
 */
bool CO_402_device_diag_ifValid(const CO_402_device_diag_if_t *diag);

/**
 * @brief Return the manufacturer-specific CANopenNode error-status bit for one axis.
 * @param logicalDevice Zero-based logical-device number, already validated by the Device manager.
 * @return Error-status bit encoded as 0x40 + @p logicalDevice.
 */
uint8_t CO_402_device_diag_errorBit(uint8_t logicalDevice);

/**
 * @brief Poll the product-owned source for a currently present physical/application fault.
 * @param axis Axis whose DiagIF is polled.
 * @param fault Output mapping filled when a product fault is currently present.
 * @return true only when a fault was reported. Active or contract-failed axes are not polled again.
 */
bool CO_402_device_diag_pollProductFault(struct CO_402_device_axis *axis,
                                         CO_402_device_fault_info_t *fault);

/**
 * @brief Latch the first active fault, write the axis Error code and queue one REPORT event.
 *
 * @p fault may provide an already sampled product fault. When NULL, the axis
 * DiagIF maps @p origin. Repeated calls while a fault is active are suppressed
 * so one CANopenNode error-status bit cannot silently change meaning mid-fault.
 *
 * @param axis Axis whose first active diagnostic is latched.
 * @param origin Fault path that acquired fault ownership.
 * @param fault Optional already sampled product mapping; NULL requests DiagIF mapping.
 * @return true when an active fault is already present or the new fault was
 *         latched successfully; false on a diagnostic contract failure.
 */
bool CO_402_device_diag_latch(struct CO_402_device_axis *axis,
                              CO_402_device_fault_origin_t origin,
                              const CO_402_device_fault_info_t *fault);

/**
 * @brief Clear this axis Error code and queue RESET for the currently active diagnostic, if any.
 * @param axis Axis completing its accepted Fault Reset transaction.
 * @return true when the externally visible diagnostic state was cleared; false on a sticky diagnostic-contract failure.
 */
bool CO_402_device_diag_clear(struct CO_402_device_axis *axis);

/**
 * @brief Restore one axis Error code after OD binding and optionally queue an active-fault replay.
 * @param axis Axis whose generated Error-code object has just been rebound.
 * @param replayActive Queue REPORT when the retained diagnostic is active.
 * @return true when the generated Error-code object accepted the restore value.
 */
bool CO_402_device_diag_restoreAxis(struct CO_402_device_axis *axis, bool replayActive);

/**
 * @brief Consume one pending axis event.
 * @param axis Axis whose pending slot is consumed.
 * @param event Output event copied before the pending slot is cleared.
 * @return true when an event was consumed; false when no event is pending or an argument is invalid.
 *
 * The caller must serialize access with the Device supervisor.
 */
bool CO_402_device_diag_takePendingEvent(struct CO_402_device_axis *axis,
                                         CO_402_device_diag_event_t *event);

#endif /* CO_402_CONFIG_DIAGNOSTICS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEVICE_DIAG_H */
