[中文](../zh/cia402-controller.md)

# CiA 402 Controller API

The CiA 402 Controller is a transport-agnostic Pure-C PDS command sequencer. It
observes a remote drive's Statusword, tracks an application-requested PDS target,
and returns the next update for PDS-owned Controlword bits.

![CiA 402 Controller boundary](../assets/cia402-controller-boundary.svg)

It is **not** a CANopen master framework. The application remains responsible
for remote Node-IDs, NMT, Heartbeat Consumer, SDO Client, PDO, SYNC, EDS/XDD
handling, commissioning, and multi-drive scheduling.

## Configuration and ownership

Enable `PKG_CANOPENNODE_CIA402_CONTROLLER` under the CiA 402 profile. The option
selects no transport feature and creates no RT-Thread task or heap allocation.
One caller-owned `CO_402_controller_axis_t` represents one remote axis.

The Controller owns only PDS Controlword bits 0..3 and Fault Reset bit 7. It
returns a `value + mask` update so PP/HM/Halt and other mode-specific bits remain
owned by the application.

## Minimal integration

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

/* Consume every valid PDS update independently of result. */
if (update.valid) {
    controlword = CO_402_controller_applyControlwordUpdate(controlword, &update);
    app_publish_remote_controlword(controlword);
}

switch (result) {
case CO_402_CONTROLLER_RESULT_IN_PROGRESS:
case CO_402_CONTROLLER_RESULT_TARGET_REACHED:
    break;
default:
    /* Application recovery policy. */
    break;
}
```

`app_remote_statusword_updated()`, `app_remote_statusword()`, and
`app_publish_remote_controlword()` are application functions. They may be backed
by PDO, SDO, another CANopen master stack, or a Host mock.

## PDS sequencing

For an Operation Enabled request, the Controller advances one observed state at
a time:

```text
Switch on disabled --Shutdown--> Ready to switch on
Ready to switch on --Switch on--> Switched on
Switched on --Enable operation--> Operation enabled
```

The Controller never assumes that one `0x000F` write can skip intermediate PDS
states. Lower-state requests use the transitions already accepted by the local
Device FSA: Disable voltage, Shutdown, or Disable operation as appropriate.

`Not ready to switch on` is observation-only; the Controller waits for the remote
drive's automatic transition. `Fault reaction active` is also observation-only
and blocks normal state commands.

## Fault and Quick Stop policy

Fault Reset is explicit. A remote Fault clears any previous target, and
`CO_402_controller_requestFaultReset()` is accepted only while the last valid
remote state is Fault. The Controller emits one Fault Reset assertion followed by
an explicit low update. Leaving Fault does not restore the previous Operation
Enabled target; the application must request a new target.

Quick Stop Active is a valid target, but automatic recovery from Quick Stop Active
to Operation Enabled is intentionally blocked. The Controller does not own the
remote quick-stop option-code policy. The application may explicitly request
Switch On Disabled and then start a new enable sequence.

## Timeout semantics

`transitionTimeout_us` bounds how long one observed PDS state may fail to make
progress toward the requested target. Each real remote state change resets that
timer. `feedbackTimeout_us` bounds the age of the last valid Statusword update.
A value of zero disables the corresponding timeout. Repeating the same active
target is idempotent and does not reset the transition timer; changing the target
starts a new transition interval.

Timeout values are product policy. The library does not invent defaults because
they depend on the master cycle, PDO/SDO strategy, remote drive, and network.

## Multi-drive use

There is no Controller manager and no Node-ID field. Applications compose as many
independent axes as needed:

```c
CO_402_controller_axis_t axes[3];
```

The application maps those instances to Node-IDs, buses, vendors, or gateways.
This keeps the PDS helper independent of network topology.
