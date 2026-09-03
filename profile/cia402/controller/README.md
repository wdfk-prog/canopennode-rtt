# CiA 402 Controller boundary

This directory reserves the Controller role for future support. It does
not compile a Controller implementation.

Role boundaries:

- `common/` contains role-neutral CiA 402 constants and stateless definitions.
- `device/` represents this local CANopen node as a CiA 402 drive Device.
- `controller/` will represent an application that controls a remote CiA 402
  Device through CANopen services.

The Controller layer may later use SDO Client, RPDO/TPDO, NMT, or other remote
transport mechanisms, but those dependencies must not move into `common/` or
become prerequisites for the local Device implementation.

This boundary intentionally provides no Controller source files, runtime state
machine, remote SDO policy, or RT-Thread task.
