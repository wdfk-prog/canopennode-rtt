# CiA 402 Controller

The Controller layer is a transport-agnostic Pure-C helper for controlling the
PDS state of a remote CiA 402 drive. It is not a CANopen master framework.

Role boundaries:

- `common/` contains role-neutral CiA 402 constants and stateless definitions.
- `device/` represents this local CANopen node as a CiA 402 drive Device.
- `controller/` observes a remote Statusword, tracks a requested PDS target, and
  generates the next PDS-owned Controlword update.

The application remains responsible for CANopen network services and transport,
including Node-ID ownership, NMT, Heartbeat Consumer, SDO Client, PDO, SYNC,
remote EDS/XDD handling, commissioning, and multi-drive scheduling.

`CO_402_controller_axis_t` intentionally contains no `CO_t`, CANopen transport
object, RT-Thread object, Node-ID, or heap-owned resource. One remote axis uses
one caller-owned Controller instance.

The Controller returns a Controlword `value + mask` update instead of replacing
the complete 16-bit Controlword. This keeps PP/HM/Halt and other mode-specific
bits under application ownership while the Controller owns only PDS bits 0..3
and Fault Reset bit 7.

Fault Reset is explicit and never restores a previous Operation Enabled target.
Quick Stop Active is not automatically resumed to Operation Enabled because the
Controller does not own the remote drive's quick-stop option policy.

See `docs/en/cia402-controller.md` or `docs/zh/cia402-controller.md` for the
public API and integration contract.
