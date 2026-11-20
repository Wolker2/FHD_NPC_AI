"""Narrow checks for new diagnostics and conditional claim retirement."""
from pathlib import Path
R=Path(__file__).resolve().parents[1]/'src'
def test_failed_native_demand_is_retained_and_rate_limited():
    t=(R/'fhd_grievance.c').read_text()
    c=t.split('static void process_claim(',1)[1].split('void FHDNPC_GrievanceTick(',1)[0]
    assert 'if(g_claims[i].last_attempt==day)return;' in c
    # 2.1.9 switches this case to the engine's direct money dialogue.
    # A player's refusal is a finished conversation; no duplicate demand.
    assert 'FHDNPC_InvokeMoneyDemand' in c
    assert 'g_claims[i].active=0;' in c
    assert 'g_claims[i].amount' in c
    assert 'if(!player(target)||!same_star(issuer,target))' in c
    assert 'CAN_CONTACT_RVA' in c

def test_trade_gating_unchanged_but_diagnostic_visible():
    t=(R/'fhd_native_trade.c').read_text()
    l=(R/'fhd_vmt_probe.c').read_text()
    assert 'salvage_reasons[2]++' in t  # missing real cargo hook
    assert 'FHD_GetTradeGoal(id,&goal)' in t  # preserve trade commitments
    assert 'FHD_WorldRisk' in t
    assert 'TryCollect)(image+0x2e1860u)' in t
    assert 'NPC AI 218 salvage eligible,no_space,no_hook,busy,unsafe,route,profile,throttled,attempts=' in l
    assert 'NPC AI 218 extortion attempted,accepted,dialogue_counter,retained=' in l

def test_station_counters_audit_unreported_threshold():
    s=(R/'fhd_station_alert.c').read_text()
    v=(R/'fhd_vmt_probe.c').read_text()
    assert 'void FHDNPC_StationDiagnostics(uint32_t*out)' in s
    assert 'e->seen>=e->need' in s
    assert 'NPC AI 218 station pairs,below_threshold,ready,reported=' in v
