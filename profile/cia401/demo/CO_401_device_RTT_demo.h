/**
 * @file CO_401_device_RTT_demo.h
 * @brief Test-only control surface for the software CiA 401 RT-Thread demo backend.
 */
#ifndef CO_401_DEVICE_RTT_DEMO_H
#define CO_401_DEVICE_RTT_DEMO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS 2U
#define CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS 2U

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)
/**
 * @brief Snapshot of the software-only backend used by the optional MSH bench frontend.
 *
 * Raw input values are the injected backend values. Output values are the final
 * physical values after CiA 401 polarity, fail-safe and conditioning processing.
 */
typedef struct {
    uint8_t digitalInput[CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS];
    uint8_t digitalOutput[CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS];
    int16_t analogInput[CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS];
    int16_t analogOutput[CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS];
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    uint8_t digitalInputFilter[CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS];
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
    bool outputSupervisionEstablished;
    bool outputSupervisionFault;
} CO_401_device_RTT_demo_snapshot_t;

/**
 * @brief Set one raw digital input bank for the next CiA 401 worker pass.
 *
 * The caller must hold the same lifecycle/OD serialization used by the worker.
 *
 * @param bank Zero-based software-demo digital input bank.
 * @param value Raw 8-bit backend value before CiA 401 polarity/event processing.
 * @return true when @p bank is valid; false otherwise.
 */
bool CO_401_device_RTT_demoSetDigitalInput(uint8_t bank, uint8_t value);

/**
 * @brief Set one raw analogue input channel for the next CiA 401 worker pass.
 *
 * The caller must hold the same lifecycle/OD serialization used by the worker.
 *
 * @param channel Zero-based software-demo analogue input channel.
 * @param value Raw Integer16 backend value before CiA 401 conditioning/event processing.
 * @return true when @p channel is valid; false otherwise.
 */
bool CO_401_device_RTT_demoSetAnalogInput(uint8_t channel, int16_t value);

/**
 * @brief Set the test-only output-supervision readiness source.
 *
 * @param established true after the simulated output-setting peer is supervised.
 */
void CO_401_device_RTT_demoSetOutputSupervisionEstablished(bool established);

/**
 * @brief Set the test-only output-setting-peer communication-fault source.
 *
 * @param active true to report the simulated peer communication fault.
 */
void CO_401_device_RTT_demoSetOutputSupervisionFault(bool active);

/**
 * @brief Copy the current software backend state for bench observation.
 *
 * The caller must serialize process-image arrays with the CiA 401 worker. Atomic
 * supervision fields may be sampled concurrently.
 *
 * @param snapshot Destination snapshot; NULL is ignored.
 */
void CO_401_device_RTT_demoGetSnapshot(CO_401_device_RTT_demo_snapshot_t *snapshot);
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CO_401_DEVICE_RTT_DEMO_H */
