import os
"""FHD_NPC_AI 1.5.0 pure rule and exact native ABI static regression.
Not a substitute for Windows gameplay verification.
"""
from pathlib import Path
import ctypes as C
import struct
import pytest
from test_core_v03 import Profile, LIB, put, profile, snapshot, restore

ROOT=Path(__file__).resolve().parents[1]
POL=C.CDLL(str(ROOT/'build/libfhd_social_rules_test.so'))
POL.FHDNPC_Profession.argtypes=[C.c_uint32,C.c_uint32]
POL.FHDNPC_Profession.restype=C.c_uint32
POL.FHDNPC_RoleFromProfession.argtypes=[C.c_uint32]
POL.FHDNPC_RoleFromProfession.restype=C.c_uint32
POL.FHDNPC_PirateMotive.argtypes=[C.POINTER(Profile),C.c_int32,C.c_uint32,C.c_int32,C.c_uint32,C.c_uint32,C.c_uint32]
POL.FHDNPC_PirateMotive.restype=C.c_uint32
POL.FHDNPC_InterruptHop.argtypes=[C.c_uint32]*5
POL.FHDNPC_InterruptHop.restype=C.c_uint32

@pytest.mark.parametrize('native,sub,profession,role',[
  (1,0,1,1),(2,0,2,2),(2,1,3,11),(2,2,4,5),
  (3,0,5,3),(3,1,6,3),(3,4,6,3),
  (4,0,7,4),(4,1,8,4),(6,0,9,9),(11,0,9,9),
  (5,0,0,0),(0,0,0,0),(12,0,0,0),(13,0,0,0),
  (2,5,0,0),(4,8,0,0),
])
def test_native_profession_is_known_and_conservative(native,sub,profession,role):
    assert POL.FHDNPC_Profession(native,sub)==profession
    assert POL.FHDNPC_RoleFromProfession(profession)==role

def mkprofil(bravery=40,aggression=40,greed=50,cunning=80):
    p=Profile();p.traits[0]=bravery;p.traits[1]=aggression
    p.traits[2]=greed;p.traits[5]=cunning
    return p

@pytest.mark.parametrize('money,cargo,result',[(0,0,0),(0,1,2),(300,0,2),(500,1,2),(-3,0,0)])
def test_piracy_demands_require_a_real_lucrative_target(money,cargo,result):
    p=mkprofil()
    assert POL.FHDNPC_PirateMotive(C.byref(p),0,0,money,cargo,0,1)==result

def test_pirate_nonprofit_motives_and_friendship():
    p=mkprofil()
    motive=lambda rel,sub,ret: POL.FHDNPC_PirateMotive(C.byref(p),rel,sub,0,0,ret,0)
    assert motive(0,0,0)==0
    assert motive(-30,0,0)==1
    assert motive(0,0,1)==1
    assert motive(0,2,0)==3
    assert motive(50,0,0)==4
    assert motive(50,0,1)==1  # an actual attack is retaliation

def test_unarmed_or_hopless_pirate_never_forced_to_stand():
    f=POL.FHDNPC_InterruptHop
    assert f(2,1,1,100,100)==0
    assert f(3,0,1,100,100)==0
    assert f(3,1,0,100,100)==0
    assert f(3,1,1,0,100)==0
    assert f(3,1,1,100,300)==0
    assert f(3,1,1,80,100)==1

def test_exact_vmt_pointers_from_supplied_2026_exe():
    p=(Path(os.environ.get('FHD_TARGET_EXE', str(ROOT.parent/'Rangers.exe')))).read_bytes()
    # Mapped .text RVA -> file offset is verified for this supplied PE.
    for rva,target_va in [(0x347134,0x71264c),(0x19931c,0x59d980),(0x199334,0x59c554)]:
       assert struct.unpack_from('<I',p,rva-0xc00)[0]==target_va

def test_player_is_contact_not_managed_and_native_fields_unchanged():
    source=(ROOT/'src/fhd_social_hooks.c').read_text()
    assert 'native_player(t)' in source
    assert 'if(!dom && !native_player(enemy)' in source
    assert 'PLAYER_VMT_NEXT_RVA 0x347134u' in source
    assert 'g_player_attacks++' in source
    assert 'g_player_rel_targets++' in source
    assert 'FHDNPC_PostPirateFear' not in source
    assert 'FHDNPC_PirateMotive' in source
    assert 'CargoGoods' in source
    # No direct mutation of player's native state in our social code.
    assert '*(volatile uint32_t*)(target+' not in source
    assert '*(volatile uint32_t*)(t+' not in source

def test_player_can_be_a_persisted_non_agent_contact():
    LIB.FHD_Reset()
    assert put(81201,9,role=3,native=3)==1
    assert LIB.FHD_Record(81201,81202,2,1,100)==1 # attacking player 81202
    assert LIB.FHD_Relation(81201,81202)<0
    initial=snapshot()
    LIB.FHD_Reset()
    assert restore(initial)==1
    assert LIB.FHD_Relation(81201,81202)<0
    assert profile(81202) is None

def test_pirate_vmt_keeps_native_escape():
    src=(ROOT/'src/fhd_social_hooks.c').read_text()
    assert 'slot_rva[8]=base_vmt_next[2]+0x50u' in src
    assert 'base_vmt_next[2]+0x68u' not in src
    assert 'candidate(p,2u)' in src


def test_cunning_pirate_avoids_overpowering_wealthy_target():
    POL.FHDNPC_AvoidStrongPrey.argtypes=[C.POINTER(Profile),C.c_uint32,C.c_uint32,C.c_uint32]
    POL.FHDNPC_AvoidStrongPrey.restype=C.c_uint32
    p=mkprofil(cunning=90,aggression=35)
    assert POL.FHDNPC_AvoidStrongPrey(C.byref(p),2,80,180)==1
    assert POL.FHDNPC_AvoidStrongPrey(C.byref(p),2,80,120)==0
    assert POL.FHDNPC_AvoidStrongPrey(C.byref(p),1,80,180)==0  # revenge, not robbery
    p.traits[1]=90  # reckless pirate accepts the fight
    assert POL.FHDNPC_AvoidStrongPrey(C.byref(p),2,80,180)==0

@pytest.mark.parametrize('prof,role',[(10,12),(11,13),(12,6),(13,7),(14,14)])
def test_verified_mod_prototype_role_registry_not_vanilla_assumption(prof,role):
    assert POL.FHDNPC_RoleFromProfession(prof)==role
    # No made-up vanilla native subtype maps to these roles.
    for ship_type in range(14):
        for sub in range(3):
            assert POL.FHDNPC_Profession(ship_type,sub)!=prof
