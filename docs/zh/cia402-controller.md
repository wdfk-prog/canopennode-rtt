[English](../en/cia402-controller.md)

# CiA 402 Controller API

CiA 402 Controller 是与 transport 无关的 Pure-C PDS 命令序列器。它观察远端
drive 的 Statusword、保存应用请求的 PDS 目标，并返回下一步 PDS Controlword
更新。

![CiA 402 Controller 边界](../assets/cia402-controller-boundary.svg)

它**不是 CANopen 主站框架**。远端 Node-ID、NMT、Heartbeat Consumer、SDO
Client、PDO、SYNC、EDS/XDD、commissioning 和多 drive 调度全部由应用负责。

## 配置与所有权

在 CiA 402 profile 下启用 `PKG_CANOPENNODE_CIA402_CONTROLLER`。该选项不会
自动选择任何 transport 功能，也不会创建 RT-Thread task 或 heap 资源。一个
caller-owned `CO_402_controller_axis_t` 对应一个远端 axis。

Controller 只拥有 Controlword 的 PDS bits 0..3 与 Fault Reset bit 7。API 返回
`value + mask`，因此 PP/HM/Halt 等 mode-specific bits 继续由应用所有。

## 最小集成示例

```c
CO_402_controller_config_t cfg = {
    .transitionTimeout_us = 500000U,
    .feedbackTimeout_us = 100000U,
};
CO_402_controller_axis_t axis;
CO_402_controller_feedback_t feedback;
CO_402_controller_controlword_update_t update;
uint16_t controlword = 0U;

CO_402_controller_init(&axis, &cfg);
CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED);

feedback.statuswordValid = app_remote_statusword_updated();
feedback.statusword = app_remote_statusword();

CO_402_controller_result_t result =
    CO_402_controller_process(&axis, &feedback, cycle_us, &update);

/* 任何 valid PDS update 都必须独立于 result 被消费。 */
if (update.valid) {
    controlword = CO_402_controller_applyControlwordUpdate(controlword, &update);
    app_publish_remote_controlword(controlword);
}

switch (result) {
case CO_402_CONTROLLER_RESULT_IN_PROGRESS:
case CO_402_CONTROLLER_RESULT_TARGET_REACHED:
    break;
default:
    /* 由应用决定恢复策略。 */
    break;
}
```

`app_remote_statusword_updated()`、`app_remote_statusword()` 和
`app_publish_remote_controlword()` 都是应用函数，可以由 PDO、SDO、其他
CANopen master stack 或 Host mock 实现。

## PDS 命令序列

请求 Operation Enabled 时，Controller 只根据已经观察到的状态逐步推进：

```text
Switch on disabled --Shutdown--> Ready to switch on
Ready to switch on --Switch on--> Switched on
Switched on --Enable operation--> Operation enabled
```

Controller 不假设一次写入 `0x000F` 就能跳过中间 PDS state。降低目标状态时，
使用当前本地 Device FSA 已接受的 Disable voltage、Shutdown 或 Disable
operation transition。

`Not ready to switch on` 只用于观察，Controller 等待远端 drive 自动进入下一
状态；`Fault reaction active` 同样只观察，不发送正常状态命令。

## Fault 与 Quick Stop 策略

Fault Reset 必须显式请求。观察到远端 Fault 后，Controller 会清除此前 target；
只有最后一个有效远端状态确认为 Fault 时，
`CO_402_controller_requestFaultReset()` 才接受请求。Controller 输出一次 Fault
Reset 高电平，并在下一次 process 明确输出低电平。远端离开 Fault 后不会自动
恢复之前的 Operation Enabled target，应用必须重新设置 target。

Quick Stop Active 可以作为目标，但第一版明确禁止从 Quick Stop Active 自动恢复
Operation Enabled。Controller 不拥有远端 quick-stop option-code policy。应用可
显式请求 Switch On Disabled，再重新开始 enable sequence。

## Timeout 语义

`transitionTimeout_us` 限制远端在同一个 PDS state 中长时间不向目标推进；每次
真实状态变化都会重新计时。`feedbackTimeout_us` 限制最后一次有效 Statusword
更新的年龄。配置为 0 表示禁用对应 timeout。重复设置相同的 active target
是幂等操作，不会重置 transition timer；只有实际更换 target 才重新开始计时。

Timeout 属于产品策略，library 不提供隐藏默认值，因为它取决于 master cycle、
PDO/SDO 策略、远端 drive 和实际网络。

## 多 drive 使用

Controller 不提供 manager，也不保存 Node-ID。应用直接组合多个独立实例：

```c
CO_402_controller_axis_t axes[3];
```

Node-ID、CAN channel、vendor 和 gateway 的映射全部由应用负责，因此 PDS helper
不依赖网络拓扑。

## 验证边界

CI 内 Host test 覆盖状态序列、降低目标、transition/feedback timeout、Fault/Fault
Reset、Quick Stop policy、未知 Statusword、Controlword mask 保留、非法请求和多
axis 隔离。Host test 只证明 Pure-C 逻辑，不证明真实 CAN master、drive 互操作、
总线时序或 HIL 行为。
