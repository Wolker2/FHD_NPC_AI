"""1.2 pure verified-event policy & real core snapshot. Not a Windows game test."""
import ctypes as C
from pathlib import Path
from test_bridge import DLL, ship, put

ROOT=Path(__file__).resolve().parents[1]
POL=C.CDLL(str(ROOT/'build/libfhd_social_rules_test.so'))
POL.FHDNPC_PartnershipCredit.argtypes=[C.c_uint32,C.c_int32]
POL.FHDNPC_PartnershipCredit.restype=C.c_int32
POL.FHDNPC_JointCombatCredit.argtypes=[C.c_uint32,C.c_uint32,C.c_uint32]
POL.FHDNPC_JointCombatCredit.restype=C.c_int32
DLL.FHD_Relation.argtypes=[C.c_uint32,C.c_uint32]
DLL.FHD_Relation.restype=C.c_int32
DLL.FHD_Record.argtypes=[C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32]
DLL.FHD_Record.restype=C.c_int32
DLL.FHD_RelationAudit.argtypes=[C.POINTER(C.c_uint32),C.c_uint32]
DLL.FHD_RelationAudit.restype=C.c_int32


def audit():
    arr=(C.c_uint32*6)()
    assert DLL.FHD_RelationAudit(arr,6)==1
    return tuple(arr)


def setup():
    DLL.FHD_BridgeNewGame()
    assert put(ship(id=1101))==1
    assert put(ship(id=1102))==1


def test_contract_does_not_claim_friendship_without_proof():
    assert POL.FHDNPC_PartnershipCredit(0,0)==0
    for rel in [-70,-1,1,25,80]:
        assert POL.FHDNPC_PartnershipCredit(1,rel)==0
    assert POL.FHDNPC_PartnershipCredit(1,0)==1


def test_joint_battle_requires_all_three_facts():
    for observed in range(2):
        for contracted in range(2):
            for managed in range(2):
                expected=1 if observed and contracted and managed else 0
                assert POL.FHDNPC_JointCombatCredit(observed,contracted,managed)==expected


def test_confirmed_contract_creates_positive_sidecar_memory_once():
    setup()
    assert POL.FHDNPC_PartnershipCredit(1,DLL.FHD_Relation(1101,1102))==1
    assert DLL.FHD_Record(1101,1102,11,10,101)==1
    assert DLL.FHD_Relation(1101,1102)==25
    assert POL.FHDNPC_PartnershipCredit(1,DLL.FHD_Relation(1101,1102))==0
    state=audit();assert state[3]==1 and state[4]==0
    sz=DLL.FHD_Snapshot(None,0)
    data=(C.c_uint8*sz)();assert DLL.FHD_Snapshot(data,sz)==sz
    DLL.FHD_BridgeNewGame()
    assert DLL.FHD_Restore(data,sz)==2
    assert audit()==state
    assert DLL.FHD_Relation(1101,1102)==25
    assert POL.FHDNPC_PartnershipCredit(1,DLL.FHD_Relation(1101,1102))==0


def test_confirmed_shared_combat_improves_both_captains():
    setup()
    assert DLL.FHD_Record(1101,1102,11,10,102)==1
    assert DLL.FHD_Record(1101,1102,1,10,103)==1
    assert DLL.FHD_Record(1102,1101,1,10,103)==1
    assert DLL.FHD_Relation(1101,1102)==70
    assert DLL.FHD_Relation(1102,1101)==45
    assert audit()[3]==2


def test_existing_grudge_not_erased_by_unverified_contract():
    setup()
    assert DLL.FHD_Record(1101,1102,2,1,100)==1
    assert DLL.FHD_Relation(1101,1102)<0
    assert POL.FHDNPC_PartnershipCredit(1,DLL.FHD_Relation(1101,1102))==0


def test_host_protects_nonparticipants_and_save_adapter():
    hooks=(ROOT/'src/fhd_social_hooks.c').read_text()
    live=(ROOT/'src/fhd_live_policy.c').read_text()
    assert 'known_partner_kind(partner)' in hooks
    assert 'read32(a,0x400u)>0u' in hooks
    assert 'partner_attacked_victim(vid,pid,vs,ps,day)' in hooks
    assert 'FHD_ProfileOf(pid,&bp)==1' in hooks
    assert 'FHDNPC_ObservePartnership(p,c);' in live
    assert '(category==0u||category==2u)' in hooks
    assert 'FHD_EVENT_RESCUE' not in hooks
    assert 'FHDNPC_PartnershipCredit(' not in hooks
    assert 'if(old_enemy){g_existing_target++;' in hooks
    assert 'dom=etype==0u && eowner==5u' in hooks
    assert (ROOT/'src/fhd_save_adapter.c').read_bytes()
