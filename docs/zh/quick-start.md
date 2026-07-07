[English](../en/quick-start.md)

# 快速接入

本文用于在一个已有可用 CAN 设备的 RT-Thread BSP 中拉起一个 CANopenNode RT-Thread 实例。

## 1. 前置条件

开启软件包前，先确认以下条件：

| 项目 | 要求 |
|---|---|
| RT-Thread CAN 驱动 | BSP 必须导出 CAN 设备，例如 `can0` 或 `can1`。 |
| CAN 收发器 | 板卡必须具备物理 CAN 收发器，并正确接入终端电阻。 |
| Bitrate | 总线上所有节点必须使用相同 bitrate。 |
| CANopen Node-ID | 选择的 Node-ID 必须在 CANopen 网络内唯一。 |
| 子模块 | 子模块初始化后必须存在 `CANopenNode/301`。 |

## 2. 克隆仓库

新拉取仓库时：

```sh
git clone --recursive <repo-url> canopennode-rtt
cd canopennode-rtt
```

已有仓库时：

```sh
git submodule update --init --recursive
```

本软件包构建时要求 `CANopenNode` 子模块位于软件包根目录。更新和恢复流程见[子模块更新说明](submodule-update.md)。

## 3. 添加到 RT-Thread 工程

保持仓库目录结构不变，并让 RT-Thread 工程能够找到本软件包。父工程需要在 menu tree 中包含本包的 `Kconfig`，并在 SCons 构建路径中包含本包的 `SConscript`。

典型本地目录：

```text
rt-thread-project/
├── bsp/<board>/
├── packages/
│   └── canopennode-rtt/
│       ├── CANopenNode/
│       ├── Kconfig
│       └── SConscript
└── rt-thread/
```

具体父级 Kconfig/SCons 入口取决于项目的软件包管理方式。本软件包自身只要求根目录内的相对路径保持不变。

## 4. 启用必要 RT-Thread 功能

在 BSP 配置中启用 `PKG_USING_CANOPENNODE` 所需的 RT-Thread 核心能力：

```text
RT_USING_HEAP
RT_USING_DEVICE
RT_USING_CAN
RT_USING_MUTEX
RT_USING_SEMAPHORE
```

按需启用可选能力：

```text
RT_CAN_USING_HDR     # 可选硬件 CAN 过滤器路径
RT_USING_ULOG        # 可选调试日志
RT_USING_PIN         # 可选 CANopen LED GPIO 输出
RT_USING_DFS         # 可选 DFS storage backend
PKG_USING_AT24CXX    # 可选 EEPROM storage backend
```

## 5. 配置软件包

运行 `menuconfig`，开启软件包：

```text
PKG_USING_CANOPENNODE=y
```

首次 demo 建议保留大部分默认值，并确认以下配置：

| 选项 | 首次建议值 |
|---|---|
| `PKG_CANOPENNODE_CAN_DEV_NAME` | auto init 使用的 BSP 实际 CAN 设备名，例如 `can1`。 |
| `PKG_CANOPENNODE_APP_AUTO_INIT` | 首次 demo 建议为 `y`。 |
| `PKG_CANOPENNODE_AUTO_INIT_NODE_ID` | `1` 到 `127` 范围内的唯一值。 |
| `PKG_CANOPENNODE_AUTO_INIT_BITRATE` | 总线 bitrate，例如 `1000`。 |
| `PKG_CANOPENNODE_USING_DEMO_OD` | 首次 bring-up 建议为 `y`。 |
| `PKG_CANOPENNODE_USING_SDO_SERVER` | 需要 SDO 访问时为 `y`。 |
| `PKG_CANOPENNODE_USING_PDO` | 需要 PDO demo 行为时为 `y`。 |

## 6. 构建

在 RT-Thread BSP 或应用构建目录执行：

```sh
scons
```

如果构建报 CANopenNode 源文件路径缺失，执行：

```sh
git submodule update --init --recursive
```

如果构建报 `examples/demo_device/OD.h` 缺失，只有在 demo OD 文件存在时才保持 `PKG_CANOPENNODE_USING_DEMO_OD=y`。应用提供自定义 OD 时应关闭该选项。

## 7. 运行验证

烧录目标板后，按以下顺序确认：

1. RT-Thread CAN 设备打开成功。
2. BSP CAN 驱动接受配置的 bitrate。
3. CANopen 节点启动后发送 boot-up 帧。
4. 若启用 SDO server，CANopen master 或 tester 能够通过 SDO 访问 OD entry。
5. 若启用 PDO 且使用 demo OD，TPDO/RPDO 行为符合 demo mapping。

Node-ID 为 `1` 时，常见 boot-up 帧 COB-ID 为 `0x701`，数据为一个字节 `0x00`。

## 8. 手动初始化路径

单实例 demo 使用自动初始化较方便。若应用需要显式控制实例创建，请关闭 `PKG_CANOPENNODE_APP_AUTO_INIT`。

```c
#include "CO_app_RTT.h"

static CANopenNodeRTT canopen_app;

static int app_canopen_init(void)
{
    return (int)canopen_app_rtt_init(&canopen_app, "can1", 1, 1000);
}
INIT_APP_EXPORT(app_canopen_init);
```

手动初始化规则：

- `CANopenNodeRTT` 首次使用前必须为零初始化。
- `canName` 必须匹配 `rt_device_find()` 能找到的 RT-Thread CAN 设备名。
- `nodeID` 必须为 `1..127`；除非启用 LSS slave 并明确使用未配置分配值。
- `bitrate` 单位是 kbit/s。
- 不要在同一 CAN 设备和 Node-ID 上同时保留自动初始化和手动实例。

## 9. 下一步

- 修改线程优先级或从应用代码调用 CANopenNode API 前，阅读 [RT-Thread 集成说明](rt-thread-integration.md)。
- 启用可选协议组前，阅读[配置指南](configuration.md)。
- 替换 demo OD 前，阅读 [Object Dictionary 指南](object-dictionary.md)。
