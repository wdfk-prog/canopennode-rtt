#!/usr/bin/env sh
# SPDX-License-Identifier: MIT

set -eu

script_dir=$(dirname "$0")
root_dir=$(CDPATH= cd "$script_dir/../../.." && pwd)
out_dir="$root_dir/_ci/profile-layout-host"
out_bin="$out_dir/profile-layout-host-test"
registry_bin="$out_dir/profile-registry-host-test"
mixed_rtt_bin="$out_dir/mixed-profile-rtt-host-test"
mixed_rtt_demo_bin="$out_dir/mixed-profile-rtt-demo-host-test"
shared_worker_bin="$out_dir/profile-shared-worker-host-test"

mkdir -p "$out_dir"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/profile/common" \
    "$root_dir/.github/ci/canopennode-rtt/profile-layout-host-test.c" \
    -o "$out_bin"

"$out_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root_dir/profile/common" \
    "$root_dir/profile/common/CO_profile_registry.c" \
    "$root_dir/.github/ci/canopennode-rtt/profile-registry-host-test.c" \
    -o "$registry_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -ffunction-sections -fdata-sections \
    -DPKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS=1 \
    -DPKG_CANOPENNODE_EM_PRODUCER=1 \
    -DPKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT=80 \
    -DPKG_CANOPENNODE_PROFILE_RTT_SHARED_WORKER=1 \
    -DPKG_CANOPENNODE_PROFILE_THREAD_STACK_SIZE=2048 \
    -DPKG_CANOPENNODE_PROFILE_THREAD_PRIORITY=5 \
    -DPKG_CANOPENNODE_RT_THREAD_PRIORITY=3 \
    -DPKG_CANOPENNODE_RT_THREAD_TICK=1 \
    -DCO_APP_RTT_H_=1 \
    -include "$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt/CO_app_RTT.h" \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt" \
    -I"$root_dir/port/rtthread" \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/example" \
    "$root_dir/port/rtthread/CO_lifecycle_RTT.c" \
    "$root_dir/.github/ci/canopennode-rtt/profile-shared-worker-host-test.c" \
    -Wl,--gc-sections \
    -o "$shared_worker_bin"

"$shared_worker_bin"

"$registry_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -DPKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS=1 \
    -DPKG_CANOPENNODE_EM_PRODUCER=1 \
    -DPKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT=80 \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt" \
    -I"$root_dir/port/rtthread" \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/example" \
    -I"$root_dir/CANopenNode/301" \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/mixed" \
    "$root_dir/CANopenNode/301/CO_ODinterface.c" \
    "$root_dir/profile/common/CO_profile_registry.c" \
    "$root_dir/profile/mixed/CO_profile_mixed_RTT.c" \
    "$root_dir/.github/ci/canopennode-rtt/mixed-profile-rtt-host-test.c" \
    -o "$mixed_rtt_bin"

"$mixed_rtt_bin"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic \
    -DPKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS=1 \
    -DPKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART=1 \
    -DPKG_CANOPENNODE_EM_PRODUCER=1 \
    -DPKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT=80 \
    -include "$root_dir/.github/ci/canopennode-rtt/stubs/mixed-profile-rtt/component-export.h" \
    -I"$root_dir/.github/ci/canopennode-rtt/stubs/cia401-rtt" \
    -I"$root_dir/port/rtthread" \
    -I"$root_dir/CANopenNode" \
    -I"$root_dir/CANopenNode/example" \
    -I"$root_dir/CANopenNode/301" \
    -I"$root_dir/profile/common" \
    -I"$root_dir/profile/mixed" \
    -I"$root_dir/profile/mixed/demo" \
    "$root_dir/profile/mixed/demo/CO_profile_mixed_demo.c" \
    "$root_dir/profile/mixed/demo/CO_profile_mixed_RTT_demo.c" \
    "$root_dir/.github/ci/canopennode-rtt/mixed-profile-rtt-demo-host-test.c" \
    -o "$mixed_rtt_demo_bin"

"$mixed_rtt_demo_bin"

${PYTHON:-python3} "$script_dir/verify-mixed-profile-artifacts.py" \
    "$root_dir/examples/demo_device" \
    "$root_dir/profile/mixed/demo/CO_profile_mixed_demo.h" \
    "$root_dir/profile/mixed/demo/CO_profile_mixed_demo.c"
