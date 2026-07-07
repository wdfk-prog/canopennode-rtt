# RT-Thread SCons script for CANopenNode RT-Thread port.

import os
from building import *

cwd = GetCurrentDir()
src = []
CPPPATH = [
    os.path.join(cwd, 'port', 'rtthread'),
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
    canopennode_301_dir = os.path.join(cwd, 'CANopenNode', '301')
    if not os.path.isdir(canopennode_301_dir):
        raise RuntimeError('CANopenNode submodule is missing. Run: git submodule update --init --recursive')

    _add_required(os.path.join('port', 'rtthread', 'CO_driver_rtthread.c'))
    _add_required(os.path.join('port', 'rtthread', 'CO_app_RTT.c'))

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
