"""Regression: TPlayer is a derived TRanger, not a matching TRanger VMT."""
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]
G=(BASE/'src/fhd_grievance.c').read_text(encoding='utf-8')
F=(BASE/'tests/native_grievance_x86.c').read_text(encoding='utf-8')
N=(BASE/'src/fhd_news.c').read_text(encoding='utf-8')

def test_player_identity_uses_game_getplayer_encoding_not_ranger_vmt():
    assert '#define ENCODED_PLAYER_RVA 0x4100fcu' in G
    assert '#define PLAYER_XOR_KEY 0xb1cd15d3u' in G
    player=G.split('static int player(',1)[1].split('static int ordinary(',1)[0]
    assert 'read32((const void*)g_base,ENCODED_PLAYER_RVA)^PLAYER_XOR_KEY' in player
    assert '(uint32_t)(uintptr_t)p==live' in player
    assert 'read32(p,0u)==(uint32_t)(g_base+PLAYER_VMT_PTR)' not in player

def test_native_fixture_uses_distinct_derived_player_vmt():
    assert 'layout_ship(pilot,1,20,PLAYER_VMT_PTR+0x100u);' in F
    assert 'put(game,ENCODED_PLAYER_RVA' in F
    assert 'CHECK(player(pilot)&&!player(pirate)&&!ordinary(pilot))' in F

def test_player_incidents_feed_relation_reaction_and_info():
    assert 'if(!is_player)continue; /* NPC-on-NPC loot stays out of player radio. */' in G
    assert 'next_memory=FHDNPC_LootNextGradeMemoryMin(native,prior,civilian?10u:0u);' in G
    assert 'FHDNPC_LocalDialogue(line,other,f->actor_id,1u,f->day,f->item_id);' in G
    assert 'queue_claim(other,actor,f->item_id,f->day,f->kind' in G
    assert 'FHDNPC_InvokeNews((void *)(image+NEWS_RVA),INFO_KIND,day,info)' in N

def test_native_claim_not_lost_on_game_cooldown_or_screen_gate():
    claim=G.split('static void process_claim(',1)[1].split('void FHDNPC_GrievanceTick(',1)[0]
    assert 'EXTORT_COOLDOWN_RVA)<7u' in claim
    assert 'CURRENT_SCREEN_RVA)!=16u' not in claim
    assert 'DIALOGUE_COUNT_RVA' in claim
    assert 'TryExtortShip has its own contact' in claim or 'original TShip.TryExtortShip has its own contact' in claim
    assert 'if(ok||after>before)g_claims[i].active=0;' in claim
    assert 'else g_claim_results[3]++;' in claim
    assert 'FHDNPC_InvokeExtort' in claim
