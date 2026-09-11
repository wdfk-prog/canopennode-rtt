/**
 * @file CO_profile_controller.h
 * @brief Stack-neutral data contracts shared by CANopen Device Profile controllers.
 */

#ifndef CO_PROFILE_CONTROLLER_H_
#define CO_PROFILE_CONTROLLER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Maximum scalar payload carried by the profile-controller transport contract. */
#define CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX 8U

/** One resolved remote Object Dictionary scalar reference. */
typedef struct {
    uint16_t index; /**< Absolute remote Object Dictionary index. */
    uint8_t subIndex; /**< Remote Object Dictionary sub-index. */
    uint8_t size; /**< Expected encoded scalar size in bytes. */
} CO_profile_object_ref_t;

/** Terminal classification for one profile-controller transport operation. */
typedef enum {
    CO_PROFILE_TRANSFER_OK = 0, /**< The remote operation completed successfully. */
    CO_PROFILE_TRANSFER_ABORT, /**< The remote SDO server returned a protocol abort. */
    CO_PROFILE_TRANSFER_TIMEOUT, /**< The remote operation exceeded its protocol timeout. */
    CO_PROFILE_TRANSFER_CANCELED, /**< Runtime reset/teardown canceled the operation. */
    CO_PROFILE_TRANSFER_LOCAL_ERROR /**< Local admission, setup, encoding or transport error. */
} CO_profile_transfer_status_t;

/** Stack-neutral terminal transfer result retained by profile-specific adapters. */
typedef struct {
    CO_profile_transfer_status_t status; /**< Terminal transfer classification. */
    uint32_t abortCode; /**< CiA 301 SDO abort code when status is ABORT/TIMEOUT. */
    int32_t localError; /**< Backend-local error when @ref status is LOCAL_ERROR. */
    uint8_t data[CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX]; /**< Uploaded scalar bytes in CANopen little-endian order. */
    uint8_t size; /**< Number of valid bytes in @ref data. */
} CO_profile_transfer_result_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_PROFILE_CONTROLLER_H_ */
