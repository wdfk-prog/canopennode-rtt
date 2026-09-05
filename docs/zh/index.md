[English](../en/index.md)

# CANopenNode RT-Thread 文档

本文档面向 RT-Thread BSP 开发者、固件集成者和 CANopen 设备开发者组织。

## 阅读顺序

1. 阅读[快速接入](quick-start.md)，在 RT-Thread BSP 中完成构建和运行验证。
2. 阅读 [RT-Thread 集成说明](rt-thread-integration.md)，理解运行时所有权、线程、锁和 CAN 设备交互。
3. 修改 Kconfig 功能组前，阅读[配置指南](configuration.md)。
4. 启用硬件微秒时间源前，阅读 [High-Resolution Time 集成说明](high-resolution-time.md)。
5. 使用 LSS 持久化时，阅读 [LSS Node-ID 与 bitrate 持久化](lss-persistence.md)。
6. 替换 demo OD 前，阅读 [Object Dictionary 指南](object-dictionary.md)。
7. 阅读 [CiA 402 Device 交付、诊断与互操作](cia402.md)，了解 A7 EMCY、发布 artifact 与产品验收。
8. 集成 CiA 402 Device core 前，阅读 [CiA 402 Pure-C Device Core 与 OD Binding](cia402-device-core.md)。
9. 接入 RT-Thread 线程 与 Communication Reset 前，阅读 [CiA 402 RT-Thread Device Thread](cia402-device-rtt.md)。
10. 应用需要控制远端 CiA 402 drive 时，阅读 [CiA 402 Controller API](cia402-controller.md)。
11. 与 Linux Host 联合执行协议验证前，阅读[测试与验证](testing.md)。
12. 更新 CANopenNode 前，阅读[子模块更新说明](submodule-update.md)。
13. 遇到构建或运行异常时，参考[故障排查](troubleshooting.md)。

## 文档地图

| 文档 | 解决的问题 |
|---|---|
| [快速接入](quick-start.md) | 如何快速添加、配置、构建并验证本软件包？ |
| [RT-Thread 集成说明](rt-thread-integration.md) | RT-Thread 运行封装如何与 CANopenNode 和 CAN 驱动交互？ |
| [配置指南](configuration.md) | 运行时、协议对象、storage、日志和调试相关 Kconfig 选项如何选择？ |
| [High-Resolution Time 集成说明](high-resolution-time.md) | High-Res Timer 的 1 MHz/32-bit/单实例约束以及当前 API 的位宽识别限制是什么？ |
| [LSS Node-ID 与 bitrate 持久化](lss-persistence.md) | 如何在第一次 CAN 初始化前从选定的 Storage backend 安全加载 LSS 配置？ |
| [测试与验证](testing.md) | MCU 侧测试夹具如何与 `canopen-slave-tester` 主站代码协同？哪些证据属于 Host、目标板或 HIL？ |
| [NMT Master 自动测试](nmt-master-test.md) | 如何让 MCU 自动驱动 Linux Lely Slave 验证 NMT Master 命令序列？ |
| [Object Dictionary 指南](object-dictionary.md) | 如何使用或替换 demo OD？ |
| [CiA 402 Device 交付、诊断与互操作](cia402.md) | A7 如何完成 axis fault→EMCY、release artifact 校验和互操作验收？ |
| [CiA 402 Pure-C Device Core 与 OD Binding](cia402-device-core.md) | A3 多轴 Device core 如何绑定 OD、运行 PDS FSA 并验证 DriveIF？ |
| [CiA 402 RT-Thread Device Thread](cia402-device-rtt.md) | A4 如何安全接入 thread、锁顺序和 Communication Reset？ |
| [CiA 402 Controller API](cia402-controller.md) | Pure-C Controller 如何在不接管 NMT/SDO/PDO/SYNC 的前提下控制远端 PDS？ |
| [子模块更新说明](submodule-update.md) | 如何克隆、初始化、更新或锁定 CANopenNode 子模块？ |
| [故障排查](troubleshooting.md) | 如何定位常见构建、CAN、SDO、PDO、storage 或 trace 问题？ |

## 仓库入口

- [English README](../../README.md)
- [中文 README](../../README.zh-CN.md)
- [配套 Linux Host/主站测试仓库](https://github.com/wdfk-prog/canopen-slave-tester)
