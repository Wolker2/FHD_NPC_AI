"""1.4.0 regression: relational temporary game fields and order-override contracts.
Linux fixture and static checks; not a running Windows game.
"""
from pathlib import Path
import ctypes as C
import struct
import pytest
from test_live_policy_07 import DLL, ship as native_ship, Frame
from test_bridge import record

ROOT=Path(__file__).resolve().parents[1]
DLL.FHDNPC_TestSetVerifiedTarget.argtypes=[C.c_uint32]
DLL.FHDNPC_Live140Stats.argtypes=[C.POINTER(C.c_uint32)]
DLL.FHDNPC_LiveBefore.argtypes=[C.POINTER(C.c_uint8),C.c_uint32,C.POINTER(Frame)]
DLL.FHDNPC_LiveAfter.argtypes=DLL.FHDNPC_LiveBefore.argtypes
DLL.FHD_Record.argtypes=[C.c_uint32]*5
DLL.FHD_Relation.argtypes=[C.c_uint32,C.c_uint32]
DLL.FHD_Relation.restype=C.c_int32


def stats():
    arr=(C.c_uint32*10)();DLL.FHDNPC_Live140Stats(arr)
    return list(arr)


def prepare(n, c, rel):
    DLL.FHD_BridgeNewGame();DLL.FHDNPC_LiveReset()
    a=native_ship(n, which=c, career=(40,30,30))
    if c==2:
        struct.pack_into('<f',a,0x518,3.0)
    struct.pack_into('<I',a,0x3f4,0x11223344)
    DLL.FHDNPC_TestSetVerifiedTarget(0)
    first=Frame();DLL.FHDNPC_LiveBefore(a,c,C.byref(first))
    DLL.FHDNPC_LiveAfter(a,c,C.byref(first))
    for day in range(101,110):
        event=11 if rel>0 else 2
        assert record(n,n+1,event,10,day)==1
    assert DLL.FHD_Relation(n,n+1)*(1 if rel>0 else -1)>15
    DLL.FHDNPC_TestSetVerifiedTarget(n+1)
    return a


@pytest.mark.parametrize('rel',[1,-1])
def test_ranger_relationship_changes_real_preferences_and_restores(rel):
    a=prepare(13001,0,rel)
    before=bytes(a)
    f=Frame();DLL.FHDNPC_LiveBefore(a,0,C.byref(f))
    assert f.valid==1 and f.changed==1
    assert f.used_career==(0 if rel>0 else 2)
    if rel>0:assert f.used_aggression<35
    else: assert f.used_aggression>=70
    info=stats()
    assert info[2]>=1  # real direct relation applied
    assert info[3]==0
    DLL.FHDNPC_LiveAfter(a,0,C.byref(f))
    assert bytes(a)==before  # original preference fields restored
    DLL.FHDNPC_TestSetVerifiedTarget(0)


@pytest.mark.parametrize('rel',[1,-1])
def test_pirate_direct_relation_affects_native_pressure_even_without_action_switch(rel):
    a=prepare(14001,2,rel)
    before=bytes(a)
    pressure=struct.unpack_from('<f',a,0x518)[0]
    f=Frame();DLL.FHDNPC_LiveBefore(a,2,C.byref(f))
    assert f.valid==1 and f.pirate_changed==1
    new=struct.unpack_from('<f',a,0x518)[0]
    if rel>0:assert new<pressure
    else: assert new>pressure
    info=stats()
    assert info[2]>=1
    assert info[5 if rel>0 else 4]>=1
    DLL.FHDNPC_LiveAfter(a,2,C.byref(f))
    assert bytes(a)==before
    DLL.FHDNPC_TestSetVerifiedTarget(0)


def test_no_fake_relation_target_never_applies_social_override():
    DLL.FHD_BridgeNewGame();DLL.FHDNPC_LiveReset()
    a=native_ship(15001,career=(40,30,30))
    struct.pack_into('<I',a,0x3f4,0x11223344)
    struct.pack_into('<I',a,0x430,0x12345678)
    DLL.FHDNPC_TestSetVerifiedTarget(0)
    original=bytes(a)
    f=Frame();DLL.FHDNPC_LiveBefore(a,0,C.byref(f))
    assert f.valid==1 and f.changed==0 and stats()[2]==0
    DLL.FHDNPC_LiveAfter(a,0,C.byref(f))
    assert bytes(a)==original


def test_transports_warriors_preserve_existing_orders_and_targets():
    source=(ROOT/'src/fhd_social_hooks.c').read_text()
    assert 'g_override_order[kind]++' in source
    assert 'g_override_existing[kind]++' in source
    assert 'g_override_flagship[kind]++' in source
    assert 'g_override_fear[kind]++' in source
    assert 'if(old_enemy){g_existing_target++;g_select_kind[kind][1]++;return;}' not in source
    assert 'if(category!=1u && category!=3u && (p[0x44cu]||p[0x43cu]))return 0;' in source
    assert 'read32(p,0x3ccu)' in source
    assert 'etype>=1u && etype<=4u && candidate(enemy,etype-1u)' in source
    assert 'dom=etype==0u && eowner==5u' in source
    assert '*(volatile uint32_t*)(p+0x3f4u)=0u' in source
    assert '*(volatile uint32_t*)(p+0x430u)' not in source



@pytest.mark.parametrize('category', [0, 2])
def test_established_orders_survive_hostility(category):
    ship = prepare(18001, category, -1)
    struct.pack_into('<I', ship, 0x430, 0x12345678)
    before = bytes(ship)
    frame = Frame()
    DLL.FHDNPC_LiveBefore(ship, category, C.byref(frame))
    DLL.FHDNPC_LiveAfter(ship, category, C.byref(frame))
    assert frame.valid and not frame.changed and not frame.pirate_changed
    assert bytes(ship) == before
    DLL.FHDNPC_TestSetVerifiedTarget(0)
