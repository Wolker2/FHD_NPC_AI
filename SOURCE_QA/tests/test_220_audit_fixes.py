"""Comprehensive verification of the 2.2.0..2.2.2 technical fixes on top of 2.1.9."""
from pathlib import Path
import hashlib
import os
import struct
import ctypes as C
from test_bridge import DLL

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get('FHD_TARGET_EXE', str(ROOT.parent / 'Rangers.exe')))


def test_rangers_exe_get_relation_to_ship_prologue_hook():
    data = EXE.read_bytes()
    assert hashlib.sha256(data).hexdigest() == 'e43a4b15503c705eebe2d8ce5778c2a2fb090db515a3fd86d319e5c5aa692ebf'
    # RVA 0x2f3664 (TShip.GetRelationToShip) must have exact 6-byte prologue 53 56 57 55 8b fa
    assert data[0x2f3664 - 0xc00 : 0x2f3664 - 0xc00 + 6] == bytes.fromhex('53 56 57 55 8b fa')
    grievance = (ROOT / 'src/fhd_grievance.c').read_text()
    stubs = (ROOT / 'src/fhd_grievance_stubs.s').read_text()
    assert 'RELATION_VALUE_RVA' in grievance
    assert 'FHDNPC_RelationValueStub' in grievance
    assert 'FHDNPC_RelationRawTrampoline' in grievance
    assert '_FHDNPC_RelationValueStub:' in stubs
    assert '_FHDNPC_RelationRawTrampoline:' in stubs


def test_player_loot_pickup_accounts_for_tractor_beam_drag():
    grievance = (ROOT / 'src/fhd_grievance.c').read_text()
    collecting = grievance.split('static int collecting(', 1)[1].split('uint32_t FHDNPC_RelationLevelValue(', 1)[0]
    assert 'read32(ship,0x110u)!=0u' in collecting
    assert '550.0f*550.0f' in collecting
    assert '420.0f*420.0f' in collecting
    assert 'should_demand_compensation(' in grievance


def test_restitution_claim_bypasses_global_pirate_cooldown_and_steers_within_contact_range():
    grievance = (ROOT / 'src/fhd_grievance.c').read_text()
    claim = grievance.split('static void process_claim(', 1)[1].split('void FHDNPC_GrievanceTick(', 1)[0]
    assert '!g_claims[i].amount && read32((void*)g_base,EXTORT_COOLDOWN_RVA)<7u' in claim
    assert 'ORDER_MOVE_RVA' in claim
    assert 'FHDNPC_InvokeMoneyDemand((void*)(g_base+MONEY_RESPONSE_RVA)' in claim


def test_transport_salvage_enables_can_collect_vmt_and_allows_uncommitted_in_space_transports():
    data = EXE.read_bytes()
    # Verify TTransport VMT+0x94 points to 0x6e1c8c (xor eax, eax; ret) and TRanger VMT+0x94 points to 0x71e334 (mov al, 1; ret)
    assert struct.unpack_from('<I', data, 0x1ab58c + 0x94 - 0xc00)[0] == 0x006e1c8c
    assert data[0x2e1c8c - 0xc00 : 0x2e1c8c - 0xc00 + 3] == bytes.fromhex('33 c0 c3')
    assert struct.unpack_from('<I', data, 0x310c70 + 0x94 - 0xc00)[0] == 0x0071e334
    assert data[0x31e334 - 0xc00 : 0x31e334 - 0xc00 + 3] == bytes.fromhex('b0 01 c3')
    trade = (ROOT / 'src/fhd_native_trade.c').read_text()
    salvage = trade.split('void FHDNPC_TransportSalvage(', 1)[1].split('static void REG market(', 1)[0]
    assert 'ship[0x428]>2u' in salvage
    assert 'TRANSPORT_VMT+0x94u' in salvage
    assert '0x31e334u' in salvage


def test_station_broadcast_works_in_player_and_local_modes_and_aggregates_raids():
    news = (ROOT / 'src/fhd_news.c').read_text()
    station_broadcast = news.split('uint32_t FHDNPC_StationBroadcast(', 1)[1]
    assert 'FHDNPC_DialogueModeGet()==FHD_DLG_VANILLA' in station_broadcast
    assert 'FHDNPC_DialogueModeGet()!=FHD_DLG_LOCAL' not in station_broadcast
    alert = (ROOT / 'src/fhd_station_alert.c').read_text()
    assert 'recent_system_attacks' in alert


def test_positive_social_memory_links_from_combat_assistance_and_partnerships():
    hooks = (ROOT / 'src/fhd_social_hooks.c').read_text()
    assert 'FHD_RecordPeer(id,pid,pseed,FHD_EVENT_DIPLOMACY,4u,day)' in hooks
    assert 'FHD_RecordPeer(ally_id,aid,as,FHD_EVENT_HELP,3u,day)' in hooks


def test_realistic_kill_and_attack_reputation_only_homeplanet_and_bonded_allies():
    data = EXE.read_bytes()
    # Verify TPlanet.ChangeRelationToRanger at RVA 0x339430 has prologue 53 56 57 55 83 c4 f8
    assert data[0x339430 - 0xc00 : 0x339430 - 0xc00 + 7] == bytes.fromhex('53 56 57 55 83 c4 f8')
    grievance = (ROOT / 'src/fhd_grievance.c').read_text()
    stubs = (ROOT / 'src/fhd_grievance_stubs.s').read_text()
    assert 'PLANET_CHANGE_REL_RVA 0x339430u' in grievance
    assert 'FHDNPC_InvokePlanetRelation(home,(void*)k,-100);' in grievance
    assert 'bonded_witness(const uint8_t *ally,const uint8_t *v)' in grievance
    assert 'incident_reported_to_home(const uint8_t *p,const uint8_t *v)' in grievance
    assert 'apply_role_planet_consequences(const uint8_t *k,const uint8_t *v,int is_kill)' in grievance
    assert 'notify_bonded_witnesses(v,k,3u,1);' in grievance
    assert '_FHDNPC_InvokePlanetRelation:' in stubs


def test_custom_mod_ships_react_to_loot_interference():
    grievance = (ROOT / 'src/fhd_grievance.c').read_text()
    vmt = (ROOT / 'src/fhd_vmt_probe.c').read_text()
    assert 'static int loot_actor(const uint8_t*p)' in grievance
    assert 'read32(ship,0xc4u)' in grievance
    assert 'if(g_armed&&kind<=3u&&loot_actor(p)&&!ordinary(p))' in grievance
    assert 'if(ship&&category<4u){extern void FHDNPC_GrievanceTick(void*,uint32_t);' in vmt


def test_v225_selective_compensation_and_forminfo_personality():
    data = EXE.read_bytes()
    assert data[0x101a6a - 0xc00 : 0x101a6a - 0xc00 + 9] == bytes.fromhex('8b 45 f4 8b 15 98 12 5b 00')
    grievance = (ROOT / 'src/fhd_grievance.c').read_text()
    stubs = (ROOT / 'src/fhd_grievance_stubs.s').read_text()
    assert 'FORM_INFO_RVA 0x101a6au' in grievance
    assert 'FHDNPC_AppendShipTraitInfo' in grievance
    assert '_FHDNPC_FormInfoStub:' in stubs
    assert 'g_claims[i].pre_rel>=30u' in grievance
