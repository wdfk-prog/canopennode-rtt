#!/usr/bin/env sh
# SPDX-License-Identifier: MIT

set -eu

script_dir=$(dirname "$0")
root_dir=$(CDPATH= cd "$script_dir/../../.." && pwd)
out_dir="$root_dir/_ci/cia401-device-host"
stage1_bin="$out_dir/cia401-device-host-test"
stage2_bin="$out_dir/cia401-digital-stage2-host-test"
lifecycle_bin="$out_dir/cia401-rtt-lifecycle-host-test"
lifecycle_bitwise_bin="$out_dir/cia401-rtt-lifecycle-bitwise-host-test"

mkdir -p "$out_dir"

# Keep the Stage-1 build free of optional feature macros to prove Kconfig rollback behavior.
${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/example" \
    -I"$root_dir/profile/cia401/common" \
    -I"$root_dir/profile/cia401/device" \
    "$root_dir/CANopenNode/301/CO_ODinterface.c" \
    "$root_dir/profile/cia401/device/CO_401_device.c" \
    "$root_dir/profile/cia401/device/CO_401_device_od.c" \
    "$root_dir/profile/cia401/device/CO_401_digital.c" \
    "$root_dir/profile/cia401/device/CO_401_analog.c" \
    "$root_dir/.github/ci/canopennode-rtt/cia401-device-host-test.c" \
    -o "$stage1_bin"

"$stage1_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -DPKG_CANOPENNODE_CIA401_DIGITAL_EVENTS=1 \
    -DPKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE=1 \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/example" \
    -I"$root_dir/profile/cia401/common" \
    -I"$root_dir/profile/cia401/device" \
    "$root_dir/CANopenNode/301/CO_ODinterface.c" \
    "$root_dir/profile/cia401/device/CO_401_device.c" \
    "$root_dir/profile/cia401/device/CO_401_device_od.c" \
    "$root_dir/profile/cia401/device/CO_401_digital.c" \
    "$root_dir/profile/cia401/device/CO_401_analog.c" \
    "$root_dir/.github/ci/canopennode-rtt/cia401-digital-stage2-host-test.c" \
    -o "$stage2_bin"

"$stage2_bin"

build_analog_variant() {
    variant=$1
    with_tpdo=$2
    shift 2
    analog_bin="$out_dir/cia401-analog-$variant-host-test"
    variant_define="-DCIA401_ANALOG_VARIANT=\"$variant\""

    if [ "$with_tpdo" = "yes" ]; then
        ${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
            "$variant_define" \
            -DCIA401_TEST_TPDO=1 \
            -DCO_CONFIG_PDO=0x23 \
            "$@" \
            -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-pdo" \
            -I"$root_dir/CANopenNode" \
            -I"$root_dir/CANopenNode/example" \
            -I"$root_dir/profile/cia401/common" \
            -I"$root_dir/profile/cia401/device" \
            "$root_dir/CANopenNode/301/CO_ODinterface.c" \
            "$root_dir/CANopenNode/301/CO_PDO.c" \
            "$root_dir/profile/cia401/device/CO_401_device.c" \
            "$root_dir/profile/cia401/device/CO_401_device_od.c" \
            "$root_dir/profile/cia401/device/CO_401_digital.c" \
            "$root_dir/profile/cia401/device/CO_401_analog.c" \
            "$root_dir/.github/ci/canopennode-rtt/cia401-analog-stage345-host-test.c" \
            -o "$analog_bin"
    else
        ${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
            "$variant_define" \
            "$@" \
            -I"$root_dir/CANopenNode" \
            -I"$root_dir/CANopenNode/example" \
            -I"$root_dir/profile/cia401/common" \
            -I"$root_dir/profile/cia401/device" \
            "$root_dir/CANopenNode/301/CO_ODinterface.c" \
            "$root_dir/profile/cia401/device/CO_401_device.c" \
            "$root_dir/profile/cia401/device/CO_401_device_od.c" \
            "$root_dir/profile/cia401/device/CO_401_digital.c" \
            "$root_dir/profile/cia401/device/CO_401_analog.c" \
            "$root_dir/.github/ci/canopennode-rtt/cia401-analog-stage345-host-test.c" \
            -o "$analog_bin"
    fi

    "$analog_bin"
}

# Compile each legal analogue feature independently so all-on cannot hide conditional-compilation coupling.
build_analog_variant events yes \
    -DPKG_CANOPENNODE_CIA401_ANALOG_EVENTS=1
build_analog_variant failsafe no \
    -DPKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE=1
build_analog_variant conditioning no \
    -DPKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING=1
build_analog_variant all yes \
    -DPKG_CANOPENNODE_CIA401_ANALOG_EVENTS=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING=1

build_rtt_matrix_variant() {
    variant=$1
    shift
    matrix_bin="$out_dir/cia401-rtt-matrix-$variant-host-test"

    ${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
        -ffunction-sections -fdata-sections \
        -DRT_USING_HEAP=1 \
        -DPKG_CANOPENNODE_CIA401=1 \
        -DPKG_CANOPENNODE_CIA401_DEVICE=1 \
        -DPKG_CANOPENNODE_CAN_BINDING_COUNT=1 \
        -DPKG_CANOPENNODE_CAN_DEV_NAME=\"can1\" \
        -DPKG_CANOPENNODE_RX_BATCH_SIZE=4 \
        -DPKG_CANOPENNODE_RX_THREAD_STACK_SIZE=1024 \
        -DPKG_CANOPENNODE_RX_THREAD_PRIORITY=6 \
        -DPKG_CANOPENNODE_RX_THREAD_TICK=1 \
        -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_THREAD=1 \
        -DPKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS=1 \
        -DPKG_CANOPENNODE_EM_PRODUCER=1 \
        -DPKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT=80 \
        -DPKG_CANOPENNODE_USING_SDO_SERVER=1 \
        -DPKG_CANOPENNODE_SDO_SRV_SEGMENTED=1 \
        -DPKG_CANOPENNODE_SDO_SRV_BLOCK=1 \
        -DPKG_CANOPENNODE_SDO_SRV_BUFFER_SIZE=1024 \
        -DPKG_CANOPENNODE_USING_CRC16=1 \
        -DPKG_CANOPENNODE_CIA401_THREAD_STACK_SIZE=1536 \
        -DPKG_CANOPENNODE_CIA401_THREAD_PRIORITY=6 \
        -DPKG_CANOPENNODE_RT_THREAD_PRIORITY=5 \
        -DPKG_CANOPENNODE_RT_THREAD_TICK=1 \
        -DPKG_CANOPENNODE_USING_PDO=1 \
        -DPKG_CANOPENNODE_TPDO=1 \
        -DPKG_CANOPENNODE_PDO_OD_IO_ACCESS=1 \
        "$@" \
        -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt" \
        -I"$root_dir/port/rtthread" \
        -I"$root_dir/CANopenNode/example" \
        -I"$root_dir/CANopenNode" \
        -I"$root_dir/CANopenNode/301" \
        -I"$root_dir/profile/cia401/port/rtthread" \
        -I"$root_dir/profile/cia401/common" \
        -I"$root_dir/profile/cia401/device" \
        "$root_dir/CANopenNode/301/CO_ODinterface.c" \
        "$root_dir/profile/cia401/device/CO_401_device.c" \
        "$root_dir/profile/cia401/device/CO_401_device_od.c" \
        "$root_dir/profile/cia401/device/CO_401_digital.c" \
        "$root_dir/profile/cia401/device/CO_401_analog.c" \
        "$root_dir/profile/cia401/port/rtthread/CO_401_device_RTT.c" \
        "$root_dir/.github/ci/canopennode-rtt/cia401-rtt-matrix-link-host-test.c" \
        -Wl,--gc-sections \
        -o "$matrix_bin"

    "$matrix_bin"
    printf 'CIA401_RTT_MATRIX_CASE_PASS:%s\n' "$variant"
}

# Keep compile/link coverage for legal RT-adapter feature branches separate from the all-on behavior tests below.
build_rtt_matrix_variant base-rtt
build_rtt_matrix_variant events-only \
    -DPKG_CANOPENNODE_CIA401_ANALOG_EVENTS=1 \
    -DPKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER=1
build_rtt_matrix_variant digital-events-only \
    -DPKG_CANOPENNODE_CIA401_DIGITAL_EVENTS=1 \
    -DPKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER=1
build_rtt_matrix_variant analog-failsafe-only \
    -DPKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE=1
build_rtt_matrix_variant digital-failsafe \
    -DPKG_CANOPENNODE_RPDO=1 \
    -DPKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE=1
build_rtt_matrix_variant single-od-all \
    -DPKG_CANOPENNODE_RPDO=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_EVENTS=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE=1 \
    -DPKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE=1 \
    -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE=1 \
    -DPKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER=1

# Link the real RT-Thread TX path; section GC keeps this Host contract focused on the observer boundary.
${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -ffunction-sections -fdata-sections \
    -DPKG_CANOPENNODE_CIA401_DIGITAL_EVENTS=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_EVENTS=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE=1 \
    -DPKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER=1 \
    -DPKG_CANOPENNODE_CAN_BINDING_COUNT=1 \
    -DPKG_CANOPENNODE_CAN_DEV_NAME=\"can1\" \
    -DPKG_CANOPENNODE_RX_BATCH_SIZE=4 \
    -DPKG_CANOPENNODE_RX_THREAD_STACK_SIZE=1024 \
    -DPKG_CANOPENNODE_RX_THREAD_PRIORITY=6 \
    -DPKG_CANOPENNODE_RX_THREAD_TICK=1 \
    -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_THREAD=1 \
    -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE=1 \
    -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART=1 \
    -DPKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS=1 \
    -DPKG_CANOPENNODE_EM_PRODUCER=1 \
    -DPKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT=80 \
    -DPKG_CANOPENNODE_USING_SDO_SERVER=1 \
    -DPKG_CANOPENNODE_SDO_SRV_SEGMENTED=1 \
    -DPKG_CANOPENNODE_SDO_SRV_BLOCK=1 \
    -DPKG_CANOPENNODE_SDO_SRV_BUFFER_SIZE=1024 \
    -DPKG_CANOPENNODE_USING_CRC16=1 \
    -DPKG_CANOPENNODE_CIA401_THREAD_STACK_SIZE=1536 \
    -DPKG_CANOPENNODE_CIA401_THREAD_PRIORITY=6 \
    -DPKG_CANOPENNODE_RT_THREAD_PRIORITY=5 \
    -DPKG_CANOPENNODE_RT_THREAD_TICK=1 \
    -DCO_MULTIPLE_OD=1 \
    -DPKG_CANOPENNODE_USING_PDO=1 \
    -DPKG_CANOPENNODE_TPDO=1 \
    -DPKG_CANOPENNODE_PDO_OD_IO_ACCESS=1 \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt" \
    -I"$root_dir/port/rtthread" \
    -I"$root_dir/CANopenNode/example" \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/301" \
    -I"$root_dir/profile/cia401/port/rtthread" \
    -I"$root_dir/profile/cia401/common" \
    -I"$root_dir/profile/cia401/device" \
    "$root_dir/CANopenNode/301/CO_ODinterface.c" \
    "$root_dir/profile/cia401/device/CO_401_device.c" \
    "$root_dir/profile/cia401/device/CO_401_device_od.c" \
    "$root_dir/profile/cia401/device/CO_401_digital.c" \
    "$root_dir/profile/cia401/device/CO_401_analog.c" \
    "$root_dir/port/rtthread/CO_driver_rtthread.c" \
    "$root_dir/profile/cia401/port/rtthread/CO_401_device_RTT.c" \
    "$root_dir/.github/ci/canopennode-rtt/cia401-rtt-lifecycle-host-test.c" \
    -Wl,--gc-sections \
    -o "$lifecycle_bin"

"$lifecycle_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -DCIA401_TEST_BITWISE_TPDO=1 \
    -ffunction-sections -fdata-sections \
    -DPKG_CANOPENNODE_CIA401_DIGITAL_EVENTS=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_EVENTS=1 \
    -DPKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE=1 \
    -DPKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER=1 \
    -DPKG_CANOPENNODE_CAN_BINDING_COUNT=1 \
    -DPKG_CANOPENNODE_CAN_DEV_NAME=\"can1\" \
    -DPKG_CANOPENNODE_RX_BATCH_SIZE=4 \
    -DPKG_CANOPENNODE_RX_THREAD_STACK_SIZE=1024 \
    -DPKG_CANOPENNODE_RX_THREAD_PRIORITY=6 \
    -DPKG_CANOPENNODE_RX_THREAD_TICK=1 \
    -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_THREAD=1 \
    -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE=1 \
    -DPKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART=1 \
    -DPKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS=1 \
    -DPKG_CANOPENNODE_EM_PRODUCER=1 \
    -DPKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT=80 \
    -DPKG_CANOPENNODE_USING_SDO_SERVER=1 \
    -DPKG_CANOPENNODE_SDO_SRV_SEGMENTED=1 \
    -DPKG_CANOPENNODE_SDO_SRV_BLOCK=1 \
    -DPKG_CANOPENNODE_SDO_SRV_BUFFER_SIZE=1024 \
    -DPKG_CANOPENNODE_USING_CRC16=1 \
    -DPKG_CANOPENNODE_CIA401_THREAD_STACK_SIZE=1536 \
    -DPKG_CANOPENNODE_CIA401_THREAD_PRIORITY=6 \
    -DPKG_CANOPENNODE_RT_THREAD_PRIORITY=5 \
    -DPKG_CANOPENNODE_RT_THREAD_TICK=1 \
    -DCO_MULTIPLE_OD=1 \
    -DPKG_CANOPENNODE_USING_PDO=1 \
    -DPKG_CANOPENNODE_TPDO=1 \
    -DPKG_CANOPENNODE_PDO_OD_IO_ACCESS=1 \
    -DPKG_CANOPENNODE_PDO_BITWISE_MAPPING=1 \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt" \
    -I"$root_dir/port/rtthread" \
    -I"$root_dir/CANopenNode/example" \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/301" \
    -I"$root_dir/profile/cia401/port/rtthread" \
    -I"$root_dir/profile/cia401/common" \
    -I"$root_dir/profile/cia401/device" \
    "$root_dir/CANopenNode/301/CO_ODinterface.c" \
    "$root_dir/profile/cia401/device/CO_401_device.c" \
    "$root_dir/profile/cia401/device/CO_401_device_od.c" \
    "$root_dir/profile/cia401/device/CO_401_digital.c" \
    "$root_dir/profile/cia401/device/CO_401_analog.c" \
    "$root_dir/port/rtthread/CO_driver_rtthread.c" \
    "$root_dir/profile/cia401/port/rtthread/CO_401_device_RTT.c" \
    "$root_dir/.github/ci/canopennode-rtt/cia401-rtt-lifecycle-host-test.c" \
    -Wl,--gc-sections \
    -o "$lifecycle_bitwise_bin"

"$lifecycle_bitwise_bin"
