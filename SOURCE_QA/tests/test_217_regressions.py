"""User-reported issues: one station message, calm floor, safe salvage, native dialogue."""
from pathlib import Path
import ctypes as C
from test_bridge import DLL
R=Path(__file__).resolve().parents[1]/'src'

DLL.FHDNPC_LootNextGradeMemoryMin.argtypes=[C.c_uint32,C.c_int32,C.c_uint32]
DLL.FHDNPC_LootNextGradeMemoryMin.restype=C.c_int32

def test_civilian_cannot_become_hostile_from_loot_alone():
    for base in (100,90,79,65,59,37,29,20,11,10):
        m=0
        for _ in range(7):
            new_m=DLL.FHDNPC_LootNextGradeMemoryMin(base,m,10)
            assert base+new_m>=10
            assert new_m<=m
            m=new_m
        assert base+m==10

def test_nonpeace_can_reach_hostility():
    for base in (90,60,29):
        m=0
        for _ in range(5):
            m=DLL.FHDNPC_LootNextGradeMemoryMin(base,m,0)
        assert base+m<10

def test_stations_do_not_broadcast_same_incident_twice():
    news=(R/'fhd_news.c').read_text()
    station=news.split('uint32_t FHDNPC_StationBroadcast(',1)[1]
    assert station.count('FHDNPC_InvokeNews(')==1
    assert 'RADIO_KIND,day,msg' not in station
    assert 'INFO_KIND,day,msg' in station
    assert 'Сводка станции:' not in station

def test_extortion_keeps_unsafe_target_guard_but_allows_player_pursuit():
    code=(R/'fhd_grievance.c').read_text().split('static void process_claim(',1)[1].split('void FHDNPC_GrievanceTick(',1)[0]
    assert 'read32(issuer,0x3f4u)!=(uint32_t)(uintptr_t)target' in code
    assert 'FHDNPC_InvokeExtort' in code
    assert 'player(target)' in code

def test_trader_salvage_uses_original_game_selector_with_safety():
    trade=(R/'fhd_native_trade.c').read_text()
    tick=(R/'fhd_vmt_probe.c').read_text()
    assert 'void FHDNPC_TransportSalvage(void *object)' in trade
    assert 'FHD_GetTradeGoal(id,&goal)' in trade
    assert 'FHD_WorldRisk' in trade
    assert 'image+0x2e1860u' in trade
    assert 'if(category==1u)' in tick
    assert 'FHDNPC_TransportSalvage(ship)' in tick
