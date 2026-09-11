#!/usr/bin/env sh
# SPDX-License-Identifier: MIT

set -eu

script_dir=$(dirname "$0")
root_dir=$(CDPATH= cd "$script_dir/../../.." && pwd)
out_dir="$root_dir/_ci/profile-controller-host"
portable_bin="$out_dir/profile-controller-portable-host-test"
wrapper_bin="$out_dir/profile-controller-rtt-wrapper-host-test"
master_bin="$out_dir/profile-master-rtt-host-test"
gateway_master_bin="$out_dir/profile-master-rtt-gateway-host-test"

mkdir -p "$out_dir"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/common/port" \
    -I"$root_dir/profile/cia401/common" \
    -I"$root_dir/profile/cia401/controller" \
    -I"$root_dir/profile/cia402/common" \
    -I"$root_dir/profile/cia402/controller" \
    "$root_dir/profile/common/port/CO_profile_transport.c" \
    "$root_dir/profile/cia401/controller/CO_401_controller.c" \
    "$root_dir/profile/cia401/controller/CO_401_controller_client.c" \
    "$root_dir/profile/cia402/common/CO_402_state.c" \
    "$root_dir/profile/cia402/controller/CO_402_controller.c" \
    "$root_dir/profile/cia402/controller/CO_402_controller_client.c" \
    "$root_dir/.github/ci/canopennode-rtt/profile-controller-adapter-host-test.c" \
    -o "$portable_bin"

"$portable_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/profile-controller-adapter" \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/common/port" \
    -I"$root_dir/profile/cia401/common" \
    -I"$root_dir/profile/cia401/controller" \
    -I"$root_dir/profile/cia401/port/rtthread" \
    -I"$root_dir/profile/cia402/common" \
    -I"$root_dir/profile/cia402/controller" \
    -I"$root_dir/profile/cia402/port/rtthread" \
    "$root_dir/profile/common/port/CO_profile_transport.c" \
    "$root_dir/profile/cia401/controller/CO_401_controller.c" \
    "$root_dir/profile/cia401/controller/CO_401_controller_client.c" \
    "$root_dir/profile/cia401/port/rtthread/CO_401_controller_RTT.c" \
    "$root_dir/profile/cia402/common/CO_402_state.c" \
    "$root_dir/profile/cia402/controller/CO_402_controller.c" \
    "$root_dir/profile/cia402/controller/CO_402_controller_client.c" \
    "$root_dir/profile/cia402/port/rtthread/CO_402_controller_RTT.c" \
    "$root_dir/.github/ci/canopennode-rtt/profile-controller-rtt-wrapper-host-test.c" \
    -o "$wrapper_bin"

"$wrapper_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -DPKG_CANOPENNODE_GLOBAL_TIMERNEXT=1 \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/profile-master-rtt" \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/common/port" \
    -I"$root_dir/profile/common/port/rtthread" \
    "$root_dir/profile/common/port/CO_profile_transport.c" \
    "$root_dir/profile/common/port/rtthread/CO_profile_master_RTT.c" \
    "$root_dir/.github/ci/canopennode-rtt/profile-master-rtt-host-test.c" \
    -o "$master_bin"

"$master_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -DPKG_CANOPENNODE_GLOBAL_TIMERNEXT=1 \
    -DCO_CONFIG_GTW=CO_CONFIG_GTW_ASCII_SDO \
    -DOD_CNT_SDO_CLI=2 \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/profile-master-rtt" \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/common/port" \
    -I"$root_dir/profile/common/port/rtthread" \
    "$root_dir/profile/common/port/CO_profile_transport.c" \
    "$root_dir/profile/common/port/rtthread/CO_profile_master_RTT.c" \
    "$root_dir/.github/ci/canopennode-rtt/profile-master-rtt-host-test.c" \
    -o "$gateway_master_bin"

"$gateway_master_bin"
