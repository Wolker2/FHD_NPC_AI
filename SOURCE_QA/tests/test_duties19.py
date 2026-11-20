"""Validate all new native entry points against the supported PE."""
import ctypes as C
import hashlib
import os
from pathlib import Path
import re
import struct

import pytest
from test_bridge import DLL, snapshot
from test_foundation18 import restore
from test_live_policy_07 import ship as native_ship, call, after, profile

ROOT = Path(__file__).resolve().parents[1]


def test_all_duty_calls_and_native_methods_match_exe():
    exe = os.environ.get('FHD_TARGET_EXE')
    if not exe:
        pytest.skip('set FHD_TARGET_EXE')
    data = Path(exe).read_bytes()
    assert hashlib.sha256(data).hexdigest() == 'e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf'
    source = (ROOT / 'src/fhd_native_duties.c').read_text()
    constants = dict(re.findall(r'([A-Z_]+)\s*=\s*(0x[\da-f]+)', source))
    routines = source.split('static const Signature routines[]={', 1)[1].split('};', 1)[0]
    sites = source.split('static const struct {Signature sig;const void *replacement;} sites[]={', 1)[1].split('};', 1)[0]
    entries = dict.fromkeys([0x315a34, 0x1a9d88, 0x1a4cd4, 0x2cc900, 0x1a0834], False)
    for text, patch in [(routines, False), (sites, True)]:
        matches = re.findall(r'\{(0x[\da-f]+|[A-Z_]+),\{([0-9a-fx, ]+)\}\}', text)
        assert len(matches) == (5 if patch else 12)
        for name, values in matches:
            address = int(constants.get(name, name), 16)
            expected = bytes(int(s, 0) for s in values.split(','))
            assert data[address - 0xc00:address - 0xc00 + 5] == expected
            if patch:
                assert expected[0] == 0xE8
                dest = address + 5 + struct.unpack('<i', expected[1:])[0]
                assert not entries[dest]
                entries[dest] = True
    assert all(entries.values())


def test_actual_18_snapshot_migrates_trade_and_relation():
    old = (Path(__file__).parent / 'fixtures' / 'fhd_npc_ai_1_8_trade.bin').read_bytes()
    assert old.startswith(b'FHDNPC04')
    DLL.FHD_BridgeNewGame()
    assert restore(old) == 2
    assert DLL.FHD_Relation(55, 77) == 9
    current = snapshot()
    assert current.startswith(b'FHDNPC08')
    assert restore(current) == 2
    assert snapshot() == current
    goal = (C.c_uint32 * 5)()
    DLL.FHD_GetTradeGoal.argtypes = [C.c_uint32, C.POINTER(C.c_uint32)]
    assert DLL.FHD_GetTradeGoal(55, goal)
    assert tuple(goal) == (15, 200, 4, 10, 16)


def test_verified_role_restores_for_same_captain_and_not_reused_id():
    n = 78001
    DLL.FHD_BridgeNewGame()
    DLL.FHDNPC_LiveReset()
    DLL.FHDNPC_RegisterVerifiedProfession.argtypes = [C.c_uint32] * 4
    assert DLL.FHDNPC_RegisterVerifiedProfession(n, 123456 + n, 2, 12) == 1
    ship = native_ship(n, which=1)
    after(ship, call(ship, 1), 1)
    assert profile(n).role == 6
    saved = snapshot()
    DLL.FHD_BridgeNewGame()
    DLL.FHDNPC_LiveReset()
    assert restore(saved) == 1
    after(ship, call(ship, 1), 1)
    assert profile(n).role == 6
    struct.pack_into('<I', ship, 0xe0, 9876)
    after(ship, call(ship, 1), 1)
    assert profile(n).role == 2
