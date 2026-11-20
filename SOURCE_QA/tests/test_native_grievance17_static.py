import os
"""Exact new-EXE opcode/hook/compatibility audits (NOT Windows game execution)."""
from pathlib import Path
import hashlib
import struct
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('FHD_TARGET_EXE', str(ROOT.parent/'Rangers.exe')))
TARGET_SHA='e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf'


def at(rva,n):
    return EXE.read_bytes()[rva-0xC00:rva-0xC00+n]


def source(filename):
    return (ROOT/'src'/filename).read_text()


@pytest.mark.parametrize('rva,prologue',[
    (0x317848,bytes.fromhex('55 8b ec 83 c4 dc')),  # reputations
    (0x317c89,bytes.fromhex('8b 5f 2c 85 db')),     # unscaled homeplanet branch
    (0x3f4f5c,bytes.fromhex('55 8b ec 51 53')),     # exact pickup
    (0x2d1b2c,bytes.fromhex('55 8b ec 83 c4 c4')),  # item shot
    (0x33fc64,bytes.fromhex('55 8b ec b9 0e 00 00 00')), # killer
])
def test_method_detours_only_verified_complete_instructions(rva,prologue):
    assert hashlib.sha256(EXE.read_bytes()).hexdigest()==TARGET_SHA
    assert at(rva,len(prologue))==prologue


def test_player_reaction_virtual_method_and_original_gameplay_preserved():
    assert at(0x347194,4)==struct.pack('<I',0x716f88)
    assert 'fhd_player_react_orig=b+RANGER_REACT_RVA' in source('fhd_grievance.c')
    assert 'jmp *_fhd_player_react_orig' in source('fhd_grievance_stubs.s')


def test_selfdefense_suppresses_both_scaled_and_unscaled_homeplanet_penalties():
    c=source('fhd_grievance.c'); s=source('fhd_grievance_stubs.s')
    assert 'HOME_RVA 0x317c89u' in c
    assert 'HOME_END_RVA 0x317cd5u' in c
    assert 'fhd_home_end=b+HOME_END_RVA' in c
    assert 'FHDNPC_ShouldSuppressReputation' in s
    assert 'movl $0,40(%esp)' in s and 'movl $0,44(%esp)' in s
    assert 'FHDNPC_IsDefenseVerified' in s
    assert 'jmp *_fhd_home_end' in s
    assert 'jmp *_fhd_reputation_continue' in s
    # The unscaled HomePlanet block genuinely follows the native main loop.
    assert at(0x317c89,7)==bytes.fromhex('8b 5f 2c 85 db 74 45')
    # Original method performs callee-cleanup of double on stack.
    assert at(0x317cd5,9)==bytes.fromhex('5f 5e 5b 8b e5 5d c2 08 00')


def test_no_phantom_theft_or_first_shot_for_loot_and_protected_actors():
    c=source('fhd_grievance.c')
    assert 'list_has(targets,item)' in c and 'collecting(other,item)' in c # queued or flying toward loot
    assert '(!player(ship)&&!ordinary(ship))' in c
    assert 'if(!confirmed_impact&&list_has((uint8_t*)(uintptr_t)read32(star,0x30u)' in c
    assert 'FHD_EVENT_LOOT_TAKEN' in c and 'FHD_EVENT_LOOT_DESTROYED' in c
    assert 'track_grudge(other,actor)' not in c
    assert 'if(f->kind==1u)magnitude=1u' in c
    assert 'FHDNPC_GrievanceOnAttack' in source('fhd_social_hooks.c')
    a=source('fhd_social_hooks.c')
    assert a.index('FHDNPC_GrievanceOnAttack(v,a);')<a.index('if(FHD_ProfileOf(vid,&profile)!=1)')


def test_death_records_cleanup_and_verified_bounties_only_for_attacks():
    c=source('fhd_grievance.c')
    assert 'FHDNPC_OnNativeKill' in source('fhd_grievance_stubs.s')
    assert 'g_offers' in c and 'FHDNPC_GrudgePlan(' in c
    assert 'track_grudge(other,actor)' not in c
    assert 'if(player(a)||ordinary(a))track_grudge(v,a);' in c
    assert 'g_offers[i].paid=1u; /* idempotent before money changes */' in c
    assert 'FHDNPC_GrievanceTick(ship,category);' in source('fhd_vmt_probe.c')
    assert 'FHD_Forget(read32(victim,4u))' in c


def test_abi3_calls_and_snapshot_migration_contract():
    assert '#define FHD_ABI_VERSION 3' in source('fhd_npc_ai.h')
    assert 'FHD_EVENT_LOOT_TAKEN=13, FHD_EVENT_LOOT_DESTROYED=14' in source('fhd_npc_ai.h')


def test_windows_binary_compiled_exports_and_imports():
    dll=ROOT/'build/FHD_NPC_AI.dll'
    assert dll.read_bytes()[:2]==b'MZ'
    info=subprocess.check_output(['llvm-objdump','-p',str(dll)],text=True)
    for name in ('XenoPlugin_Query','XenoPlugin_Initialize','FHDNPC_BountyFor','FHD_Version'):
        assert name in info
    for name in ('KERNEL32','VirtualProtect','FlushInstructionCache'):
        assert name.lower() in info.lower()


def test_runtime_logging_of_effective_actions():
    c=source('fhd_vmt_probe.c')
    for line in ('NPC AI 18 firststrike npc,player,defense,denied,home_branch_skipped=',
                 'NPC AI 18 loot taken,destroyed,protest_intent,compensation_intent,confront_intent='):
        assert line in c


def test_shot_wrapper_matches_original_delphi_callee_cleanup():
    s=source('fhd_grievance_stubs.s')
    block=s.split('_FHDNPC_ShotStub:')[1].split('_fhd_shot_trampoline:')[0]
    assert 'call _fhd_shot_trampoline\n addl $4,%esp' not in block
    assert 'popl %ebp\n ret $4' in block
    # Original native method must use RET 4 (stack bool cleaned by callee).
    assert at(0x2d20d4,9)==bytes.fromhex('5f 5e 5b 8b e5 5d c2 04 00')


def test_missile_mineral_and_item_destruction_call_sites():
    for rva in (0x3f7f0b, 0x3f8295):
        opcode, displacement = struct.unpack('<Bi', at(rva, 5))
        assert opcode == 0xe8
        assert rva + 5 + displacement == 0x3e145c
        assert at(rva - 6, 6) == bytes.fromhex('8b 55 fc 8b 45 f4')
    assert at(0x3f867d,5) == bytes.fromhex('e8 8a db c1 ff')
    assert at(0x3f7fb3,5) == bytes.fromhex('e8 54 e2 c1 ff')


def test_native_relationship_labels_read_the_same_numeric_level():
    assert at(0x2d42a4, 8) == bytes.fromhex('e8 bb f3 01 00 83 e0 7f')
    assert at(0x2d42f2, 5) == bytes.fromhex('e8 ad ff ff ff')


def test_radio_ship_target_uses_native_message_lock_and_target_layout():
    assert at(0x3a5dd4, 10) == bytes.fromhex('a1 74 cc 81 00 e8 9a 38 c8 ff')
    assert at(0x029678, 5) == bytes.fromhex('8b 10 ff 12 c3')
    assert at(0x029680, 6) == bytes.fromhex('8b 10 ff 52 04 c3')
