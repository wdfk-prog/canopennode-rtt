#!/usr/bin/env sh
# SPDX-License-Identifier: MIT

set -eu

script_dir=$(dirname "$0")
root_dir=$(CDPATH= cd "$script_dir/../../.." && pwd)
out_dir="$root_dir/_ci/cia402-controller-host"
out_bin="$out_dir/cia402-controller-host-test"

mkdir -p "$out_dir"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/cia402/common" \
    -I"$root_dir/profile/cia402/controller" \
    "$root_dir/profile/cia402/common/CO_402_state.c" \
    "$root_dir/profile/cia402/controller/CO_402_controller.c" \
    "$root_dir/.github/ci/canopennode-rtt/cia402-controller-host-test.c" \
    -o "$out_bin"

"$out_bin"

device_bin="$out_dir/cia402-device-layout-host-test"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/example" \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/cia402/common" \
    -I"$root_dir/profile/cia402/device" \
    "$root_dir/CANopenNode/301/CO_ODinterface.c" \
    "$root_dir/profile/cia402/common/CO_402_state.c" \
    "$root_dir/profile/cia402/device/CO_402_device_fsa.c" \
    "$root_dir/profile/cia402/device/CO_402_device_od.c" \
    "$root_dir/profile/cia402/device/CO_402_device.c" \
    "$root_dir/.github/ci/canopennode-rtt/cia402-device-layout-host-test.c" \
    -o "$device_bin"

"$device_bin"

build_rtt_scheduling_variant() {
    variant=$1
    shift
    scheduling_bin="$out_dir/cia402-rtt-scheduling-$variant-host-test"

    ${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
        -ffunction-sections -fdata-sections \
        -DPKG_CANOPENNODE_CIA402_DEVICE_RTT_THREAD=1 \
        -DPKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS=1 \
        -DPKG_CANOPENNODE_EM_PRODUCER=1 \
        -DPKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT=80 \
        -DPKG_CANOPENNODE_RT_THREAD_PRIORITY=3 \
        -DPKG_CANOPENNODE_RT_THREAD_TICK=1 \
        "$@" \
        -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt" \
        -I"$root_dir/port/rtthread" \
        -I"$root_dir/CANopenNode" \
        -I"$root_dir/CANopenNode/example" \
        -I"$root_dir/CANopenNode/301" \
        -I"$root_dir/profile/common" \
        -I"$root_dir/profile/cia402/common" \
        -I"$root_dir/profile/cia402/device" \
        -I"$root_dir/profile/cia402/port/rtthread" \
        "$root_dir/CANopenNode/301/CO_ODinterface.c" \
        "$root_dir/profile/cia402/common/CO_402_state.c" \
        "$root_dir/profile/cia402/device/CO_402_device_fsa.c" \
        "$root_dir/profile/cia402/device/CO_402_device_od.c" \
        "$root_dir/profile/cia402/device/CO_402_device.c" \
        "$root_dir/profile/cia402/port/rtthread/CO_402_device_RTT.c" \
        "$root_dir/.github/ci/canopennode-rtt/cia402-rtt-scheduling-host-test.c" \
        -Wl,--gc-sections \
        -o "$scheduling_bin"

    "$scheduling_bin"
}

build_rtt_scheduling_variant shared \
    -DPKG_CANOPENNODE_PROFILE_RTT_SHARED_WORKER=1 \
    -DPKG_CANOPENNODE_PROFILE_THREAD_STACK_SIZE=2048 \
    -DPKG_CANOPENNODE_PROFILE_THREAD_PRIORITY=5

build_rtt_scheduling_variant dedicated \
    -DPKG_CANOPENNODE_CIA402_DEVICE_RTT_DEDICATED_WORKER=1 \
    -DPKG_CANOPENNODE_CIA402_THREAD_STACK_SIZE=2048 \
    -DPKG_CANOPENNODE_CIA402_THREAD_PRIORITY=5

