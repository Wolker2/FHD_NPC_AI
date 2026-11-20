"""Focused regression coverage for official station-only broadcasts."""
from pathlib import Path
from test_bridge import DLL
from test_loot_radio214 import grade
import ctypes as C
ROOT=Path(__file__).resolve().parents[1]
G=(ROOT/'src/fhd_grievance.c').read_text()
N=(ROOT/'src/fhd_news.c').read_text()
S=(ROOT/'src/fhd_station_alert.c').read_text()
A=(ROOT/'src/fhd_social_hooks.c').read_text()
D=(ROOT/'src/fhd_native_duties.c').read_text()

def test_hostile_relation_is_reachable_after_four_verified_events():
    for native in [25,59,79,95]:
        m=0
        for _ in range(5):
            m=DLL.FHDNPC_LootNextGradeMemory(native,m)
        assert grade(max(0,min(100,native+m)))==0

def test_radio_from_stations_only():
    assert 'if(speaker[0x10u]<6u && !personal)return;' in N
    assert 'station_sender(FHD_DialogueSender(),star)' in N
    assert 'FHDNPC_StationBroadcast' in N
    assert 'mode==FHD_DLG_VANILLA' in N
    assert 'FHDNPC_LocalDialogue(FHD_DLG_WARNING,p,id(star)' not in D

def test_station_alerts_rely_on_verified_attacks_only():
    assert 'g_pairs[slot].vseed=vs;' in A
    assert A.index('FHDNPC_StationObservePirateAttack')>A.index('g_pairs[slot].vseed=vs;')
    assert '3u+((u32(p,4u)^u32(s,4u))&1u)' in S
    assert 'FHDNPC_StationBroadcast(s,offender,day,e->dispatched)' in S
    assert 'ptr(w,0x3f4u)==attacker' in S

def test_native_compensation_does_not_use_guessed_screen_phase():
    claim=G.split('static void process_claim(',1)[1].split('void FHDNPC_GrievanceTick(',1)[0]
    assert 'CURRENT_SCREEN_RVA)!=16u' not in claim
    assert 'phase!=1u&&phase!=3u' not in claim
    assert 'EXTORT_COOLDOWN_RVA)<7u' in claim
    assert 'FHDNPC_InvokeExtort' in claim

def test_pirate_hostility_target_is_real_player_not_other_npc():
    tick=G.split('void FHDNPC_GrievanceTick(',1)[1].split('static void pay_bounty(',1)[0]
    assert 'armed_resentment && !read32(p,0x3f4u)' in tick
    assert 'player(target) && same_star(p,target)' in tick
    assert '(int32_t)native+grudge<10' in tick
    assert 'FHDNPC_InvokeJoint(p,p,target)' in tick

def test_system_presence_is_diagnostic_not_new_ship_creation():
    assert 'FHDNPC_PopulationStats' in S
    assert 'FHDNPC_StationReset' in S
    assert 'CreateShip' not in S and 'BuyWarrior' not in S
