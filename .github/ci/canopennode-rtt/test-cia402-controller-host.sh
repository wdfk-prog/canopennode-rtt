#!/usr/bin/env sh
# SPDX-License-Identifier: MIT

set -eu

script_dir=$(dirname "$0")
root_dir=$(CDPATH= cd "$script_dir/../../.." && pwd)
out_dir="$root_dir/_ci/cia402-controller-host"
out_bin="$out_dir/cia402-controller-host-test"

mkdir -p "$out_dir"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/profile/cia402/common" \
    -I"$root_dir/profile/cia402/controller" \
    "$root_dir/profile/cia402/common/CO_402_state.c" \
    "$root_dir/profile/cia402/controller/CO_402_controller.c" \
    "$root_dir/.github/ci/canopennode-rtt/cia402-controller-host-test.c" \
    -o "$out_bin"

"$out_bin"
