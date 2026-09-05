# RT-Thread SCons script for CANopenNode RT-Thread port.

import os
from building import *

cwd = GetCurrentDir()
src = []
CPPPATH = [
    os.path.join(cwd, 'port', 'rtthread'),
    os.path.join(cwd, 'port', 'rtthread', 'demo'),
    os.path.join(cwd, 'port', 'rtthread', 'storage'),
    os.path.join(cwd, 'CANopenNode'),
    os.path.join(cwd, 'CANopenNode', '301'),
    os.path.join(cwd, 'CANopenNode', '303'),
    os.path.join(cwd, 'CANopenNode', '304'),
    os.path.join(cwd, 'CANopenNode', '305'),
    os.path.join(cwd, 'CANopenNode', '309'),
    os.path.join(cwd, 'CANopenNode', 'storage'),
    os.path.join(cwd, 'CANopenNode', 'extra'),
]


def _has_any(*names):
    for name in names:
        if GetDepend(name):
            return True
    return False


def _add_required_any(enabled, logical_name, candidates, hint=None):
    if not enabled:
        return None

    for path in candidates:
        abs_path = os.path.join(cwd, path)
        if os.path.isfile(abs_path):
            src.append(path)
            return path

    if hint is None:
        hint = 'Check CANopenNode submodule version/path or disable the related Kconfig option.'
    raise RuntimeError('Required CANopenNode source for %s is missing. Tried: %s. %s'
                       % (logical_name, ', '.join(candidates), hint))


def _add_required(path):
    abs_path = os.path.join(cwd, path)
    if not os.path.isfile(abs_path):
        raise RuntimeError('Required CANopenNode source is missing: %s' % path)
    src.append(path)


if GetDepend('PKG_USING_CANOPENNODE'):
    if GetDepend('PKG_CANOPENNODE_CIA402'):
        CPPPATH += [
            os.path.join(cwd, 'profile', 'cia402', 'common'),
        ]
        _add_required(os.path.join('profile', 'cia402', 'common', 'CO_402_state.c'))

    if GetDepend('PKG_CANOPENNODE_CIA402_DEVICE'):
        CPPPATH += [
            os.path.join(cwd, 'profile', 'cia402', 'device'),
            os.path.join(cwd, 'profile', 'cia402', 'device', 'modes'),
        ]
        _add_required(os.path.join('profile', 'cia402', 'device', 'CO_402_device.c'))
        _add_required(os.path.join('profile', 'cia402', 'device', 'CO_402_device_fsa.c'))
        _add_required(os.path.join('profile', 'cia402', 'device', 'CO_402_device_od.c'))
        _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_SYNC_FASTPATH'),
                          'CiA 402 cyclic synchronous bridge',
                          [os.path.join('profile', 'cia402', 'device', 'CO_402_device_sync.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_MODE_PP'),
                          'CiA 402 Profile Position mode',
                          [os.path.join('profile', 'cia402', 'device', 'modes', 'CO_402_mode_pp.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_MODE_PV'),
                          'CiA 402 Profile Velocity mode',
                          [os.path.join('profile', 'cia402', 'device', 'modes', 'CO_402_mode_pv.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_MODE_HM'),
                          'CiA 402 Homing mode',
                          [os.path.join('profile', 'cia402', 'device', 'modes', 'CO_402_mode_hm.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_MODE_CSP'),
                          'CiA 402 Cyclic Synchronous Position mode',
                          [os.path.join('profile', 'cia402', 'device', 'modes', 'CO_402_mode_csp.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_MODE_CSV'),
                          'CiA 402 Cyclic Synchronous Velocity mode',
                          [os.path.join('profile', 'cia402', 'device', 'modes', 'CO_402_mode_csv.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_MODE_CST'),
                          'CiA 402 Cyclic Synchronous Torque mode',
                          [os.path.join('profile', 'cia402', 'device', 'modes', 'CO_402_mode_cst.c')])

    if GetDepend('PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS'):
        _add_required(os.path.join('port', 'rtthread', 'CO_lifecycle_RTT.c'))

    if GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_RTT_THREAD'):
        CPPPATH += [os.path.join(cwd, 'profile', 'cia402', 'port', 'rtthread')]
        _add_required(os.path.join('profile', 'cia402', 'port', 'rtthread', 'CO_402_device_RTT.c'))

    _add_required_any(GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_RTT_MSH'),
                      'CiA 402 RT-Thread MSH debug control',
                      [os.path.join('profile', 'cia402', 'port', 'rtthread', 'CO_402_device_RTT_msh.c')])

    if GetDepend('PKG_CANOPENNODE_CIA402_DEVICE_RTT_DEMO'):
        _add_required(os.path.join('profile', 'cia402', 'demo', 'CO_402_device_RTT_demo.c'))

    canopennode_301_dir = os.path.join(cwd, 'CANopenNode', '301')
    if not os.path.isdir(canopennode_301_dir):
        raise RuntimeError('CANopenNode submodule is missing. Run: git submodule update --init --recursive')

    _add_required(os.path.join('port', 'rtthread', 'CO_driver_rtthread.c'))
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_RTT_CAN_FILTER'),
                      'RT-Thread coarse CAN ingress filter',
                      [os.path.join('port', 'rtthread', 'CO_can_filter_RTT.c')])
    _add_required(os.path.join('port', 'rtthread', 'CO_time_RTT.c'))
    _add_required_any(GetDepend('PKG_CANOPENNODE_GLOBAL_TIMERNEXT'),
                      'event-driven mainline scheduler',
                      [os.path.join('port', 'rtthread', 'CO_mainline_RTT.c')])
    _add_required(os.path.join('port', 'rtthread', 'CO_app_RTT.c'))
    _add_required_any(GetDepend('PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE'),
                      'ASCII gateway RT-Thread console bridge',
                      [os.path.join('port', 'rtthread', 'CO_gateway_RTT.c')])
    if (GetDepend('PKG_CANOPENNODE_DEMO_SRDO_DIAGNOSTIC')
            and not GetDepend('PKG_CANOPENNODE_USING_DEMO_OD')):
        raise RuntimeError(
            'PKG_CANOPENNODE_DEMO_SRDO_DIAGNOSTIC requires the generated demo OD; '
            'enable PKG_CANOPENNODE_USING_DEMO_OD or disable the SRDO fixture.')

    if GetDepend('PKG_CANOPENNODE_USING_DEMO_OD'):
        demo_enabled = (GetDepend('PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC')
                        or GetDepend('PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC')
                        or GetDepend('PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC')
                        or GetDepend('PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST')
                        or GetDepend('PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST')
                        or GetDepend('PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC')
                        or GetDepend('PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST')
                        or GetDepend('PKG_CANOPENNODE_DEMO_SRDO_DIAGNOSTIC'))
        _add_required_any(demo_enabled, 'demo dispatcher',
                          [os.path.join('port', 'rtthread', 'demo', 'CO_demo.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC'),
                          'demo TIME diagnostic', [os.path.join('port', 'rtthread', 'demo', 'CO_demo_time.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC'),
                          'demo EMCY consumer diagnostic',
                          [os.path.join('port', 'rtthread', 'demo', 'CO_demo_emcy_consumer.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC'),
                          'demo GFC diagnostic', [os.path.join('port', 'rtthread', 'demo', 'CO_demo_gfc.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST'),
                          'demo SDO server block test',
                          [os.path.join('port', 'rtthread', 'demo', 'CO_demo_sdo_block.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST'),
                          'demo SDO client test',
                          [os.path.join('port', 'rtthread', 'demo', 'CO_demo_sdo_client.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC'),
                          'demo Storage diagnostic',
                          [os.path.join('port', 'rtthread', 'demo', 'CO_demo_storage.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST'),
                          'demo NMT master test', [os.path.join('port', 'rtthread', 'demo', 'CO_demo_nmt_master.c')])
        _add_required_any(GetDepend('PKG_CANOPENNODE_DEMO_SRDO_DIAGNOSTIC'),
                          'demo SRDO diagnostic', [os.path.join('port', 'rtthread', 'demo', 'CO_demo_srdo.c')])

    _add_required(os.path.join('CANopenNode', 'CANopen.c'))
    _add_required(os.path.join('CANopenNode', '301', 'CO_NMT_Heartbeat.c'))
    _add_required(os.path.join('CANopenNode', '301', 'CO_ODinterface.c'))

    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_HB_CONS'),
                      'heartbeat consumer', [os.path.join('CANopenNode', '301', 'CO_HBconsumer.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_NODE_GUARDING'),
                      'node guarding', [os.path.join('CANopenNode', '301', 'CO_Node_Guarding.c')])
    _add_required(os.path.join('CANopenNode', '301', 'CO_Emergency.c'))
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_SDO_SERVER'),
                      'SDO server', [os.path.join('CANopenNode', '301', 'CO_SDOserver.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_SDO_CLIENT'),
                      'SDO client', [os.path.join('CANopenNode', '301', 'CO_SDOclient.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_TIME'),
                      'TIME object', [os.path.join('CANopenNode', '301', 'CO_TIME.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_SYNC'),
                      'SYNC object', [os.path.join('CANopenNode', '301', 'CO_SYNC.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_PDO'),
                      'PDO objects', [os.path.join('CANopenNode', '301', 'CO_PDO.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_LEDS'),
                      'CiA 303 LEDs', [os.path.join('CANopenNode', '303', 'CO_LEDs.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_GFC'),
                      'GFC object', [os.path.join('CANopenNode', '304', 'CO_GFC.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_SRDO'),
                      'SRDO object', [os.path.join('CANopenNode', '304', 'CO_SRDO.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_LSS_SLAVE'),
                      'LSS slave', [os.path.join('CANopenNode', '305', 'CO_LSSslave.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_LSS_MASTER'),
                      'LSS master', [os.path.join('CANopenNode', '305', 'CO_LSSmaster.c')])
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_GATEWAY_ASCII'),
                      'ASCII gateway', [os.path.join('CANopenNode', '309', 'CO_gateway_ascii.c')])
    src += SConscript(os.path.join(cwd, 'port', 'rtthread', 'storage', 'SConscript'))
    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_FIFO'),
                      'FIFO helper', [os.path.join('CANopenNode', '301', 'CO_fifo.c')])

    crc16_internal = (_has_any('PKG_CANOPENNODE_USING_CRC16', 'PKG_CANOPENNODE_FIFO_CRC16_CCITT')
                      and not GetDepend('PKG_CANOPENNODE_CRC16_EXTERNAL'))
    _add_required_any(crc16_internal, 'internal CRC16 helper', [os.path.join('CANopenNode', '301', 'crc16-ccitt.c')],
                      'Enable PKG_CANOPENNODE_CRC16_EXTERNAL only if the application provides CRC16.')

    _add_required_any(GetDepend('PKG_CANOPENNODE_USING_TRACE'),
                      'trace recorder', [os.path.join('CANopenNode', 'extra', 'CO_trace.c')])

    if GetDepend('PKG_CANOPENNODE_USING_DEMO_OD'):
        od_dir = os.path.join('examples', 'demo_device')
        od_c = os.path.join(od_dir, 'OD.c')
        od_h = os.path.join(cwd, od_dir, 'OD.h')
        if not os.path.isfile(od_h):
            raise RuntimeError('Required Object Dictionary header is missing: %s' % os.path.join(od_dir, 'OD.h'))
        _add_required(od_c)
        CPPPATH += [os.path.join(cwd, od_dir)]

LOCAL_CCFLAGS = ''
if GetDepend('PKG_USING_CANOPENNODE'):
    LOCAL_CCFLAGS += ' -DCO_DRIVER_CUSTOM'

group = DefineGroup('CANopenNode', src, depend=['PKG_USING_CANOPENNODE'], CPPPATH=CPPPATH, LOCAL_CCFLAGS=LOCAL_CCFLAGS)

Return('group')
