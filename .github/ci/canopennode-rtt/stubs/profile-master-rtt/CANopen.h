#ifndef PROFILE_MASTER_RTT_HOST_STUB_CANOPEN_H
#define PROFILE_MASTER_RTT_HOST_STUB_CANOPEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CO_CONFIG_SDO_CLI_ENABLE 0x01U
#define CO_CONFIG_SDO_CLI (CO_CONFIG_SDO_CLI_ENABLE)
#define CO_CONFIG_NMT_MASTER 0x02U
#define CO_CONFIG_NMT (CO_CONFIG_NMT_MASTER)
#define CO_CONFIG_GTW_ASCII_SDO 0x01U
#ifndef CO_CONFIG_GTW
#define CO_CONFIG_GTW 0U
#endif

#define CO_CAN_ID_SDO_CLI 0x600U
#define CO_CAN_ID_SDO_SRV 0x580U

typedef uint_fast8_t bool_t;

typedef enum {
    CO_SDO_RT_uploadDataBufferFull = 5,
    CO_SDO_RT_blockUploadInProgress = 2,
    CO_SDO_RT_waitingResponse = 1,
    CO_SDO_RT_ok_communicationEnd = 0,
    CO_SDO_RT_wrongArguments = -2,
    CO_SDO_RT_endedWithServerAbort = -10
} CO_SDO_return_t;

typedef enum {
    CO_SDO_AB_NONE = 0,
    CO_SDO_AB_TIMEOUT = 0x05040000UL,
    CO_SDO_AB_NO_OBJECT = 0x06020000UL
} CO_SDO_abortCode_t;

typedef enum {
    CO_NMT_ENTER_OPERATIONAL = 1,
    CO_NMT_ENTER_STOPPED = 2,
    CO_NMT_ENTER_PRE_OPERATIONAL = 128,
    CO_NMT_RESET_NODE = 129,
    CO_NMT_RESET_COMMUNICATION = 130
} CO_NMT_command_t;

typedef enum {
    CO_RESET_NOT = 0,
    CO_RESET_COMM = 1,
    CO_RESET_APP = 2
} CO_NMT_reset_cmd_t;

typedef enum {
    CO_ERROR_NO = 0,
    CO_ERROR_ILLEGAL_ARGUMENT = -1
} CO_ReturnError_t;

typedef struct {
    unsigned unused;
} OD_t;

typedef struct {
    unsigned unused;
} CO_SDOclient_t;

typedef struct {
    unsigned unused;
} CO_NMT_t;

typedef struct CO_t {
    CO_SDOclient_t *SDOclient;
    CO_NMT_t *NMT;
} CO_t;

CO_SDO_return_t CO_SDOclient_setup(CO_SDOclient_t *client, uint32_t clientToServer,
                                   uint32_t serverToClient, uint8_t nodeId);
CO_SDO_return_t CO_SDOclientUploadInitiate(CO_SDOclient_t *client, uint16_t index, uint8_t subIndex,
                                           uint16_t timeoutMs, bool_t blockEnable);
CO_SDO_return_t CO_SDOclientUpload(CO_SDOclient_t *client, uint32_t dtUs, bool_t sendAbort,
                                   CO_SDO_abortCode_t *abortCode, size_t *sizeIndicated,
                                   size_t *sizeTransferred, uint32_t *timerNextUs);
size_t CO_SDOclientUploadBufRead(CO_SDOclient_t *client, uint8_t *buffer, size_t count);
CO_SDO_return_t CO_SDOclientDownloadInitiate(CO_SDOclient_t *client, uint16_t index, uint8_t subIndex,
                                             size_t size, uint16_t timeoutMs, bool_t blockEnable);
size_t CO_SDOclientDownloadBufWrite(CO_SDOclient_t *client, const uint8_t *data, size_t count);
CO_SDO_return_t CO_SDOclientDownload(CO_SDOclient_t *client, uint32_t dtUs, bool_t sendAbort,
                                     bool_t bufferPartial, CO_SDO_abortCode_t *abortCode,
                                     size_t *sizeTransferred, uint32_t *timerNextUs);
void CO_SDOclientClose(CO_SDOclient_t *client);
CO_ReturnError_t CO_NMT_sendCommand(CO_NMT_t *nmt, CO_NMT_command_t command, uint8_t nodeId);

#endif /* PROFILE_MASTER_RTT_HOST_STUB_CANOPEN_H */
