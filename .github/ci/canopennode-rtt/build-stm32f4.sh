#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -euo pipefail

PACKAGE_NAME="${PACKAGE_NAME:-canopennode-rtt}"
RTTHREAD_REF="${RTTHREAD_REF:-master}"
RTTHREAD_ENV_REF="${RTTHREAD_ENV_REF:-master}"
STM32_BSP="${STM32_BSP:-bsp/stm32/stm32f407-atk-explorer}"
STM32_SERIES_LC="${STM32_SERIES_LC:-stm32f4}"
CANOPENNODE_CI_PROFILE="${CANOPENNODE_CI_PROFILE:-demo-default}"
PACKAGE_ROOT="${GITHUB_WORKSPACE:-$(pwd)}"
WORK_DIR="${WORK_DIR:-$PACKAGE_ROOT/_ci}"
RTTHREAD_DIR="$WORK_DIR/rt-thread"
RTT_ENV_DIR="$WORK_DIR/env"
LOG_FILE="$WORK_DIR/build-stm32f4.log"

mkdir -p "$WORK_DIR"
: > "$LOG_FILE"

log()
{
    printf '[ci] %s\n' "$*" | tee -a "$LOG_FILE"
}

run_logged()
{
    log "+ $*"
    "$@" 2>&1 | tee -a "$LOG_FILE"
}

clone_repo()
{
    local url="$1"
    local ref="$2"
    local dir="$3"

    if [ -d "$dir/.git" ]; then
        log "Reuse existing checkout: $dir"
        return 0
    fi

    rm -rf "$dir"
    run_logged git clone --depth 1 --branch "$ref" "$url" "$dir"
}

append_unique_config_line()
{
    local file="$1"
    local line="$2"

    grep -Fxq "$line" "$file" 2>/dev/null || printf '%s\n' "$line" >> "$file"
}

append_define_once()
{
    local file="$1"
    local macro="$2"
    local value="${3:-}"

    sed -i "/^#define[[:space:]]\+$macro\([[:space:]]\|$\)/d" "$file"
    if [ -n "$value" ]; then
        printf '#define %s %s\n' "$macro" "$value" >> "$file"
    else
        printf '#define %s\n' "$macro" >> "$file"
    fi
}

remove_exact_line()
{
    local file="$1"
    local line="$2"
    local tmp

    tmp="$(mktemp)"
    grep -Fxv "$line" "$file" > "$tmp" || true
    mv "$tmp" "$file"
}

remove_ci_sconstruct_patch()
{
    local sconstruct="$1"

    if [ ! -f "$sconstruct" ]; then
        return 0
    fi

    remove_exact_line "$sconstruct" "# CI-only local package integration for canopennode-rtt compile smoke test."
    remove_exact_line "$sconstruct" "objs += SConscript('packages/canopennode-rtt/SConscript')"
}

cleanup_canopennode_build_artifacts()
{
    local bsp_dir="$1"

    log "Clean stale CANopenNode build artifacts"
    rm -rf "$bsp_dir/build/packages/$PACKAGE_NAME"
    if [ -d "$bsp_dir/packages/$PACKAGE_NAME" ]; then
        find "$bsp_dir/packages/$PACKAGE_NAME" -type f \
            \( -name '*.o' -o -name '*.d' -o -name '*.a' \) -delete
    fi
}


append_config_define()
{
    local config_file="$1"
    local rtconfig_file="$2"
    local macro="$3"

    append_unique_config_line "$config_file" "$macro=y"
    append_define_once "$rtconfig_file" "$macro"
}

append_config_value()
{
    local config_file="$1"
    local rtconfig_file="$2"
    local macro="$3"
    local value="$4"

    append_unique_config_line "$config_file" "$macro=$value"
    append_define_once "$rtconfig_file" "$macro" "$value"
}

append_canopennode_base_profile()
{
    local config_file="$1"
    local rtconfig_file="$2"

    append_config_define "$config_file" "$rtconfig_file" "RT_USING_HEAP"
    append_config_define "$config_file" "$rtconfig_file" "RT_USING_DEVICE"
    append_config_define "$config_file" "$rtconfig_file" "RT_USING_CAN"
    append_config_define "$config_file" "$rtconfig_file" "RT_USING_MUTEX"
    append_config_define "$config_file" "$rtconfig_file" "RT_USING_SEMAPHORE"
    append_config_define "$config_file" "$rtconfig_file" "BSP_USING_CAN"
    append_config_define "$config_file" "$rtconfig_file" "BSP_USING_CAN1"

    # Keep the local staged package under BSP/packages and let its SConscript see
    # PKG_USING_CANOPENNODE through rtconfig.h only. Writing PKG_USING_CANOPENNODE=y
    # to .config may also let RT-Thread package plumbing load the same SConscript,
    # which can duplicate objects under packages/ and build/packages/.
    remove_exact_line "$config_file" "PKG_USING_CANOPENNODE=y"
    append_define_once "$rtconfig_file" "PKG_USING_CANOPENNODE"

    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_CAN_DEV_NAME" '"can1"'
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_CAN_BINDING_COUNT" "1"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RX_THREAD_STACK_SIZE" "2048"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RX_THREAD_PRIORITY" "2"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RX_THREAD_TICK" "10"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RX_BATCH_SIZE" "8"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_MAIN_THREAD_STACK_SIZE" "2048"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_MAIN_THREAD_PRIORITY" "10"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_MAIN_THREAD_TICK" "10"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RT_THREAD_STACK_SIZE" "2048"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RT_THREAD_PRIORITY" "3"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RT_THREAD_TICK" "10"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_TIMER_PERIOD_US" "1000"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_APP_FIRST_HB_TIME_MS" "500"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_APP_SDO_SRV_TIMEOUT_MS" "1000"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_APP_SDO_CLI_TIMEOUT_MS" "500"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_APP_AUTO_INIT"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_AUTO_INIT_CAN_DEV_NAME" '"can1"'
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_AUTO_INIT_NODE_ID" "1"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_AUTO_INIT_BITRATE_1000"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_AUTO_INIT_BITRATE" "1000"

    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_EM_PRODUCER"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_EM_HISTORY"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT" "80"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_ERR_CONDITION_GENERIC_STACK"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_ERR_CONDITION_COMMUNICATION_STACK"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_ERR_CONDITION_MANUFACTURER_STACK"

    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_SDO_SERVER"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SDO_SRV_SEGMENTED"
    append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SDO_SRV_BUFFER_SIZE" "32"

    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_DEMO_OD"
}

append_canopennode_default_objects()
{
    local config_file="$1"
    local rtconfig_file="$2"

    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GLOBAL_OD_DYNAMIC"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_HB_CONS"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_HB_CONS_CALLBACK_NONE"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_TIME"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_SYNC"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SYNC_PRODUCER"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_PDO"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RPDO"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_TPDO"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_RPDO_TIMERS"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_TPDO_TIMERS"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_PDO_SYNC"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_PDO_OD_IO_ACCESS"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_LEDS"
    append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_LSS_SLAVE"
}

append_canopennode_profile()
{
    local config_file="$1"
    local rtconfig_file="$2"
    local profile="$3"

    append_canopennode_base_profile "$config_file" "$rtconfig_file"

    case "$profile" in
        demo-minimal)
            log "CI Kconfig profile demo-minimal: core, emergency, SDO server, demo OD"
            ;;
        demo-default)
            log "CI Kconfig profile demo-default: default demo object set"
            append_canopennode_default_objects "$config_file" "$rtconfig_file"
            ;;
        demo-pdo-sync)
            log "CI Kconfig profile demo-pdo-sync: PDO/SYNC/TIME with callback and timer flags"
            append_canopennode_default_objects "$config_file" "$rtconfig_file"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GLOBAL_CALLBACK_PRE"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GLOBAL_RT_CALLBACK_PRE"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GLOBAL_TIMERNEXT"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_NMT_CALLBACK_CHANGE"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_TIME_PRODUCER"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_PDO_BITWISE_MAPPING"
            ;;
        demo-sdo-client-gateway)
            log "CI Kconfig profile demo-sdo-client-gateway: SDO client, FIFO/CRC16, gateway, NMT/LSS master"
            append_canopennode_default_objects "$config_file" "$rtconfig_file"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_NMT_MASTER"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_SDO_CLIENT"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SDO_CLI_SEGMENTED"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SDO_CLI_BLOCK"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SDO_CLI_LOCAL"
            append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SDO_CLI_BUFFER_SIZE" "1000"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_FIFO"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_FIFO_ALT_READ"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_FIFO_CRC16_CCITT"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_FIFO_ASCII_COMMANDS"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_FIFO_ASCII_DATATYPES"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_CRC16"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_LSS_MASTER"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_GATEWAY_ASCII"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GATEWAY_ASCII_SDO"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GATEWAY_ASCII_NMT"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GATEWAY_ASCII_LSS"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GATEWAY_ASCII_LOG"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GATEWAY_ASCII_ERROR_DESC"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GATEWAY_ASCII_PRINT_HELP"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GATEWAY_ASCII_PRINT_LEDS"
            append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GTW_BLOCK_DL_LOOP" "1"
            append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GTWA_COMM_BUF_SIZE" "200"
            append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GTWA_LOG_BUF_SIZE" "2000"
            ;;
        demo-safety-debug)
            log "CI Kconfig profile demo-safety-debug: node guarding, GFC/SRDO, CAN HDR filter, ulog debug"
            append_canopennode_default_objects "$config_file" "$rtconfig_file"
            append_config_define "$config_file" "$rtconfig_file" "RT_USING_ULOG"
            append_config_define "$config_file" "$rtconfig_file" "RT_CAN_USING_HDR"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_RTT_CAN_FILTER"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_NODE_GUARDING"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_NODE_GUARDING_SLAVE"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_NODE_GUARDING_MASTER"
            append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_NODE_GUARDING_MASTER_COUNT" "8"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_GFC"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GFC_CONSUMER"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_GFC_PRODUCER"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_CRC16"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_SRDO"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SRDO_CHECK_TX"
            append_config_value "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_SRDO_MINIMUM_DELAY" "0"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_DEBUG"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_DEBUG_COMMON"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_DEBUG_SDO_SERVER"
            append_config_define "$config_file" "$rtconfig_file" "PKG_CANOPENNODE_USING_FRAME_TRACE"
            ;;
        *)
            echo "Unknown CANopenNode CI profile: $profile" >&2
            echo "Supported profiles: demo-minimal demo-default demo-pdo-sync demo-sdo-client-gateway demo-safety-debug" >&2
            exit 1
            ;;
    esac
}

apply_ci_config()
{
    local bsp_dir="$1"
    local config_file="$bsp_dir/.config"
    local rtconfig_file="$bsp_dir/rtconfig.h"

    log "Apply STM32F4 CAN and CANopenNode CI configuration: profile=$CANOPENNODE_CI_PROFILE"
    touch "$config_file" "$rtconfig_file"

    cat >> "$rtconfig_file" <<'EOF_RTCONFIG'

/* CI-only STM32F4 CANopenNode compile profile. */
EOF_RTCONFIG

    append_canopennode_profile "$config_file" "$rtconfig_file" "$CANOPENNODE_CI_PROFILE"

    grep -E '^(#define[[:space:]]+(RT_USING_CAN|RT_USING_ULOG|RT_CAN_USING_HDR|BSP_USING_CAN|PKG_USING_CANOPENNODE|PKG_CANOPENNODE_))' "$rtconfig_file" \
        2>&1 | tee -a "$LOG_FILE"
}

verify_stm32_headers()
{
    local bsp_dir="$1"
    local series_lc="$2"
    local header

    cd "$bsp_dir"
    for header in "${series_lc}xx.h" "${series_lc}xx_hal.h"; do
        if ! find packages -type f -name "$header" -print -quit | grep -q .; then
            echo "Required STM32 package header was not fetched: $header" >&2
            grep -E '^(PKG_USING_|SOC_SERIES_)' .config rtconfig.h >&2 || true
            exit 1
        fi
    done
}

verify_stm32_can_support()
{
    local bsp_dir="$1"

    if ! grep -R "BSP_USING_CAN" -n "$bsp_dir" "$RTTHREAD_DIR/bsp/stm32/libraries" >/dev/null; then
        echo "Selected STM32 BSP tree does not expose BSP_USING_CAN: $bsp_dir" >&2
        exit 1
    fi

    if ! find "$RTTHREAD_DIR/bsp/stm32/libraries" -type f -name 'drv_can.c' -print -quit | grep -q .; then
        echo "STM32 HAL CAN driver source drv_can.c was not found." >&2
        exit 1
    fi
}

enable_hal_can_module()
{
    local bsp_dir="$1"
    local series_lc="$2"
    local hal_conf="$bsp_dir/board/CubeMX_Config/Inc/${series_lc}xx_hal_conf.h"

    if [ ! -f "$hal_conf" ]; then
        echo "STM32 HAL config file was not found: $hal_conf" >&2
        find "$bsp_dir/board" -type f -name '*_hal_conf.h' -print >&2 || true
        exit 1
    fi

    log "Enable HAL CAN module in ${hal_conf#$bsp_dir/}"
    HAL_CONF="$hal_conf" python3 - <<'PY_HAL_CAN'
import os
import re
from pathlib import Path

hal_conf = Path(os.environ["HAL_CONF"])
text = hal_conf.read_text(encoding="utf-8", errors="ignore")
if not re.search(r"^\s*#define\s+HAL_CAN_MODULE_ENABLED\b", text, re.MULTILINE):
    anchor = re.search(r"^\s*#define\s+HAL_MODULE_ENABLED\b.*$", text, re.MULTILINE)
    if anchor:
        insert_at = anchor.end()
        text = text[:insert_at] + "\n#define HAL_CAN_MODULE_ENABLED" + text[insert_at:]
    else:
        text = "#define HAL_CAN_MODULE_ENABLED\n" + text
    hal_conf.write_text(text, encoding="utf-8")
PY_HAL_CAN

    grep -E '^#define[[:space:]]+HAL_CAN_MODULE_ENABLED$' "$hal_conf" 2>&1 | tee -a "$LOG_FILE"
}

verify_outputs()
{
    local bsp_dir="$1"
    local bin_file="$bsp_dir/rtthread.bin"
    local can_obj
    local elf_file=""
    local candidate

    for candidate in "$bsp_dir/rtthread.elf" "$bsp_dir/rt-thread.elf"; do
        if [ -f "$candidate" ]; then
            elf_file="$candidate"
            break
        fi
    done

    if [ -z "$elf_file" ]; then
        echo "No RT-Thread ELF output found under $bsp_dir" >&2
        find "$bsp_dir" -maxdepth 1 -type f \( -name '*.elf' -o -name '*.bin' -o -name '*.map' \) -print >&2
        exit 1
    fi

    test -f "$bin_file"
    can_obj="$(find "$bsp_dir/build" -type f -name '*drv_can*.o' -print -quit)"
    test -n "$can_obj" || {
        echo "CAN driver object was not built; BSP_USING_CAN/BSP_USING_CAN1 did not take effect." >&2
        exit 1
    }
    log "CAN driver object: ${can_obj#$bsp_dir/}"
    arm-none-eabi-size "$elf_file" 2>&1 | tee -a "$LOG_FILE"
}

clone_repo "https://github.com/RT-Thread/rt-thread.git" "$RTTHREAD_REF" "$RTTHREAD_DIR"
clone_repo "https://github.com/RT-Thread/env.git" "$RTTHREAD_ENV_REF" "$RTT_ENV_DIR"

export RTT_ROOT="$RTTHREAD_DIR"
export RTT_ENV="$RTT_ENV_DIR"
export RTT_CC="${RTT_CC:-gcc}"
export RTT_EXEC_PATH="${RTT_EXEC_PATH:-/opt/gcc-arm-none-eabi/bin}"
export PATH="$HOME/.local/bin:$RTT_ENV:$RTT_EXEC_PATH:$PATH"

if [ ! -x "$RTT_EXEC_PATH/arm-none-eabi-gcc" ]; then
    echo "arm-none-eabi-gcc not found under RTT_EXEC_PATH=$RTT_EXEC_PATH" >&2
    exit 1
fi

BSP_DIR="$RTTHREAD_DIR/$STM32_BSP"
if [ ! -f "$BSP_DIR/SConstruct" ]; then
    echo "STM32 BSP SConstruct not found: $BSP_DIR/SConstruct" >&2
    exit 1
fi

cd "$BSP_DIR"
run_logged scons --pyconfig-silent
run_logged python3 "$RTT_ENV/env.py" package --upgrade
run_logged python3 "$RTT_ENV/env.py" package --update
verify_stm32_headers "$BSP_DIR" "$STM32_SERIES_LC"
verify_stm32_can_support "$BSP_DIR"
enable_hal_can_module "$BSP_DIR" "$STM32_SERIES_LC"

log "Stage package under $BSP_DIR/packages/$PACKAGE_NAME"
rm -rf "$BSP_DIR/packages/$PACKAGE_NAME"
mkdir -p "$BSP_DIR/packages"
rsync -a --delete \
    --exclude='.git' \
    --exclude='_ci' \
    --exclude='.github' \
    "$PACKAGE_ROOT/" "$BSP_DIR/packages/$PACKAGE_NAME/"

remove_ci_sconstruct_patch "$BSP_DIR/SConstruct"
apply_ci_config "$BSP_DIR"
cleanup_canopennode_build_artifacts "$BSP_DIR"

cd "$BSP_DIR"
run_logged scons -j"$(nproc)"
verify_outputs "$BSP_DIR"

log "CANOPENNODE_STM32F4_BUILD_PASS profile=$CANOPENNODE_CI_PROFILE"
