"""Regression coverage for persistent identities and legacy 1.7 memory."""
import ctypes as C
import os
import struct
from pathlib import Path

import pytest
from test_bridge import DLL, ship, put, record, snapshot
from test_live_policy_07 import ship as live_ship, call, after

for name, n in [('FHD_RelationPeer', 3), ('FHD_RecordPeer', 6),
                ('FHD_FirstStrike', 5), ('FHD_NoteFirstStrike', 6),
                ('FHD_BindLegacyPlayer', 2)]:
    getattr(DLL, name).argtypes = [C.c_uint32] * n
DLL.FHD_NoteFirstStrike.restype = None
DLL.FHD_BindLegacyPlayer.restype = None


def restore(data):
    buf = (C.c_ubyte * len(data)).from_buffer_copy(data)
    return DLL.FHD_Restore(buf, len(data))


def test_relation_decay_depends_on_time_instead_of_update_frequency():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=55)) == 1
    assert DLL.FHD_RecordPeer(55, 77, 301, 2, 2, 100) == 1
    initial = snapshot()
    assert DLL.FHD_RelationPeer(55, 77, 301) == -15
    for day in range(101, 251):
        DLL.FHD_BridgeDay(day)
    stepwise = DLL.FHD_RelationPeer(55, 77, 301)
    assert restore(initial) == 1
    DLL.FHD_BridgeDay(250)
    assert DLL.FHD_RelationPeer(55, 77, 301) == stepwise == -10


def test_reused_peer_has_no_old_debts_or_friendships():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=55, seed=1001)) == 1
    assert put(ship(id=77, seed=301)) == 1
    assert record(55, 77, 3, 5) == 1
    assert DLL.FHD_Relation(55, 77) == -67
    DLL.FHD_BridgeDay(500)
    assert DLL.FHD_Relation(55, 77) == -65
    assert put(ship(id=77, seed=302)) == 1
    assert DLL.FHD_Relation(55, 77) == 0
    assert record(55, 77, 1, 2, 501) == 1
    assert DLL.FHD_RelationPeer(55, 77, 302) == 9
    assert DLL.FHD_RelationPeer(55, 77, 301) == 0


def test_first_strike_survives_save_and_only_applies_to_same_incident():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=55, seed=1001)) == 1
    DLL.FHD_NoteFirstStrike(55, 1001, 800, 88, 100, 1)
    DLL.FHD_NoteFirstStrike(55, 1001, 800, 88, 101, 2)
    state = snapshot()
    assert state.startswith(b'FHDNPC08')
    DLL.FHD_BridgeNewGame()
    assert restore(state) == 1
    assert DLL.FHD_FirstStrike(55, 1001, 800, 88, 115) == 1
    assert DLL.FHD_FirstStrike(55, 1001, 800, 88, 116) == 0
    assert DLL.FHD_FirstStrike(55, 1001, 800, 89, 115) == 0
    assert DLL.FHD_FirstStrike(55, 1001, 801, 88, 115) == 0
    assert put(ship(id=55, seed=1002)) == 1
    assert DLL.FHD_FirstStrike(55, 1002, 800, 88, 115) == 0


def test_legacy_snapshot_preserves_known_npcs_and_can_bind_current_player_once():
    old = (Path(__file__).parent / 'fixtures' / 'fhd_npc_ai_1_7_contacts.bin').read_bytes()
    DLL.FHD_BridgeNewGame()
    assert restore(old) == 2
    assert DLL.FHD_RelationPeer(55, 77, 301) == -15
    assert DLL.FHD_RelationPeer(55, 90, 901) == 0
    DLL.FHD_BindLegacyPlayer(90, 901)
    DLL.FHD_BindLegacyPlayer(91, 902)
    assert DLL.FHD_RelationPeer(55, 90, 901) == 9
    assert DLL.FHD_RelationPeer(55, 91, 902) == 0
    assert restore(snapshot()) == 2
    assert DLL.FHD_RelationPeer(55, 90, 901) == 9


@pytest.mark.parametrize('category,subtype', [(4, 0), (3, 1)])
def test_protected_native_classes_are_not_registered(category, subtype):
    DLL.FHD_BridgeNewGame()
    a = live_ship(42000, which=category)
    a[0x510] = subtype
    before = bytes(a)
    frame = call(a, category)
    after(a, frame, category)
    assert not frame.valid and bytes(a) == before and DLL.FHD_Count() == 0


def test_exact_trade_call_sites_and_callees():
    exe = os.environ.get('FHD_TARGET_EXE')
    if exe is None:
        pytest.skip('set FHD_TARGET_EXE to run against the supported game binary')
    import hashlib
    data = Path(exe).read_bytes()
    assert hashlib.sha256(data).hexdigest() == 'e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf'

    def at(rva):
        return data[rva - 0xc00:rva - 0xc00 + 5]

    for callsite, routine, prologue in [
        (0x1ac35e, 0x1acae4, '53565783c4'),
        (0x1ac60a, 0x1ac84c, '538bd88bc3'),
    ]:
        op = at(callsite)
        assert op[0] == 0xe8
        assert callsite + 5 + struct.unpack('<i', op[1:])[0] == routine
        assert at(routine) == bytes.fromhex(prologue)
    for routine, prologue in [
        (0x2d52ac, '558bec81c4'), (0x2d5bfc, '5356575583'),
        (0x2d5638, '5356575583'), (0x2c9a68, '535657558b'),
        (0x347300, 'a1fc008100'),
    ]:
        assert at(routine) == bytes.fromhex(prologue)
