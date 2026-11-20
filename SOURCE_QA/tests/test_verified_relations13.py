"""1.3 positive escort/day and relationship-biased decisions.
Unit fixtures. Native Windows executable execution is tested only by the user.
"""
from pathlib import Path
import ctypes as C
import struct
from test_bridge import DLL, ship, put, record, snapshot
from test_live_policy_07 import ship as live_ship, Frame
from test_core_v03 import situation, choose, LIB

ROOT=Path(__file__).resolve().parents[1]
RULE=C.CDLL(str(ROOT/'build/libfhd_social_rules_test.so'))
RULE.FHDNPC_EscortCredit.argtypes=[C.c_uint32]*5+[C.c_int32]
RULE.FHDNPC_EscortCredit.restype=C.c_int32
DLL.FHD_RelationLastDay.argtypes=[C.c_uint32,C.c_uint32]
DLL.FHD_RelationLastDay.restype=C.c_uint32
DLL.FHD_Relation.argtypes=[C.c_uint32,C.c_uint32]
DLL.FHD_Relation.restype=C.c_int32
DLL.FHD_Restore.argtypes=[C.c_void_p,C.c_uint32]
DLL.FHD_Restore.restype=C.c_int32
DLL.FHDNPC_TestSetVerifiedTarget.argtypes=[C.c_uint32]
DLL.FHDNPC_LiveBefore.argtypes=[C.POINTER(C.c_uint8),C.c_uint32,C.POINTER(Frame)]
DLL.FHDNPC_LiveAfter.argtypes=DLL.FHDNPC_LiveBefore.argtypes
DLL.FHDNPC_LiveReset.argtypes=[]
DLL.FHDNPC_LiveStatsGet.argtypes=[C.c_void_p]


def test_escort_requires_verified_native_current_partner():
    for index in range(6):
        params=[1,1,1,100,99,25]
        if index==0: params[0]=0
        if index==1: params[1]=0
        if index==2: params[2]=0
        if index==3: params[3]=0
        if index==4: params[4]=100
        if index==5: params[5]=0
        assert RULE.FHDNPC_EscortCredit(*params)==0
    assert RULE.FHDNPC_EscortCredit(1,1,1,101,100,25)==1
    assert RULE.FHDNPC_EscortCredit(1,1,1,101,100,44)==1
    assert RULE.FHDNPC_EscortCredit(1,1,1,101,100,45)==0
    assert RULE.FHDNPC_EscortCredit(1,1,1,101,100,-4)==0


def test_last_event_day_persists_without_modifying_snapshot_format():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=8121))==1
    assert put(ship(id=8122))==1
    assert DLL.FHD_RelationLastDay(8121,8122)==0
    assert record(8121,8122,11,10,201)==1
    assert DLL.FHD_Relation(8121,8122)==25
    assert DLL.FHD_RelationLastDay(8121,8122)==201
    assert RULE.FHDNPC_EscortCredit(1,1,1,201,DLL.FHD_RelationLastDay(8121,8122),25)==0
    assert RULE.FHDNPC_EscortCredit(1,1,1,202,DLL.FHD_RelationLastDay(8121,8122),25)==1
    assert record(8121,8122,11,2,202)==1
    state=snapshot()
    DLL.FHD_BridgeNewGame()
    data=(C.c_ubyte*len(state)).from_buffer_copy(state)
    assert DLL.FHD_Restore(data,len(state))==2
    assert DLL.FHD_RelationLastDay(8121,8122)==202
    assert DLL.FHD_Relation(8121,8122)==30
    assert RULE.FHDNPC_EscortCredit(1,1,1,202,202,30)==0
    assert RULE.FHDNPC_EscortCredit(1,1,1,203,202,30)==1


def test_zero_relation_does_not_add_fictitious_target_credit():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=8221))==1
    assert DLL.FHD_RelationLastDay(8221,8222)==0
    assert RULE.FHDNPC_EscortCredit(1,1,1,100,0,DLL.FHD_Relation(8221,8222))==0


def test_relation_can_switch_action_for_ranger_pirate_profile():
    DLL.FHD_BridgeNewGame()
    # Character 6 (raider): trade, piracy and combat all matter.
    assert put(ship(id=8331,typ=1,char=6,seed=1006))==1
    for i in range(8): assert record(8331,8332,2,1,101+i)==1
    zero=situation(trading_available=1,piracy_available=1,combat_available=1,
                   relative_enemy_power=50,danger=30,explore_available=1)
    neutral=choose(8331,zero)[1]
    zero.target_id=8332
    linked=choose(8331,zero)[1]
    assert linked.relation_to_target<0
    assert linked.action==2
    assert neutral.action!=linked.action
    assert linked.score>neutral.score


def test_positive_relation_discourages_piracy_without_new_orders():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=8441,typ=3,char=2))==1
    current=situation(piracy_available=1,combat_available=1,relative_enemy_power=50)
    neutral=choose(8441,current)[1]
    assert neutral.action==2
    for i in range(4):assert record(8441,8442,1,10,101+i)==1
    current.target_id=8442
    friendly=choose(8441,current)[1]
    assert friendly.relation_to_target>0
    assert friendly.action!=2 or friendly.score<neutral.score


def test_live_adapter_accepts_only_verification_result_for_target():
    DLL.FHD_BridgeNewGame();DLL.FHDNPC_LiveReset()
    a=live_ship(8551,career=(25,40,35))
    struct.pack_into('<I',a,0x3f4,0x11111111)
    DLL.FHDNPC_TestSetVerifiedTarget(0)
    n=Frame();DLL.FHDNPC_LiveBefore(a,0,C.byref(n));DLL.FHDNPC_LiveAfter(a,0,C.byref(n))
    assert n.valid==1
    baseline_action=n.action
    # Stub simulates ONLY the native classifier's already verified id;
    # it does not attempt to validate x86 game pointers on Linux.
    for i in range(8):assert record(8551,8552,2,1,100+i)==1
    DLL.FHDNPC_TestSetVerifiedTarget(8552)
    f=Frame();DLL.FHDNPC_LiveBefore(a,0,C.byref(f))
    assert f.valid==1
    assert f.action==2
    assert f.action!=baseline_action
    stats=(C.c_uint32*54)();DLL.FHDNPC_LiveStatsGet(stats)
    assert stats[50]>=1 # verified target
    assert stats[51]>=1 # nonzero relation
    assert stats[52]>=1 # changed preferred action
    assert stats[53]>=1 # changed native preference
    DLL.FHDNPC_LiveAfter(a,0,C.byref(f))
    DLL.FHDNPC_TestSetVerifiedTarget(0)


def test_windows_hook_scope_same_without_new_vmt_slots():
    soc=(ROOT/'src/fhd_social_hooks.c').read_text()
    assert 'FHD_ProfileOfIdentity(eid,*seed,0)!=1' in soc
    assert 'candidate(t,kind-1u)' in soc
    assert '#define ENTRY_COUNT 12u' in soc
    assert 'FHDNPC_FearStub2' not in soc
    assert 'FHDNPC_SelectStub2' in soc
    assert 'FHD_EVENT_RESCUE' not in soc
    assert 'FHD_EVENT_TRADE' not in soc
    assert (ROOT/'src/fhd_save_adapter.c').read_bytes()
    assert (ROOT/'src/fhd_npc_sidecar.c').read_bytes()
