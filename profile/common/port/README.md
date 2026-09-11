# Device Profile Controller transport port

`CO_profile_transport` is the portable boundary between Device Profile Controller
logic and a concrete CANopen/OS backend. It contains no CANopenNode, RT-Thread,
Lely, BSP, or heap dependency.

A backend provides one `CO_profile_transport_ops_t` table for the operations used
by the current control-plane clients:

- bounded scalar SDO upload;
- bounded scalar SDO download;
- optional CiA 301 NMT command dispatch.

The profile clients under `profile/cia401/controller/` and
`profile/cia402/controller/` consume only this interface. Backend-specific
thread ownership, request queues, reset cancellation, timeout implementation,
and CANopen stack objects stay behind the port.

The CANopenNode RT-Thread implementation is
`port/rtthread/CO_profile_master_RTT.*`. Other systems can reuse the same
401/402 client code by implementing the two mandatory SDO operations and binding
them with `CO_profileTransport_init()`. A backend only needs to provide
`nmtCommand` when its application uses the common NMT command API; otherwise
`CO_profileTransport_nmtCommand()` reports `CO_PROFILE_CALL_NOT_READY`.

The portable transport is intended for commissioning and control-plane work.
Cyclic PDO/SYNC data paths have different timing and process-image requirements
and should use a dedicated realtime interface rather than extending these
blocking SDO operations into a generic CAN-frame abstraction.
