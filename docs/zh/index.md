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
7. 与 Linux Host 联合执行协议验证前，阅读[测试与验证](testing.md)。
8. 更新 CANopenNode 前，阅读[子模块更新说明](submodule-update.md)。
9. 遇到构建或运行异常时，参考[故障排查](troubleshooting.md)。

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
| [子模块更新说明](submodule-update.md) | 如何克隆、初始化、更新或锁定 CANopenNode 子模块？ |
| [故障排查](troubleshooting.md) | 如何定位常见构建、CAN、SDO、PDO、storage 或 trace 问题？ |

## 仓库入口

- [English README](../../README.md)
- [中文 README](../../README.zh-CN.md)
- [配套 Linux Host/主站测试仓库](https://github.com/wdfk-prog/canopen-slave-tester)
