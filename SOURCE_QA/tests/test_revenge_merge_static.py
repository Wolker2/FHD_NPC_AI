from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def source(name):return (ROOT/'src'/name).read_text()

def test_attack_only_revenge_and_no_loot_bounty():
    c=source('fhd_grievance.c')
    assert 'if(player(a)||ordinary(a))track_grudge(v,a);' in c
    assert 'track_grudge(other,actor)' not in c
    assert 'FHDNPC_GrudgePlan(' in c
    assert 'g_offers[i].paid=1u; /* idempotent before money changes */' in c
    assert 'FHDNPC_InvokeSetMoney' in c

def test_native_tick_and_safety_contract():
    v=source('fhd_vmt_probe.c')
    assert 'FHDNPC_GrievanceTick(ship,category);' in v
    c=source('fhd_grievance.c')
    assert 'if(!g_armed||kind>3u||!ordinary(p))return;' in c
    assert 'if(!merc&&plan==2u)continue;' in c
    assert 'if(!issuer||issuer==k||!ordinary(issuer))continue;' in c
    assert 'FHDNPC_LocalDialogue(FHD_DLG_CONTRACT,p,target,0u,day,amount);' in c
