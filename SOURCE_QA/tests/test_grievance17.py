"""v1.7 motive/reputation/bounty pure policies + ABI3, not Windows gameplay tests."""
from pathlib import Path
import ctypes as C
import pytest
from test_core_v03 import LIB, Profile, put, snapshot, restore

ROOT=Path(__file__).resolve().parents[1]
POL=C.CDLL(str(ROOT/'build/libfhd_social_rules_test.so'))
POL.FHDNPC_LootReaction.argtypes=[C.POINTER(Profile),C.c_uint32,C.c_uint32,C.c_uint32]
POL.FHDNPC_LootReaction.restype=C.c_int32
POL.FHDNPC_GrudgePlan.argtypes=[C.POINTER(Profile),C.c_int32,C.c_uint32,C.c_uint32]
POL.FHDNPC_GrudgePlan.restype=C.c_int32
POL.FHDNPC_JustifiedDefense.argtypes=[C.c_uint32]*4
POL.FHDNPC_JustifiedDefense.restype=C.c_int32


def person(role=3,char=8,greed=70,vengeance=70,aggression=70,cunning=70):
    p=Profile();p.role=role;p.character=char
    p.traits[1]=aggression;p.traits[2]=greed;p.traits[5]=cunning;p.traits[6]=vengeance
    return p


@pytest.mark.parametrize('role,expected',[  # same character and temperament, different professions
 (2,1),   # trader protests
 (11,0),  # liner does not confront
 (5,1),   # diplomat protests
 (4,1),   # military is confrontational but doesn't invent a ransom UI
 (3,2),   # pirate demands compensation from the player
 (6,2),   # smuggler seeks compensation (NPC memory; no invented UI)
 (7,1),   # mercenary disputes loot
 (12,1),  # agent protests
 (13,1),  # special agent protests
 (14,1),  # spy avoids open battle but remembers the incident
])
def test_classes_have_profession_specific_reactions(role,expected):
    p=person(role=role,greed=42,vengeance=48,aggression=40,cunning=54)
    assert POL.FHDNPC_LootReaction(C.byref(p),0,1,0)==expected


def test_pirate_can_escalate_when_armed_cunning_and_outmatches_target():
    p=person(role=3,greed=90,vengeance=90,aggression=90,cunning=95)
    p.traits[0]=95  # bravery: only a bold pirate escalates to immediate attack
    assert POL.FHDNPC_LootReaction(C.byref(p),0,1,0)==3
    assert POL.FHDNPC_LootReaction(C.byref(p),0,1,1)==2
    assert POL.FHDNPC_LootReaction(C.byref(p),1,1,0)==3


def test_item_destroyed_is_not_demanded_back_and_pirate_will_seek_compensation():
    p=person(role=3,greed=55,vengeance=55,aggression=55,cunning=30)
    assert POL.FHDNPC_LootReaction(C.byref(p),1,1,0)==2


@pytest.mark.parametrize('first,star,age,truce,expected',[
 (1,1,0,0,1),(1,1,15,0,1),
 (2,1,0,0,0),(0,1,0,0,0),
 (1,0,0,0,0),(1,1,16,0,0),(1,1,0,1,0),
])
def test_self_defense_only_for_first_npc_strike(first,star,age,truce,expected):
    assert POL.FHDNPC_JustifiedDefense(first,star,age,truce)==expected


@pytest.mark.parametrize('relation,budget,strength,expected',[ 
 (-65,10000,800,3),  # bounty
 (-50,2100,300,2),   # mercenary contract if a verified executor is available
 (-20,30000,1000,0), # not an insult deserving contract
 (-70,450,200,0),   # can't pay
 (-70,30000,0,0),   # unknown relative threat
 (-70,30000,800000,1), # vastly stronger target: no paid contract
])
def test_revenge_requires_hostility_funds_and_grounded_threat(relation,budget,strength,expected):
    p=person(greed=95,vengeance=100,aggression=95,cunning=90)
    assert POL.FHDNPC_GrudgePlan(C.byref(p),relation,budget,strength)==expected


def test_nonvengeful_character_does_not_fund_murder():
    p=person(greed=1,vengeance=3,aggression=4,cunning=5)
    assert POL.FHDNPC_GrudgePlan(C.byref(p),-100,100000,50)==0


@pytest.mark.parametrize('ev,expected_max',[ (13,-3),(14,-6)])
def test_confirmed_loot_event_is_persisted_without_new_sav_format(ev,expected_max):
    LIB.FHD_Reset()
    assert put(906201,2002,role=2,native=2)==1
    assert LIB.FHD_Record(906201,906202,ev,1,250)==1
    before=LIB.FHD_Relation(906201,906202)
    assert before<=expected_max
    archive=snapshot()
    assert archive[:4]==b'FHDN'
    LIB.FHD_Reset()
    assert restore(archive)==1
    assert LIB.FHD_Relation(906201,906202)==before
