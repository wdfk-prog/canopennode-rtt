/**
 * @file CO_402_device_diag.c
 * @brief Pure-C per-axis CiA 402 diagnostic latch and deferred event implementation.
 */

#include <string.h>

#include "CO_402_device.h"
#include "CO_402_device_diag.h"
#include "CO_402_log.h"

#if CO_402_CONFIG_DIAGNOSTICS

static bool CO_402_device_diag_faultInfoValid(const CO_402_device_fault_info_t *fault)
{
    return fault != NULL && fault->pdsErrorCode != 0U && fault->emcyCode != 0U;
}

static void CO_402_device_diag_contractFailure(CO_402_device_axis_t *axis,
                                                CO_402_device_fault_origin_t origin,
                                                const char *reason)
{
    if (axis == NULL) {
        return;
    }

    axis->diagnosis.contractFailed = true;
    (void)origin;
    (void)reason;
    CO_402_LOG_E("CiA402 diagnostic contract failure: axis=%u origin=%u reason=%s",
                 (unsigned int)axis->logicalDevice, (unsigned int)origin,
                 reason != NULL ? reason : "unknown");
}

bool CO_402_device_diag_ifValid(const CO_402_device_diag_if_t *diag)
{
    return diag != NULL && diag->getFaultInfo != NULL;
}

uint8_t CO_402_device_diag_errorBit(uint8_t logicalDevice)
{
    return (uint8_t)(CO_402_DEVICE_DIAG_ERROR_BIT_BASE + logicalDevice);
}

bool CO_402_device_diag_pollProductFault(CO_402_device_axis_t *axis,
                                         CO_402_device_fault_info_t *fault)
{
    if (axis == NULL || fault == NULL || axis->diagnosis.active
        || axis->diagnosis.contractFailed || !CO_402_device_diag_ifValid(axis->diag)) {
        return false;
    }

    memset(fault, 0, sizeof(*fault));
    return axis->diag->getFaultInfo(axis->diagObject, CO_402_FAULT_ORIGIN_PRODUCT, fault);
}

bool CO_402_device_diag_latch(CO_402_device_axis_t *axis,
                              CO_402_device_fault_origin_t origin,
                              const CO_402_device_fault_info_t *fault)
{
    CO_402_device_fault_info_t mappedFault;
    const CO_402_device_fault_info_t *selectedFault = fault;

    if (axis == NULL) {
        return false;
    }
    if (axis->diagnosis.active) {
        return true;
    }

    if (selectedFault == NULL) {
        memset(&mappedFault, 0, sizeof(mappedFault));
        if (!CO_402_device_diag_ifValid(axis->diag)
            || !axis->diag->getFaultInfo(axis->diagObject, origin, &mappedFault)) {
            CO_402_device_diag_contractFailure(axis, origin, "fault mapping unavailable");
            return false;
        }
        selectedFault = &mappedFault;
    }

    if (!CO_402_device_diag_faultInfoValid(selectedFault)) {
        CO_402_device_diag_contractFailure(axis, origin, "fault mapping contains zero error code");
        return false;
    }
    if (axis->od.errorCode == NULL
        || OD_set_u16(axis->od.errorCode, 0U, selectedFault->pdsErrorCode, true) != ODR_OK) {
        CO_402_device_diag_contractFailure(axis, origin, "axis Error code OD write failed");
        return false;
    }

    axis->diagnosis.active = true;
    axis->diagnosis.origin = origin;
    axis->diagnosis.fault = *selectedFault;
    axis->diagnosis.pending = true;
    axis->diagnosis.event.type = CO_402_DIAG_EVENT_REPORT;
    axis->diagnosis.event.logicalDevice = axis->logicalDevice;
    axis->diagnosis.event.errorBit = CO_402_device_diag_errorBit(axis->logicalDevice);
    axis->diagnosis.event.fault = *selectedFault;

    CO_402_LOG_I("CiA402 diagnostic latched: axis=%u origin=%u pds=0x%04x emcy=0x%04x info=0x%08lx",
                 (unsigned int)axis->logicalDevice, (unsigned int)origin,
                 (unsigned int)selectedFault->pdsErrorCode, (unsigned int)selectedFault->emcyCode,
                 (unsigned long)selectedFault->infoCode);
    return true;
}

bool CO_402_device_diag_clear(CO_402_device_axis_t *axis)
{
    if (axis == NULL || axis->diagnosis.contractFailed) {
        return false;
    }

    if (axis->od.errorCode == NULL || OD_set_u16(axis->od.errorCode, 0U, 0U, true) != ODR_OK) {
        CO_402_device_diag_contractFailure(axis, CO_402_FAULT_ORIGIN_FAULT_RESET,
                                           "axis Error code clear failed");
        return false;
    }

    if (!axis->diagnosis.active) {
        return true;
    }

    axis->diagnosis.pending = true;
    axis->diagnosis.event.type = CO_402_DIAG_EVENT_RESET;
    axis->diagnosis.event.logicalDevice = axis->logicalDevice;
    axis->diagnosis.event.errorBit = CO_402_device_diag_errorBit(axis->logicalDevice);
    axis->diagnosis.event.fault = axis->diagnosis.fault;
    axis->diagnosis.active = false;
    axis->diagnosis.origin = CO_402_FAULT_ORIGIN_PRODUCT;
    memset(&axis->diagnosis.fault, 0, sizeof(axis->diagnosis.fault));

    CO_402_LOG_I("CiA402 diagnostic cleared: axis=%u", (unsigned int)axis->logicalDevice);
    return true;
}

bool CO_402_device_diag_restoreAxis(CO_402_device_axis_t *axis, bool replayActive)
{
    uint16_t pdsErrorCode;

    if (axis == NULL || axis->od.errorCode == NULL) {
        return false;
    }

    pdsErrorCode = axis->diagnosis.active ? axis->diagnosis.fault.pdsErrorCode : 0U;
    if (OD_set_u16(axis->od.errorCode, 0U, pdsErrorCode, true) != ODR_OK) {
        CO_402_device_diag_contractFailure(axis, axis->diagnosis.origin,
                                           "axis Error code restore failed");
        return false;
    }

    if (replayActive && axis->diagnosis.active) {
        axis->diagnosis.pending = true;
        axis->diagnosis.event.type = CO_402_DIAG_EVENT_REPORT;
        axis->diagnosis.event.logicalDevice = axis->logicalDevice;
        axis->diagnosis.event.errorBit = CO_402_device_diag_errorBit(axis->logicalDevice);
        axis->diagnosis.event.fault = axis->diagnosis.fault;
    }

    return true;
}

bool CO_402_device_diag_takePendingEvent(CO_402_device_axis_t *axis,
                                         CO_402_device_diag_event_t *event)
{
    if (axis == NULL || event == NULL || !axis->diagnosis.pending) {
        return false;
    }

    *event = axis->diagnosis.event;
    axis->diagnosis.pending = false;
    memset(&axis->diagnosis.event, 0, sizeof(axis->diagnosis.event));
    return true;
}

#endif /* CO_402_CONFIG_DIAGNOSTICS */
