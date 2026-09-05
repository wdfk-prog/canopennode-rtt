#!/usr/bin/env sh
# SPDX-License-Identifier: MIT

set -eu

script_dir=$(dirname "$0")
root_dir=$(CDPATH= cd "$script_dir/../../.." && pwd)
out_dir="$root_dir/_ci/cia401-device-host"
out_bin="$out_dir/cia401-device-host-test"

mkdir -p "$out_dir"

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
    -o "$out_bin"

"$out_bin"
