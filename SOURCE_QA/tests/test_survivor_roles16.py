import os
"""1.6 pure native diplomacy/fear/extortion policies and compatibility tests.
These are Linux fixtures, NOT proof of Windows gameplay behavior.
"""
from pathlib import Path
import ctypes as C
import struct
import pytest
from test_core_v03 import LIB, Profile, put, profile, snapshot, restore
from test_live_policy_07 import DLL, Frame, ship, Profile as NativeProfile

ROOT=Path(__file__).resolve().parents[1]
POL=C.CDLL(str(ROOT/'build/libfhd_social_rules_test.so'))
POL.FHDNPC_MemoryFear.argtypes=[C.c_uint32,C.POINTER(Profile),C.c_int32,C.c_uint32,C.c_uint32,C.c_uint32]
POL.FHDNPC_MemoryFear.restype=C.c_uint32
POL.FHDNPC_TrustKnownAggressor.argtypes=[C.c_uint32,C.c_int32,C.c_uint32]
POL.FHDNPC_TrustKnownAggressor.restype=C.c_uint32
POL.FHDNPC_AttackMemoryWeight.argtypes=[C.c_uint32]
POL.FHDNPC_AttackMemoryWeight.restype=C.c_uint32
POL.FHDNPC_RegisterVerifiedProfession.argtypes=[C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
POL.FHDNPC_RegisterVerifiedProfession.restype=C.c_int32
POL.FHDNPC_LookupVerifiedProfession.argtypes=[C.c_uint32,C.c_uint32,C.c_uint32]
POL.FHDNPC_LookupVerifiedProfession.restype=C.c_uint32
POL.FHDNPC_ResetVerifiedProfessions.argtypes=[]


def traits(caution=60):
    p=Profile(); p.traits[3]=caution; return p


@pytest.mark.parametrize('profession,relative_strength,expected',[
  (2,70,0),(2,86,1),(2,110,1), # merchant
  (3,65,0),(3,70,1), # liner
  (4,75,0),(4,80,1), # diplomat
  (10,95,1),(11,95,1),(12,75,1),(13,160,1),(14,75,1),
  (5,200,0),(0,200,0), # pirates and unknown aren't transports
])
def test_aware_survivor_careers(profession,relative_strength,expected):
    p=traits()
    assert POL.FHDNPC_MemoryFear(profession,C.byref(p),-14,100,relative_strength,100)==expected


@pytest.mark.parametrize('relation',[-100,-50,-7])
def test_known_attacker_enables_survival_memory(relation):
    p=traits()
    assert POL.FHDNPC_MemoryFear(2,C.byref(p),relation,100,130,100)==1
    assert POL.FHDNPC_TrustKnownAggressor(1,relation,2)==0


@pytest.mark.parametrize('relation',[-6,-1,0,15,100])
def test_no_fear_or_trust_veto_without_verified_hostility(relation):
    p=traits()
    assert POL.FHDNPC_MemoryFear(3,C.byref(p),relation,100,200,100)==0
    assert POL.FHDNPC_TrustKnownAggressor(1,relation,3)==1


def test_native_selfdefense_fear_preserved_and_strength_unknown_not_invented():
    p=traits(100)
    assert POL.FHDNPC_MemoryFear(2,C.byref(p),-50,0,100,100)==0
    assert POL.FHDNPC_MemoryFear(2,C.byref(p),-50,100,0,100)==0
    assert POL.FHDNPC_MemoryFear(2,C.byref(p),-50,100,200,101)==0
    assert POL.FHDNPC_MemoryFear(2,C.byref(p),-50,100,70,100)==0
    assert POL.FHDNPC_TrustKnownAggressor(0,0,2)==0


def test_attacked_liner_diplomat_remember_longer_than_merchant():
    assert POL.FHDNPC_AttackMemoryWeight(2)==1
    assert POL.FHDNPC_AttackMemoryWeight(3)==2
    assert POL.FHDNPC_AttackMemoryWeight(4)==2


def test_native_extortion_record_survives_abi3_snapshot():
    LIB.FHD_Reset()
    assert put(764200, 222, role=2,native=2)==1
    LIB.FHD_Record.argtypes=[C.c_uint32]*5
    LIB.FHD_Record.restype=C.c_int32
    assert LIB.FHD_Record(764200,764201,12,2,140)==1
    assert LIB.FHD_Relation(764200,764201)==-12
    original=snapshot()
    LIB.FHD_Reset()
    assert restore(original)==1
    assert LIB.FHD_Relation(764200,764201)==-12
    assert profile(764201) is None  # player isn't managed by the NPC AI


@pytest.mark.parametrize('type,profession,expected',[
    (1,10,1),(2,11,1),(3,12,1),(4,13,1),(2,14,1),
    (0,14,0),(5,14,0),(12,14,0),(13,14,0),
    (1,1,0),(2,0,0),(4,15,0),
])
def test_explicit_mod_roles_require_verified_native_class(type,profession,expected):
    POL.FHDNPC_ResetVerifiedProfessions()
    status=POL.FHDNPC_RegisterVerifiedProfession(100112,543,type,profession)
    assert status==expected
    assert POL.FHDNPC_LookupVerifiedProfession(100112,543,type)==(profession if expected else 0)
    assert POL.FHDNPC_LookupVerifiedProfession(100112,544,type)==0


def test_explicit_mod_role_applied_to_real_profile_without_game_commands():
    DLL.FHD_BridgeNewGame();DLL.FHDNPC_LiveReset()
    DLL.FHDNPC_RegisterVerifiedProfession.argtypes=[C.c_uint32]*4
    DLL.FHDNPC_RegisterVerifiedProfession.restype=C.c_int32
    assert DLL.FHDNPC_RegisterVerifiedProfession(750020,123456+750020,2,14)==1
    a=ship(750020,which=1)
    original=bytes(a)
    f=Frame()
    DLL.FHDNPC_LiveBefore(a,1,C.byref(f))
    assert f.valid==1
    x=NativeProfile()
    assert DLL.FHD_ProfileOf(750020,C.byref(x))==1
    assert x.role==14   # spy, explicitly tagged by a verified mod
    DLL.FHDNPC_LiveAfter(a,1,C.byref(f))
    assert bytes(a)==original
    # A new campaign/reset clears tags so identity reuse cannot inherit roles.
    DLL.FHDNPC_LiveReset()
    DLL.FHDNPC_LookupVerifiedProfession.argtypes=[C.c_uint32]*3
    DLL.FHDNPC_LookupVerifiedProfession.restype=C.c_uint32
    assert DLL.FHDNPC_LookupVerifiedProfession(750020,123456+750020,2)==0


def test_1_6_transport_vmt_proven_by_actual_exe_bytes():
    game=(Path(os.environ.get('FHD_TARGET_EXE', str(ROOT.parent/'Rangers.exe')))).read_bytes()
    for slot_va,method_va in [
        (0x5AB610,0x5AD6F0), # RecomputeFearState
        (0x5AB618,0x5ADD20), # TrustsAttackRequester
        (0x5AB630,0x5AE678), # ReactToExtortionDemand
    ]:
        slot_rva=slot_va-0x400000
        method_rva=method_va-0x400000
        assert struct.unpack_from('<I',game,slot_rva-0xC00)[0]==method_va
        assert game[method_rva-0xC00] in (0x53,0x55,0x56,0x8B)


def test_transport_fear_trust_and_extortion_hooks():
    code=(ROOT/'src/fhd_social_hooks.c').read_text()
    assert '#define ENTRY_COUNT 12u' in code
    assert 'slot_rva[9]=base_vmt_next[1]+0x68u;orig_rva[9]=0x1ad6f0u' in code
    assert 'slot_rva[10]=base_vmt_next[1]+0x70u;orig_rva[10]=0x1add20u' in code
    assert 'slot_rva[11]=base_vmt_next[1]+0x88u;orig_rva[11]=0x1ae678u' in code
    assert 'p[0x3b4u]=1u;' in code  # use vanilla's existing escape branch
    assert '*(volatile uint32_t*)(target+' not in code
    assert '*(volatile uint32_t*)(r+' not in code

@pytest.mark.parametrize('role,action,lower_bound',[
    (12,9,28),(12,8,18),       # agent missions/diplomacy
    (13,9,35),(13,6,20),      # special agent missions/ally support
    (14,7,25),(14,8,24),      # spy reconnaissance/diplomacy
    (6,1,26),(6,2,16),        # smuggler trade/optional piracy
    (7,3,22),(7,9,30),        # mercenary combat/mission
])
def test_verified_role_scores_grounded_actions(role,action,lower_bound):
    POL.FHDNPC_RoleDecisionBonus.argtypes=[C.POINTER(Profile),C.c_uint32]
    POL.FHDNPC_RoleDecisionBonus.restype=C.c_int32
    p=traits();p.role=role
    assert POL.FHDNPC_RoleDecisionBonus(C.byref(p),action)>=lower_bound
    p.role=2  # existing vanilla trader actions not modified by tagging policy
    assert POL.FHDNPC_RoleDecisionBonus(C.byref(p),action)==0


def test_verified_spy_role_can_change_available_diplomatic_decision():
    from test_core_v03 import situation, choose
    LIB.FHD_Reset()
    # Same seed, identity and character; only verified role differs.
    assert put(90505,0,role=2,native=2,seed=8304)==1
    sit=situation(trading_available=1,explore_available=1,
                  diplomacy_available=1,mission_available=1)
    _,normal=choose(90505,sit)
    assert put(90505,0,role=14,native=2,seed=8304)==1
    _,spy=choose(90505,sit)
    assert normal.action==9 and spy.action==8
    assert spy.score>normal.score


def test_survivor_policy_respects_native_truce_and_docked_attacker():
    source=(ROOT/'src/fhd_social_hooks.c').read_text()
    assert 'read32(p,0x3f8u)==(uint32_t)(uintptr_t)target' in source
    assert 'read32(target,0x1cu) || read32(target,0x20u)' in source
    assert 'if(!star || read32(target,0x24u)!=star)return original;' in source
