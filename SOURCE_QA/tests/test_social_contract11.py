"""Static safety contract for extra hooks and save audit."""
from pathlib import Path
R=Path(__file__).resolve().parents[1]
SOC=(R/'src/fhd_social_hooks.c').read_text()
HOST=(R/'src/fhd_xeno_probe.c').read_text()
VMT=(R/'src/fhd_vmt_probe.c').read_text()
CORE=(R/'src/fhd_npc_ai.c').read_text()

def test_exclusions_still_present():
 assert 'type==12u' in SOC and 'read32(p,0x0cu)' in SOC
 assert 'dom=etype==0u && eowner==5u' in SOC
 assert 'FHD_ProfileOf(id,&prof)!=1' in SOC

def test_selfdefense_and_flagships_still_native():
 assert 'g_self_defense++' in SOC
 assert 'p[0x510u]==1u' in SOC
 assert 'g_override_fear[kind]++' in SOC
 assert 'g_override_flagship[kind]++' in SOC

def test_true_partner_betrayal_only():
 assert 'read32(v,0x3fcu)==(uint32_t)(uintptr_t)a' in SOC
 assert 'FHD_EVENT_BETRAY:FHD_EVENT_ATTACK' in SOC

def test_detailed_reasons_exported():
 for x in ['g_existing_target','g_flagship_protected','g_order_protected','g_profile_missing','g_friend_avoided','g_risk_avoided','g_strength_unknown']:
  assert x in SOC
 assert 'NPC AI 18 selection' in VMT and 'NPC AI 18 policy' in VMT

def test_audit_after_restore_and_presave():
 assert 'FHDNPC_LogMemoryAudit("SAVE")' in VMT
 assert 'FHDNPC_LogMemoryAudit("RESTORED")' in HOST
 assert 'int32_t FHD_CALL FHD_RelationAudit' in CORE

def test_unchanged_sav_sidecar():
 assert 'FHDNPC_StageSaveSnapshot' in VMT
 assert 'FHDNPC_RestoreLoadedSnapshot' in HOST
