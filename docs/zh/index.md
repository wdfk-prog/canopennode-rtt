[English](../en/index.md)

# CANopenNode RT-Thread 文档

本文档作为项目手册，面向 RT-Thread BSP 开发者、固件集成者和 CANopen 设备开发者。

## 推荐阅读顺序

1. [快速接入](quick-start.md)——把 package 加入 RT-Thread BSP 并启动第一个节点。
2. [RT-Thread 集成说明](rt-thread-integration.md)——运行时 ownership、线程、锁、reset lifecycle 与 CAN device 交互。
3. [配置指南](configuration.md)——Kconfig 功能组及依赖关系。
4. [Object Dictionary](object-dictionary.md)——demo OD 用法与产品 OD 替换方式。
5. [High-Resolution Time](high-resolution-time.md)——可选硬件微秒时间源。
6. [LSS 持久化](lss-persistence.md)——Node-ID/bitrate 持久化和运行时 bitrate 切换。
7. [CiA 402 Device Core](cia402-device-core.md)——Pure-C 多轴 PDS 与 OD binding。
8. [CiA 402 RT-Thread 集成](cia402-device-rtt.md)——lifecycle、线程和 Communication Reset。
9. [CiA 402 诊断与产品集成](cia402.md)——每轴诊断与产品 OD 使用方式。
10. [CiA 402 Controller API](cia402-controller.md)——远端 drive 的 PDS Controlword 状态推进。
11. [子模块更新](submodule-update.md)——初始化、更新和锁定 CANopenNode。
12. [故障排查](troubleshooting.md)——常见构建、CAN、OD、Storage 和运行问题。

## 文档地图

| 文档 | 内容 |
|---|---|
| [快速接入](quick-start.md) | Package 接入、初始配置、构建入口与节点启动。 |
| [RT-Thread 集成说明](rt-thread-integration.md) | 运行时结构、调度、ownership、lifecycle 与 reset 行为。 |
| [配置指南](configuration.md) | Kconfig 功能组、依赖、默认值和集成选择。 |
| [Object Dictionary](object-dictionary.md) | 生成式 demo OD、自定义 OD 集成和 demo-only 对象。 |
| [High-Resolution Time](high-resolution-time.md) | timer source 契约和时间位宽限制。 |
| [LSS 持久化](lss-persistence.md) | LSS record、启动加载、bitrate 切换与恢复策略。 |
| [CiA 402 Device Core](cia402-device-core.md) | 多轴 Device runtime、PDS supervisor、OD binding 与 DriveIF 契约。 |
| [CiA 402 RT-Thread 集成](cia402-device-rtt.md) | Device lifecycle attach、worker thread、锁顺序和 reset/rebind。 |
| [CiA 402 诊断与产品集成](cia402.md) | Error-code/EMCY bridge 与产品生成 OD。 |
| [CiA 402 Controller API](cia402-controller.md) | 与 transport 无关的远端 PDS target 状态推进。 |
| [子模块更新](submodule-update.md) | CANopenNode submodule 维护。 |
| [故障排查](troubleshooting.md) | 常见集成失败的定位入口。 |

## 仓库入口

- [English README](../../README.md)
- [中文 README](../../README.zh-CN.md)
