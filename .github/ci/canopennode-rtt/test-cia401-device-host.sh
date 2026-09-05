#!/usr/bin/env sh
# SPDX-License-Identifier: MIT

set -eu

script_dir=$(dirname "$0")
root_dir=$(CDPATH= cd "$script_dir/../../.." && pwd)
out_dir="$root_dir/_ci/cia401-device-host"
stage1_bin="$out_dir/cia401-device-host-test"
stage2_bin="$out_dir/cia401-digital-stage2-host-test"

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
