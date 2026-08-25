[English](../en/lss-persistence.md)

# LSS Node-ID 与 bitrate 持久化

本页说明 RT-Thread 端如何使用现有 RT-Thread Storage backend 保存和加载 LSS Node-ID/bitrate。该实现使用一个独立的单槽记录，不复用 `OD_PERSIST_COMM` 的数据布局。

## 适用范围

启用该功能后，启动顺序为：

```mermaid
flowchart TD
    A[CO_new] --> B[Prepare selected Storage auxiliary backend]
    B --> C[Read and validate LSS record]
    C --> D[Resolve pending Node-ID and bitrate]
    D --> E[CO_CANinit]
    E --> F[CO_LSSinit + bind LSS Store callback]
    F --> G[Initialize normal OD-backed Storage]
    G --> H[CO_CANopenInit]
```

这样第一次 `CO_CANinit()` 就能使用保存的 bitrate，同时不会在 CAN 初始化前调用正常 OD-backed Storage 的 `init`。普通 Communication Reset 只使用当前 RAM 中的 LSS pending 值，不重新读取持久记录；Reset Node 或真实重新上电后，应用初始化路径会再次读取记录。

当前功能提供：

- `CO_storage_rtt_backend_ops_t` 中独立的 `aux_init` / `aux_read` / `aux_write` auxiliary contract；
- 单槽记录的读取、格式校验、CRC 校验和 commit 校验；
- 单槽写入原语，按“先使旧 commit 无效，最后提交新 commit”的顺序写入；
- valid commit 写入后的完整记录 readback、decode、CRC、Node-ID 和 bitrate 复核；
- 启动前加载 Node-ID/bitrate；
- `CO_LSSslave_initCfgStoreCall()` 与 RT-Thread 单槽持久化的连接；
- bitrate 校验使用 CANopen LSS 标准 bit timing table；
- 仅当持久化 bitrate 与应用启动 bitrate 不同时，CAN 初始化失败才回退并重试启动 bitrate。

本功能会处理 LSS `Store configuration (0x17)`。运行时 bitrate activate 仍未实现；如果产品固定使用 1 Mbit/s，可以只使用 LSS Node-ID configure/store 路径。

## Kconfig

```text
PKG_CANOPENNODE_USING_LSS_SLAVE=y
PKG_CANOPENNODE_USING_STORAGE=y
PKG_CANOPENNODE_LSS_PERSIST=y
```

`PKG_CANOPENNODE_LSS_PERSIST` 对所有已选择的 RT-Thread Storage backend 都可用，不再依赖特定 backend，也不依赖 `PKG_CANOPENNODE_STORAGE_PERSIST_COMM`。

- DFS 使用 backend 自己管理的 `*_storage_aux.bin` 文件保存 auxiliary 数据，并通过独立 `aux_init` 在第一次 CAN 初始化前绑定实例。
- 通用 EEPROM Storage backend 通过 EEPROM device provider 访问介质；内置 AT24CXX provider 会自动保留配置 Storage 区域的最后一个 EEPROM page 作为 auxiliary 区域。
- 自定义 EEPROM provider 可以使用强符号替换内置 weak provider，需要实现 `co_storage_rtt_eeprom_module_get()` 和整组 `CO_eeprom_*` target hooks；启用 LSS persistence 时还需要实现 `co_storage_rtt_eeprom_provider_aux_init()`、`co_storage_rtt_eeprom_aux_read()` 与 `co_storage_rtt_eeprom_aux_write()`。
- 用户自定义 Storage backend 需要在 `CO_storage_rtt_backend_ops_t` 中实现 `aux_init`、`aux_read` 和 `aux_write`。其中 `aux_init` 是独立的 pre-CAN auxiliary 准备入口，不再复用正常 Storage `init`。

对于 AT24CXX backend，`PKG_CANOPENNODE_STORAGE_AT24C_OFFSET` 和 `PKG_CANOPENNODE_STORAGE_AT24C_REGION_SIZE` 仍定义由本软件包占用的完整 Storage 区域。启用 LSS persistence 时，该区域必须大于一个 EEPROM page，因为最后一个 page 会保留给 auxiliary persistence，正常 CANopen Storage 只能使用前面的区域。

## 单槽记录格式

固定记录长度为 16 bytes：

| Offset | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 4 | magic | ASCII `LSS1` |
| 4 | 1 | formatVersion | 当前为 `1` |
| 5 | 1 | flags | 当前必须为 `0` |
| 6 | 2 | length | 固定为 `16`，little-endian |
| 8 | 1 | nodeId | `1..127` 或 `0xFF` |
| 9 | 1 | reserved | 必须为 `0` |
| 10 | 2 | bitrate | kbit/s，little-endian |
| 12 | 2 | crc16 | 对 byte `0..11` 计算 CRC16-CCITT |
| 14 | 2 | commit | 有效值 `0xA55A` |

加载时只有完整通过 magic/version/length/reserved/CRC/commit、Node-ID 和 LSS timing-table 校验的记录才会覆盖应用启动值。

记录使用 byte-oriented C 结构体定义；CRC 与 commit 操作所需的记录长度和字段边界通过 `sizeof()` 与 `offsetof()` 推导，不再重复维护数值偏移宏。

## LSS Store 连接

`co_app_rtt_lss_init()` 在 `CO_LSSinit()` 成功后注册：

```c
CO_LSSslave_initCfgStoreCall(app->canOpenStack->LSSslave,
                             app,
                             co_app_rtt_lss_store_config);
```

CANopenNode 收到 LSS `0x17` 后，会把当前 pending Node-ID 和 bitrate 同步传入该 callback。callback 调用 `co_lss_persist_rtt_store()`；只有单槽记录完整提交并完成最终 readback/validation 后才返回 `true`，CANopenNode 才回复 Store success。

Communication Reset 会删除并重新创建 `CO_t`，因此 callback 放在 `co_app_rtt_lss_init()` 中绑定，确保每次重新创建 LSS slave object 后都会重新注册。

## 写入与掉电行为

单槽写入顺序：

```text
写 invalid commit
 -> 写 body + CRC
 -> readback 并比较 body
 -> 写 valid commit
 -> readback 完整记录
 -> 校验 magic/version/length/reserved/CRC/commit/Node-ID/bitrate
 -> 成功后返回 true
```

如果 valid commit 写接口失败，代码会尽力再次写入 invalid marker 后返回失败。如果 valid commit 已经写入，但最终完整 readback 或 decode/compare 失败，也会尽力重新写 invalid marker，然后向 LSS Master 返回 Store failed。

这里的重新 invalidate 是 best-effort：如果最终校验失败后介质同时也拒绝 invalidate 写入，调用仍返回失败，但介质上可能保留此前已提交的有效记录。该边界无法通过单槽和普通 read/write contract 完全消除；需要更强 rollback 语义时应由产品使用双槽或原子后端。

单槽方案不保留上一版本。如果在 invalid commit 写入后、valid commit 完成前掉电，下一次启动会发现记录无效并回退到应用传入的默认 Node-ID/bitrate。这是单槽方案的明确行为边界。

## Reset 语义

- 初次应用启动：先执行 auxiliary backend 准备并读取 LSS record，再执行第一次 `CO_CANinit()`；随后初始化 LSS、绑定 Store callback，再执行正常 OD-backed Storage 初始化。
- Communication Reset：重建 `CO_t`，保留 RAM 中已经配置的 pending Node-ID/bitrate，不重新读取 LSS record；新的 LSS object 会重新绑定 Store callback。
- Reset Node / power-cycle：重新走应用初始化，重新读取持久 record。

这与 CANopenNode LSS 对 pending Node-ID/bitrate 的生命周期要求一致：持久值在程序启动后初始化，Communication Reset 不应覆盖已经配置的 pending 值。

## 使用原始 CAN 帧验证 Node-ID configure/store

对于仅验证 Node-ID 修改与保存的 J08 类测试，不需要在 MCU 侧再实现 LSS Master。可以在 Linux Host 使用 SocketCAN/can-utils 直接发送 CiA 305 LSS 帧。

CAN-ID：

```text
LSS Master -> Slave : 0x7E5
LSS Slave  -> Master: 0x7E4
NMT                  : 0x000
```

先读取 DUT OD `0x1018:01..04`，然后按 little-endian 发送 selective 四步：

```text
7E5#40VVVVVVVV000000
7E5#41PPPPPPPP000000
7E5#42RRRRRRRR000000
7E5#43SSSSSSSS000000
```

最后一帧匹配后期望：

```text
7E4#4400000000000000
```

假设测试 Node-ID 为 `0x22`：

```text
# Configure Node-ID
7E5#1122000000000000
# expected: 7E4#1100000000000000

# Store configuration
7E5#1700000000000000
# expected: 7E4#1700000000000000
```

Configure 后、Communication Reset 前，`Inquire Node-ID (0x5E)` 返回的是 active Node-ID，而不是 pending Node-ID。因此此时查询仍可能返回旧 ID，这是 CANopenNode 当前语义，不表示 configure 失败。

如果旧 active Node-ID 为 1，应用新的 pending Node-ID：

```text
000#8201
```

期望看到新节点的 boot-up，例如 Node-ID `0x22`：

```text
722#00
```

J08-3 还应继续执行：

1. 对当前 Node-ID 发送 NMT Reset Node (`0x81`)；
2. 确认复位后仍以保存的新 Node-ID 上线；
3. 真实断电/上电；
4. 确认仍使用保存的新 Node-ID；
5. 最后使用 LSS configure + Store 恢复原 Node-ID，再 reset/power-cycle 复核。

## 错误处理

以下情况不会把未验证数据传入 `CO_CANinit()`：

- empty storage record；
- magic/version/length/reserved/commit 不匹配；
- CRC 错误；
- Node-ID 非法；
- bitrate 不在 CANopen LSS 标准 timing table 中；
- storage backend read 失败。

Store 时，body readback、commit write、最终完整 readback 或最终 decode/compare 任一步失败都会向 CANopenNode 返回 `false`，对应 LSS Store failed。

如果已经加载了有效持久记录，但第一次 `CO_CANinit()` 失败，仅当持久化 bitrate 与应用启动 bitrate 不同时才使用启动参数重试。`CO_CANinit()` 实际依赖的是 bitrate，因此两者相同时再次调用只会重复同一次 CAN 初始化，不再重试。

除 Storage 自身初始化的致命错误外，LSS record 无效时应用保留启动参数继续初始化 CAN。目标板仍应验证 Reset Communication、Reset Node、真实 power-cycle 和 Storage backend failure 场景。
