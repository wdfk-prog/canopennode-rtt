# CiA 401 Controller

`CO_401_controller` is a transport-agnostic Pure-C helper for a remote CiA 401
logical device. It owns only the declared process-image shape and resolves
canonical `0x6000..0x67FF` objects through the common CiA 301 logical-device
layout. It owns no CANopen stack object, Node-ID, RTOS resource or heap memory.

`CO_401_controller_client` adds the reusable remote I/O control-plane behavior on
top of `CO_profile_transport`. It is still stack- and RTOS-neutral: another
system only needs to implement the mandatory common SDO upload/download
operations. NMT dispatch is optional unless the application also uses the
common NMT API.

`CO_401_controller_RTT` is now a thin compatibility facade. It binds that same
portable client to `CO_profile_master_RTT`, which keeps CANopenNode SDO/NMT work
inside the RT-Thread CANopen mainline owner.
